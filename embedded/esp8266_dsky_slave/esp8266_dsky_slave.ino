#include "dsky_protocol.h"
#include "web_dsky_page.h"

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#include <string.h>

#if defined(__has_include)
#if __has_include("wifi_config.h")
#include "wifi_config.h"
#endif
#endif

#ifndef DSKY_WIFI_SSID
#define DSKY_WIFI_SSID ""
#endif

#ifndef DSKY_WIFI_PASSWORD
#define DSKY_WIFI_PASSWORD ""
#endif

#ifndef DSKY_AP_SSID
#define DSKY_AP_SSID "AGC-DSKY"
#endif

#ifndef DSKY_AP_PASSWORD
#define DSKY_AP_PASSWORD "apollo11"
#endif

namespace {

constexpr uint32_t kUsbBaud = 115200;
constexpr uint32_t kCoreBaud = 38400;
constexpr uint8_t kCoreRxPin = 14;  // NodeMCU D5: connect to ESP32 TX2/GPIO17.
constexpr uint8_t kCoreTxPin = 12;  // NodeMCU D6: connect to ESP32 RX2/GPIO16.
constexpr unsigned long kStatusPeriodMs = 1000;
constexpr unsigned long kBlinkPeriodMs = 500;
constexpr unsigned long kLinkTimeoutMs = 5000;
constexpr unsigned long kWifiConnectTimeoutMs = 12000;
constexpr const char* kHostname = "agc-dsky";

SoftwareSerial coreSerial(kCoreRxPin, kCoreTxPin);
ESP8266WebServer webServer(80);
dsky::State state;
dsky::Phase phase;

char coreLineBuffer[dsky::kLineBufferSize];
size_t coreLineLength = 0;

char usbLineBuffer[dsky::kLineBufferSize];
size_t usbLineLength = 0;

unsigned long lastStateRxMs = 0;
unsigned long lastStatusMs = 0;
unsigned long lastKeyTxMs = 0;

bool ledActiveLow = true;
bool wifiApMode = false;
bool mdnsActive = false;
char lastKeyName[12] = "NONE";

void setStatusLed(bool on) {
#if defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, ledActiveLow ? !on : on);
#endif
}

bool linkUp() {
  return millis() - lastStateRxMs < kLinkTimeoutMs;
}

void emitDskyStatus(Stream& port) {
  port.print(F("DSKY "));
  port.print(linkUp() ? F("LINK=1") : F("LINK=0"));
  port.print(F(" P="));
  port.print(state.program);
  port.print(F(" V="));
  port.print(state.verb);
  port.print(F(" N="));
  port.print(state.noun);
  port.print(F(" R1="));
  port.print(state.r1);
  port.print(F(" R2="));
  port.print(state.r2);
  port.print(F(" R3="));
  port.print(state.r3);
  port.print(F(" ALARM="));
  port.print(state.alarm);
  port.print(F(" FLASH="));
  port.print(state.flashVerbNoun ? 1 : 0);
  port.print(F(" LAMPS="));
  port.print(state.lampMask, HEX);
  port.print(F(" PHASE="));
  port.println(phase.label);
}

void sendKey(dsky::Key key) {
  char line[dsky::kLineBufferSize];
  if (!dsky::formatKeyLine(key, line, sizeof(line))) {
    return;
  }

  coreSerial.println(line);
  dsky::copyField(lastKeyName, sizeof(lastKeyName), dsky::keyName(key));
  lastKeyTxMs = millis();
  Serial.print(F("TX->ESP32 "));
  Serial.println(line);
}

bool sendShortcutKey(char* line) {
  if (line == nullptr || line[0] == '\0' || line[1] != '\0') {
    return false;
  }

  if (line[0] >= '0' && line[0] <= '9') {
    sendKey(static_cast<dsky::Key>(
        static_cast<uint8_t>(dsky::Key::Digit0) + (line[0] - '0')));
    return true;
  }

  if (line[0] == 'V') {
    sendKey(dsky::Key::Verb);
    return true;
  }

  if (line[0] == 'N') {
    sendKey(dsky::Key::Noun);
    return true;
  }

  if (line[0] == 'E') {
    sendKey(dsky::Key::Enter);
    return true;
  }

  if (line[0] == 'C') {
    sendKey(dsky::Key::Clear);
    return true;
  }

  if (line[0] == 'P') {
    sendKey(dsky::Key::Proceed);
    return true;
  }

  if (line[0] == 'R') {
    sendKey(dsky::Key::Reset);
    return true;
  }

  return false;
}

String webIpText() {
  return wifiApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

String webWifiName() {
  return wifiApMode ? String(DSKY_AP_SSID) : WiFi.SSID();
}

void handleRoot() {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send_P(200, PSTR("text/html; charset=utf-8"), kDskyIndexHtml);
}

void handleApiState() {
  char payload[720];
  const String ip = webIpText();
  const String wifi = webWifiName();
  const char* mode = wifiApMode ? "AP" : "STA";

  snprintf(payload, sizeof(payload),
           "{\"link\":%s,\"program\":%u,\"verb\":%u,\"noun\":%u,"
           "\"r1\":\"%s\",\"r2\":\"%s\",\"r3\":\"%s\",\"alarm\":%u,"
           "\"flash\":%s,\"lamps\":%lu,\"missionSeconds\":%lu,"
           "\"uptimeMs\":%lu,\"lastKey\":\"%s\",\"lastKeyAgeMs\":%lu,"
           "\"mode\":\"%s\",\"ip\":\"%s\",\"wifi\":\"%s\","
           "\"phase\":\"%s\",\"r1Label\":\"%s\",\"r2Label\":\"%s\","
           "\"r3Label\":\"%s\"}",
           linkUp() ? "true" : "false",
           static_cast<unsigned int>(state.program),
           static_cast<unsigned int>(state.verb),
           static_cast<unsigned int>(state.noun), state.r1, state.r2,
           state.r3, static_cast<unsigned int>(state.alarm),
           state.flashVerbNoun ? "true" : "false",
           static_cast<unsigned long>(state.lampMask),
           static_cast<unsigned long>(state.missionSeconds),
           static_cast<unsigned long>(millis()), lastKeyName,
           static_cast<unsigned long>(millis() - lastKeyTxMs), mode,
           ip.c_str(), wifi.c_str(), phase.label, phase.r1Label,
           phase.r2Label, phase.r3Label);

  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", payload);
}

bool keyFromWebArgument(dsky::Key* key) {
  if (key == nullptr || !webServer.hasArg("name")) {
    return false;
  }

  String name = webServer.arg("name");
  name.trim();
  name.toUpperCase();

  if (name == F("ENTER")) {
    name = F("ENTR");
  } else if (name == F("CLEAR")) {
    name = F("CLR");
  } else if (name == F("PROCEED")) {
    name = F("PRO");
  } else if (name == F("RESET")) {
    name = F("RSET");
  } else if (name == F("KEY REL") || name == F("KEY_RELEASE")) {
    name = F("KEYREL");
  } else if (name == F("+")) {
    name = F("PLUS");
  } else if (name == F("-")) {
    name = F("MINUS");
  }

  char keyName[16];
  name.toCharArray(keyName, sizeof(keyName));
  *key = dsky::parseKeyName(keyName);
  return *key != dsky::Key::None;
}

void handleApiKey() {
  dsky::Key key = dsky::Key::None;
  if (!keyFromWebArgument(&key)) {
    webServer.send(400, "text/plain", "Missing or invalid key name");
    return;
  }

  sendKey(key);
  webServer.send(200, "text/plain", "OK");
}

void handleApiRaw() {
  String line;
  if (webServer.hasArg("line")) {
    line = webServer.arg("line");
  } else {
    line = webServer.arg("plain");
  }

  line.trim();
  if (line.length() == 0 || line.length() >= dsky::kLineBufferSize) {
    webServer.send(400, "text/plain", "Missing or too-long raw command");
    return;
  }

  coreSerial.println(line);
  Serial.print(F("WEB->ESP32 "));
  Serial.println(line);
  webServer.send(200, "text/plain", "OK");
}

void handleNotFound() {
  webServer.send(404, "text/plain", "Not found");
}

void setupWebServer() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/api/state", HTTP_GET, handleApiState);
  webServer.on("/api/key", HTTP_GET, handleApiKey);
  webServer.on("/api/key", HTTP_POST, handleApiKey);
  webServer.on("/api/raw", HTTP_GET, handleApiRaw);
  webServer.on("/api/raw", HTTP_POST, handleApiRaw);
  webServer.on("/favicon.ico", HTTP_GET,
               []() { webServer.send(204, "text/plain", ""); });
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

void setupWiFi() {
  WiFi.persistent(false);
  WiFi.hostname(kHostname);

  if (strlen(DSKY_WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(DSKY_WIFI_SSID, DSKY_WIFI_PASSWORD);

    Serial.print(F("Connecting to Wi-Fi SSID "));
    Serial.println(DSKY_WIFI_SSID);

    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startMs < kWifiConnectTimeoutMs) {
      setStatusLed((millis() / 150UL) % 2UL == 0);
      delay(50);
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiApMode = false;
      Serial.print(F("Web DSKY ready at http://"));
      Serial.println(WiFi.localIP());

      if (MDNS.begin(kHostname)) {
        mdnsActive = true;
        Serial.print(F("mDNS ready at http://"));
        Serial.print(kHostname);
        Serial.println(F(".local"));
      }

      return;
    }

    Serial.println(F("Wi-Fi station failed. Starting fallback AP."));
  }

  wifiApMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DSKY_AP_SSID, DSKY_AP_PASSWORD);
  Serial.print(F("Fallback AP SSID: "));
  Serial.println(DSKY_AP_SSID);
  Serial.print(F("Fallback AP password: "));
  Serial.println(DSKY_AP_PASSWORD);
  Serial.print(F("Web DSKY ready at http://"));
  Serial.println(WiFi.softAPIP());
}

void handleCoreLine(char* line) {
  dsky::State parsed;
  dsky::initState(&parsed);

  if (dsky::parseStateLine(line, &parsed)) {
    state = parsed;
    lastStateRxMs = millis();
    emitDskyStatus(Serial);
    return;
  }

  dsky::Phase parsedPhase;
  dsky::initPhase(&parsedPhase);
  if (dsky::parsePhaseLine(line, &parsedPhase)) {
    phase = parsedPhase;
    emitDskyStatus(Serial);
  }
}

void handleUsbLine(char* line) {
  if (strcmp(line, "HELP") == 0) {
    Serial.println(F("Commands: KEY,<name> or shortcuts V N 0..9 E C P R"));
    Serial.println(F("Examples: V then 3 then 7, N then 3 then 6"));
    Serial.print(F("Web DSKY: http://"));
    Serial.println(webIpText());
    return;
  }

  if (strcmp(line, "STATE") == 0) {
    emitDskyStatus(Serial);
    return;
  }

  if (sendShortcutKey(line)) {
    return;
  }

  char scratch[dsky::kLineBufferSize];
  dsky::copyField(scratch, sizeof(scratch), line);

  dsky::Key key = dsky::Key::None;
  if (dsky::parseKeyLine(scratch, &key)) {
    sendKey(key);
    return;
  }

  coreSerial.println(line);
  Serial.print(F("USB->ESP32 "));
  Serial.println(line);
}

void pollCoreSerial() {
  while (coreSerial.available() > 0) {
    const char incoming = static_cast<char>(coreSerial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      coreLineBuffer[coreLineLength] = '\0';
      handleCoreLine(coreLineBuffer);
      coreLineLength = 0;
      continue;
    }

    if (coreLineLength + 1 < sizeof(coreLineBuffer)) {
      coreLineBuffer[coreLineLength++] = incoming;
    }
  }
}

void pollUsbSerial() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      usbLineBuffer[usbLineLength] = '\0';
      handleUsbLine(usbLineBuffer);
      usbLineLength = 0;
      continue;
    }

    if (usbLineLength + 1 < sizeof(usbLineBuffer)) {
      usbLineBuffer[usbLineLength++] = incoming;
    }
  }
}

void updateStatusLed() {
  if (!linkUp()) {
    setStatusLed((millis() / kBlinkPeriodMs) % 2UL == 0);
    return;
  }

  const bool alarm = state.alarm != 0;
  const bool flash = state.flashVerbNoun && ((millis() / kBlinkPeriodMs) % 2UL == 0);
  setStatusLed(alarm || flash);
}

}  // namespace

void setup() {
  dsky::initState(&state);
  dsky::initPhase(&phase);

#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  setStatusLed(false);
#endif

  Serial.begin(kUsbBaud);
  coreSerial.begin(kCoreBaud);

  delay(250);
  Serial.println(F("ESP8266 DSKY slave ready."));
  setupWiFi();
  setupWebServer();
  Serial.println(F("Type HELP for key commands."));
}

void loop() {
  webServer.handleClient();
  if (mdnsActive) {
    MDNS.update();
  }

  pollCoreSerial();
  pollUsbSerial();
  updateStatusLed();

  const unsigned long now = millis();
  if (now - lastStatusMs >= kStatusPeriodMs) {
    lastStatusMs = now;
    emitDskyStatus(Serial);
  }
}
