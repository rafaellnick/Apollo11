#include <LiquidCrystal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint8_t kLcdRsPin = 12;
constexpr uint8_t kLcdEnPin = 11;
constexpr uint8_t kLcdD4Pin = 5;
constexpr uint8_t kLcdD5Pin = 4;
constexpr uint8_t kLcdD6Pin = 3;
constexpr uint8_t kLcdD7Pin = 2;

constexpr unsigned long kLcdRefreshMs = 250;
constexpr unsigned long kPagePeriodMs = 2000;
constexpr unsigned long kTelemetryPeriodMs = 1000;
constexpr unsigned long kDemoStepMs = 1000;

constexpr size_t kLineWidth = 16;
constexpr size_t kRegisterFieldSize = 8;
constexpr size_t kInputBufferSize = 48;
constexpr uint8_t kPageCount = 5;

LiquidCrystal lcd(
    kLcdRsPin, kLcdEnPin, kLcdD4Pin, kLcdD5Pin, kLcdD6Pin, kLcdD7Pin);

struct AgcState {
  uint8_t majorMode;
  uint8_t verb;
  uint8_t noun;
  uint16_t alarm;
  uint32_t missionSeconds;
  bool flashVerbNoun;
  bool keyRelease;
  char r1[kRegisterFieldSize];
  char r2[kRegisterFieldSize];
  char r3[kRegisterFieldSize];
};

AgcState state = {
    0,
    16,
    36,
    0,
    0,
    false,
    false,
    "+00000",
    "+00000",
    "+00000",
};

char inputBuffer[kInputBufferSize];
uint8_t inputLength = 0;

uint8_t currentPage = 0;
bool autoRotatePages = true;
bool demoMode = true;

unsigned long lastLcdRefreshMs = 0;
unsigned long lastPageAdvanceMs = 0;
unsigned long lastTelemetryMs = 0;
unsigned long lastDemoStepMs = 0;

void copyField(char* destination, size_t destinationSize, const char* source) {
  if (destinationSize == 0) {
    return;
  }

  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

uint8_t clampTwoDigit(int value) {
  if (value < 0) {
    return 0;
  }

  if (value > 99) {
    return 99;
  }

  return static_cast<uint8_t>(value);
}

uint16_t clampAlarm(int value) {
  if (value < 0) {
    return 0;
  }

  if (value > 9999) {
    return 9999;
  }

  return static_cast<uint16_t>(value);
}

void formatMissionClock(char* buffer, size_t bufferSize, uint32_t totalSeconds) {
  const unsigned long hours = (totalSeconds / 3600UL) % 100UL;
  const uint8_t minutes = static_cast<uint8_t>((totalSeconds / 60UL) % 60UL);
  const uint8_t seconds = static_cast<uint8_t>(totalSeconds % 60UL);

  snprintf(buffer, bufferSize, "T+%02lu:%02u:%02u", hours, minutes, seconds);
}

void formatSignedRegister(char* buffer, size_t bufferSize, long value) {
  const char sign = value < 0 ? '-' : '+';
  const unsigned long magnitude = static_cast<unsigned long>(labs(value) % 100000L);
  snprintf(buffer, bufferSize, "%c%05lu", sign, magnitude);
}

void writePaddedLine(uint8_t row, const char* text) {
  char padded[kLineWidth + 1];
  memset(padded, ' ', kLineWidth);
  padded[kLineWidth] = '\0';

  const size_t textLength = strlen(text);
  const size_t copyLength = textLength < kLineWidth ? textLength : kLineWidth;
  memcpy(padded, text, copyLength);

  lcd.setCursor(0, row);
  lcd.print(padded);
}

void printTwoDigits(uint8_t value) {
  if (value < 10) {
    Serial.print('0');
  }
  Serial.print(value);
}

void emitHelp() {
  Serial.println(F("Commands: P=nn V=nn N=nn A=nnnn T=seconds"));
  Serial.println(F("          R1=text R2=text R3=text"));
  Serial.println(F("          FLASH=0|1 KEYREL=0|1"));
  Serial.println(F("          AUTO=0|1 PAGE=0..4 DEMO=0|1"));
  Serial.println(F("          DUMP HELP"));
}

void emitSerialTelemetry() {
  char missionClock[12];
  formatMissionClock(missionClock, sizeof(missionClock), state.missionSeconds);

  Serial.print(F("STATE "));
  Serial.print(F("P="));
  printTwoDigits(state.majorMode);
  Serial.print(F(" V="));
  printTwoDigits(state.verb);
  Serial.print(F(" N="));
  printTwoDigits(state.noun);
  Serial.print(F(" A="));
  Serial.print(state.alarm);
  Serial.print(F(" CLOCK="));
  Serial.print(missionClock);
  Serial.print(F(" R1="));
  Serial.print(state.r1);
  Serial.print(F(" R2="));
  Serial.print(state.r2);
  Serial.print(F(" R3="));
  Serial.print(state.r3);
  Serial.print(F(" FLASH="));
  Serial.print(state.flashVerbNoun ? 1 : 0);
  Serial.print(F(" KEYREL="));
  Serial.print(state.keyRelease ? 1 : 0);
  Serial.print(F(" PAGE="));
  Serial.print(currentPage);
  Serial.print(F(" AUTO="));
  Serial.print(autoRotatePages ? 1 : 0);
  Serial.print(F(" DEMO="));
  Serial.println(demoMode ? 1 : 0);
}

void renderLcd() {
  char line1[kLineWidth + 1];
  char line2[kLineWidth + 1];
  char missionClock[12];

  const bool blinkOn = state.flashVerbNoun && ((millis() / 500UL) % 2UL == 1UL);

  if (blinkOn) {
    snprintf(line1, sizeof(line1), "P%02u V-- N--", state.majorMode);
  } else {
    snprintf(line1, sizeof(line1), "P%02u V%02u N%02u", state.majorMode, state.verb,
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
    case 3:
      snprintf(line2, sizeof(line2), "R3 %s", state.r3);
      break;
    default:
      snprintf(line2, sizeof(line2), "FL %u KR %u AUTO %u",
               state.flashVerbNoun ? 1 : 0, state.keyRelease ? 1 : 0,
               autoRotatePages ? 1 : 0);
      break;
  }

  writePaddedLine(0, line1);
  writePaddedLine(1, line2);
}

void updateDemoState() {
  const unsigned long now = millis();
  if (now - lastDemoStepMs < kDemoStepMs) {
    return;
  }

  lastDemoStepMs = now;
  state.missionSeconds++;

  const long signedBase = static_cast<long>((state.missionSeconds * 37UL) % 99999UL);
  formatSignedRegister(state.r1, sizeof(state.r1), signedBase);
  formatSignedRegister(state.r2, sizeof(state.r2), -signedBase / 2L);
  formatSignedRegister(state.r3, sizeof(state.r3), signedBase / 3L);

  if (state.missionSeconds % 8UL == 0UL) {
    state.flashVerbNoun = !state.flashVerbNoun;
  }

  if (state.missionSeconds % 11UL == 0UL) {
    state.keyRelease = !state.keyRelease;
  }

  if (state.flashVerbNoun) {
    state.alarm = 1202;
  } else {
    state.alarm = 0;
  }

  if (state.missionSeconds % 15UL == 0UL) {
    state.verb = static_cast<uint8_t>((state.verb + 1U) % 100U);
  }

  if (state.missionSeconds % 20UL == 0UL) {
    state.noun = static_cast<uint8_t>((state.noun + 1U) % 100U);
  }

  if (state.missionSeconds % 30UL == 0UL) {
    state.majorMode = static_cast<uint8_t>((state.majorMode + 1U) % 100U);
  }
}

void handleCommand(char* line) {
  if (line[0] == '\0') {
    return;
  }

  if (strcmp(line, "HELP") == 0) {
    emitHelp();
    return;
  }

  if (strcmp(line, "DUMP") == 0) {
    emitSerialTelemetry();
    return;
  }

  if (strncmp(line, "P=", 2) == 0) {
    state.majorMode = clampTwoDigit(atoi(line + 2));
    demoMode = false;
    return;
  }

  if (strncmp(line, "V=", 2) == 0) {
    state.verb = clampTwoDigit(atoi(line + 2));
    demoMode = false;
    return;
  }

  if (strncmp(line, "N=", 2) == 0) {
    state.noun = clampTwoDigit(atoi(line + 2));
    demoMode = false;
    return;
  }

  if (strncmp(line, "A=", 2) == 0) {
    state.alarm = clampAlarm(atoi(line + 2));
    demoMode = false;
    return;
  }

  if (strncmp(line, "T=", 2) == 0) {
    const long value = atol(line + 2);
    state.missionSeconds = value < 0 ? 0 : static_cast<uint32_t>(value);
    demoMode = false;
    return;
  }

  if (strncmp(line, "TIME=", 5) == 0) {
    const long value = atol(line + 5);
    state.missionSeconds = value < 0 ? 0 : static_cast<uint32_t>(value);
    demoMode = false;
    return;
  }

  if (strncmp(line, "R1=", 3) == 0) {
    copyField(state.r1, sizeof(state.r1), line + 3);
    demoMode = false;
    return;
  }

  if (strncmp(line, "R2=", 3) == 0) {
    copyField(state.r2, sizeof(state.r2), line + 3);
    demoMode = false;
    return;
  }

  if (strncmp(line, "R3=", 3) == 0) {
    copyField(state.r3, sizeof(state.r3), line + 3);
    demoMode = false;
    return;
  }

  if (strncmp(line, "FLASH=", 6) == 0) {
    state.flashVerbNoun = atoi(line + 6) != 0;
    demoMode = false;
    return;
  }

  if (strncmp(line, "KEYREL=", 7) == 0) {
    state.keyRelease = atoi(line + 7) != 0;
    demoMode = false;
    return;
  }

  if (strncmp(line, "AUTO=", 5) == 0) {
    autoRotatePages = atoi(line + 5) != 0;
    return;
  }

  if (strncmp(line, "PAGE=", 5) == 0) {
    const int requestedPage = atoi(line + 5);
    if (requestedPage >= 0 && requestedPage < kPageCount) {
      currentPage = static_cast<uint8_t>(requestedPage);
      autoRotatePages = false;
    }
    return;
  }

  if (strncmp(line, "DEMO=", 5) == 0) {
    demoMode = atoi(line + 5) != 0;
    return;
  }

  Serial.print(F("Unknown command: "));
  Serial.println(line);
}

void processSerialInput() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      inputBuffer[inputLength] = '\0';
      handleCommand(inputBuffer);
      inputLength = 0;
      continue;
    }

    if (inputLength + 1 < kInputBufferSize) {
      inputBuffer[inputLength++] = incoming;
    }
  }
}

}  // namespace

void setup() {
  lcd.begin(16, 2);
  lcd.clear();
  writePaddedLine(0, "AGC UNO FRONTEND");
  writePaddedLine(1, "Serial: HELP");

  Serial.begin(115200);
  delay(250);
  emitHelp();
  emitSerialTelemetry();
}

void loop() {
  processSerialInput();

  if (demoMode) {
    updateDemoState();
  }

  const unsigned long now = millis();

  if (autoRotatePages && now - lastPageAdvanceMs >= kPagePeriodMs) {
    lastPageAdvanceMs = now;
    currentPage = static_cast<uint8_t>((currentPage + 1U) % kPageCount);
  }

  if (now - lastLcdRefreshMs >= kLcdRefreshMs) {
    lastLcdRefreshMs = now;
    renderLcd();
  }

  if (now - lastTelemetryMs >= kTelemetryPeriodMs) {
    lastTelemetryMs = now;
    emitSerialTelemetry();
  }
}
