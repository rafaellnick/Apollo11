#include "dsky_protocol.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <string.h>

namespace {

constexpr uint32_t kUsbBaud = 115200;
constexpr uint32_t kCoreBaud = 38400;
constexpr uint8_t kCoreRxPin = 14;  // NodeMCU D5: connect to ESP32 TX2/GPIO17.
constexpr uint8_t kCoreTxPin = 12;  // NodeMCU D6: connect to ESP32 RX2/GPIO16.
constexpr unsigned long kStatusPeriodMs = 1000;
constexpr unsigned long kBlinkPeriodMs = 500;
constexpr unsigned long kLinkTimeoutMs = 5000;

SoftwareSerial coreSerial(kCoreRxPin, kCoreTxPin);
dsky::State state;

char coreLineBuffer[dsky::kLineBufferSize];
size_t coreLineLength = 0;

char usbLineBuffer[dsky::kLineBufferSize];
size_t usbLineLength = 0;

unsigned long lastStateRxMs = 0;
unsigned long lastStatusMs = 0;

bool ledActiveLow = true;

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
  port.println(state.lampMask, HEX);
}

void sendKey(dsky::Key key) {
  char line[dsky::kLineBufferSize];
  if (!dsky::formatKeyLine(key, line, sizeof(line))) {
    return;
  }

  coreSerial.println(line);
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

void handleCoreLine(char* line) {
  dsky::State parsed;
  dsky::initState(&parsed);

  if (dsky::parseStateLine(line, &parsed)) {
    state = parsed;
    lastStateRxMs = millis();
    emitDskyStatus(Serial);
  }
}

void handleUsbLine(char* line) {
  if (strcmp(line, "HELP") == 0) {
    Serial.println(F("Commands: KEY,<name> or shortcuts V N 0..9 E C P R"));
    Serial.println(F("Examples: V then 3 then 7, N then 3 then 6"));
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

#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  setStatusLed(false);
#endif

  Serial.begin(kUsbBaud);
  coreSerial.begin(kCoreBaud);

  delay(250);
  Serial.println(F("ESP8266 DSKY slave ready."));
  Serial.println(F("Type HELP for key commands."));
}

void loop() {
  pollCoreSerial();
  pollUsbSerial();
  updateStatusLed();

  const unsigned long now = millis();
  if (now - lastStatusMs >= kStatusPeriodMs) {
    lastStatusMs = now;
    emitDskyStatus(Serial);
  }
}
