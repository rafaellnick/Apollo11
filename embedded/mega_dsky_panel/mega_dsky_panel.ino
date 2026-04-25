#include "dsky_protocol.h"

#include <Arduino.h>
#include <LiquidCrystal.h>

namespace {

constexpr uint32_t kUsbBaud = 115200;
constexpr uint32_t kCoreBaud = 38400;

constexpr uint8_t kLcdRsPin = 12;
constexpr uint8_t kLcdEnPin = 11;
constexpr uint8_t kLcdD4Pin = 5;
constexpr uint8_t kLcdD5Pin = 4;
constexpr uint8_t kLcdD6Pin = 3;
constexpr uint8_t kLcdD7Pin = 2;

constexpr unsigned long kDebounceMs = 30;
constexpr unsigned long kPagePeriodMs = 2000;
constexpr unsigned long kLcdRefreshMs = 200;
constexpr unsigned long kLinkTimeoutMs = 5000;

constexpr size_t kDetailPageCount = 4;

LiquidCrystal lcd(
    kLcdRsPin, kLcdEnPin, kLcdD4Pin, kLcdD5Pin, kLcdD6Pin, kLcdD7Pin);

struct ButtonBinding {
  dsky::Key key;
  uint8_t pin;
  bool stablePressed;
  bool lastRawPressed;
  unsigned long lastChangeMs;
};

struct LampBinding {
  uint8_t pin;
  uint32_t mask;
};

ButtonBinding buttons[] = {
    {dsky::Key::Digit0, 22, false, false, 0},
    {dsky::Key::Digit1, 23, false, false, 0},
    {dsky::Key::Digit2, 24, false, false, 0},
    {dsky::Key::Digit3, 25, false, false, 0},
    {dsky::Key::Digit4, 26, false, false, 0},
    {dsky::Key::Digit5, 27, false, false, 0},
    {dsky::Key::Digit6, 28, false, false, 0},
    {dsky::Key::Digit7, 29, false, false, 0},
    {dsky::Key::Digit8, 30, false, false, 0},
    {dsky::Key::Digit9, 31, false, false, 0},
    {dsky::Key::Verb, 32, false, false, 0},
    {dsky::Key::Noun, 33, false, false, 0},
    {dsky::Key::Clear, 34, false, false, 0},
    {dsky::Key::Proceed, 35, false, false, 0},
    {dsky::Key::KeyRelease, 36, false, false, 0},
    {dsky::Key::Enter, 37, false, false, 0},
    {dsky::Key::Reset, 38, false, false, 0},
    {dsky::Key::Plus, 39, false, false, 0},
    {dsky::Key::Minus, 40, false, false, 0},
};

LampBinding lamps[] = {
    {41, dsky::kLampCompActy},
    {42, dsky::kLampUplinkActy},
    {43, dsky::kLampTemp},
    {44, dsky::kLampGimbalLock},
    {45, dsky::kLampProg},
    {46, dsky::kLampKeyRel},
    {47, dsky::kLampOprErr},
    {48, dsky::kLampStby},
    {49, dsky::kLampNoAtt},
    {50, dsky::kLampTracker},
};

dsky::State state;
char coreLineBuffer[dsky::kLineBufferSize];
size_t coreLineLength = 0;

char usbLineBuffer[dsky::kLineBufferSize];
size_t usbLineLength = 0;

unsigned long lastStateRxMs = 0;
unsigned long lastPageAdvanceMs = 0;
unsigned long lastLcdRefreshMs = 0;
uint8_t currentPage = 0;

void writePaddedLine(uint8_t row, const char* text) {
  char padded[17];
  memset(padded, ' ', 16);
  padded[16] = '\0';

  const size_t textLength = strlen(text);
  const size_t copyLength = textLength < 16 ? textLength : 16;
  memcpy(padded, text, copyLength);

  lcd.setCursor(0, row);
  lcd.print(padded);
}

void updateLampOutputs() {
  for (size_t i = 0; i < sizeof(lamps) / sizeof(lamps[0]); ++i) {
    digitalWrite(lamps[i].pin,
                 (state.lampMask & lamps[i].mask) != 0 ? HIGH : LOW);
  }
}

void sendKey(dsky::Key key) {
  char line[dsky::kLineBufferSize];
  if (!dsky::formatKeyLine(key, line, sizeof(line))) {
    return;
  }

  Serial1.println(line);
  Serial.print(F("TX->ESP32 "));
  Serial.println(line);
}

void formatMissionClock(char* buffer, size_t bufferSize, uint32_t totalSeconds) {
  const unsigned long hours = (totalSeconds / 3600UL) % 100UL;
  const uint8_t minutes = static_cast<uint8_t>((totalSeconds / 60UL) % 60UL);
  const uint8_t seconds = static_cast<uint8_t>(totalSeconds % 60UL);
  snprintf(buffer, bufferSize, "T+%02lu:%02u:%02u", hours, minutes, seconds);
}

void renderLcd() {
  const unsigned long now = millis();
  const bool linkUp = (now - lastStateRxMs) < kLinkTimeoutMs;

  if (!linkUp) {
    writePaddedLine(0, "MEGA DSKY PANEL");
    writePaddedLine(1, "Waiting for AGC");
    return;
  }

  char line1[17];
  char line2[17];
  char missionClock[12];

  const bool blinkOn = state.flashVerbNoun && ((millis() / 500UL) % 2UL == 1UL);

  if (blinkOn) {
    snprintf(line1, sizeof(line1), "P%02u V-- N--", state.program);
  } else {
    snprintf(line1, sizeof(line1), "P%02u V%02u N%02u", state.program, state.verb,
             state.noun);
  }

  formatMissionClock(missionClock, sizeof(missionClock), state.missionSeconds);

  switch (currentPage) {
    case 0:
      snprintf(line2, sizeof(line2), "A%04u %s", state.alarm, missionClock);
      break;
    case 1:
      snprintf(line2, sizeof(line2), "R1 %s", state.r1);
      break;
    case 2:
      snprintf(line2, sizeof(line2), "R2 %s", state.r2);
      break;
    default:
      snprintf(line2, sizeof(line2), "R3 %s", state.r3);
      break;
  }

  writePaddedLine(0, line1);
  writePaddedLine(1, line2);
}

void handleCoreLine(char* line) {
  dsky::State parsed;
  dsky::initState(&parsed);

  if (dsky::parseStateLine(line, &parsed)) {
    state = parsed;
    lastStateRxMs = millis();
    updateLampOutputs();

    char echo[dsky::kLineBufferSize];
    if (dsky::formatStateLine(state, echo, sizeof(echo))) {
      Serial.print(F("RX<-ESP32 "));
      Serial.println(echo);
    }
  }
}

void handleUsbLine(char* line) {
  char scratch[dsky::kLineBufferSize];
  dsky::copyField(scratch, sizeof(scratch), line);

  dsky::Key key = dsky::Key::None;
  if (dsky::parseKeyLine(scratch, &key)) {
    sendKey(key);
    return;
  }

  Serial1.println(line);
  Serial.print(F("USB->ESP32 "));
  Serial.println(line);
}

void pollCoreSerial() {
  while (Serial1.available() > 0) {
    const char incoming = static_cast<char>(Serial1.read());

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

void scanButtons() {
  const unsigned long now = millis();

  for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
    const bool rawPressed = digitalRead(buttons[i].pin) == LOW;

    if (rawPressed != buttons[i].lastRawPressed) {
      buttons[i].lastRawPressed = rawPressed;
      buttons[i].lastChangeMs = now;
    }

    if (now - buttons[i].lastChangeMs < kDebounceMs) {
      continue;
    }

    if (rawPressed != buttons[i].stablePressed) {
      buttons[i].stablePressed = rawPressed;

      if (buttons[i].stablePressed) {
        sendKey(buttons[i].key);
      }
    }
  }
}

}  // namespace

void setup() {
  dsky::initState(&state);

  lcd.begin(16, 2);
  lcd.clear();
  writePaddedLine(0, "MEGA DSKY PANEL");
  writePaddedLine(1, "Waiting for AGC");

  Serial.begin(kUsbBaud);
  Serial1.begin(kCoreBaud);

  for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }

  for (size_t i = 0; i < sizeof(lamps) / sizeof(lamps[0]); ++i) {
    pinMode(lamps[i].pin, OUTPUT);
    digitalWrite(lamps[i].pin, LOW);
  }

  delay(250);
  Serial.println(F("Mega DSKY panel ready."));
  Serial.println(F("Type KEY,VERB or other protocol lines over USB for testing."));
}

void loop() {
  pollCoreSerial();
  pollUsbSerial();
  scanButtons();

  const unsigned long now = millis();

  if (now - lastPageAdvanceMs >= kPagePeriodMs) {
    lastPageAdvanceMs = now;
    currentPage = static_cast<uint8_t>((currentPage + 1U) % kDetailPageCount);
  }

  if (now - lastLcdRefreshMs >= kLcdRefreshMs) {
    lastLcdRefreshMs = now;
    renderLcd();
  }
}
