#include "../shared/agc_core.h"
#include "../shared/dsky_protocol.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint32_t kUsbBaud = 115200;
constexpr uint32_t kPanelBaud = 38400;
constexpr int kPanelRxPin = 16;
constexpr int kPanelTxPin = 17;
constexpr int kJoystickXPin = 34;
constexpr int kJoystickYPin = 35;
constexpr int kJoystickSwitchPin = 27;
constexpr unsigned long kTelemetryPeriodMs = 250;
constexpr unsigned long kMonitorPeriodMs = 1000;
constexpr unsigned long kJoystickSamplePeriodMs = 50;
constexpr unsigned long kCompActyPulseMs = 150;
constexpr uint16_t kInstructionsPerLoop = 64;
constexpr int kAdcMax = 4095;
constexpr int kJoystickDeadzone = 80;
constexpr int kJoystickOutputMax = 1000;
constexpr uint8_t kJoystickCalibrationSamples = 32;

HardwareSerial panelSerial(2);
agc::Core agcCore;
dsky::State state;

char panelLineBuffer[dsky::kLineBufferSize];
size_t panelLineLength = 0;

char usbLineBuffer[dsky::kLineBufferSize];
size_t usbLineLength = 0;

bool stateDirty = true;
unsigned long lastTelemetryMs = 0;
unsigned long lastMonitorMs = 0;
unsigned long lastJoystickSampleMs = 0;
unsigned long compActyUntilMs = 0;

enum class EntryMode : uint8_t {
  Idle = 0,
  Verb,
  Noun,
};

EntryMode entryMode = EntryMode::Idle;
char pendingDigits[3] = {'0', '0', '\0'};
uint8_t pendingCount = 0;
uint32_t manualLampMask = 0;

struct JoystickState {
  int rawX = 0;
  int rawY = 0;
  int centerX = kAdcMax / 2;
  int centerY = kAdcMax / 2;
  int normalizedX = 0;
  int normalizedY = 0;
  bool switchPressed = false;
};

JoystickState joystick;

uint16_t parseOctal(const char* text) {
  if (text == nullptr) {
    return 0;
  }

  return static_cast<uint16_t>(strtoul(text, nullptr, 8));
}

uint8_t clampDisplayCode(uint16_t value) {
  return static_cast<uint8_t>(agc::Core::toInt(value) % 100);
}

void formatAgcWord(char* buffer, size_t bufferSize, uint16_t word) {
  const int16_t value = agc::Core::toInt(word);
  const char sign = value < 0 ? '-' : '+';
  const unsigned int magnitude =
      static_cast<unsigned int>(abs(value) % 100000);
  snprintf(buffer, bufferSize, "%c%05u", sign, magnitude);
}

void setCoreDisplayRegister(uint16_t address, uint8_t value) {
  agcCore.writeErasable(address, agc::Core::fromInt(value));
}

int clampInt(int value, int minimum, int maximum) {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

int normalizeJoystickAxis(int raw, int center) {
  const int delta = raw - center;

  if (abs(delta) <= kJoystickDeadzone) {
    return 0;
  }

  const int span =
      delta > 0 ? (kAdcMax - center - kJoystickDeadzone)
                : (center - kJoystickDeadzone);

  if (span <= 0) {
    return 0;
  }

  const long scaled =
      static_cast<long>(delta > 0 ? delta - kJoystickDeadzone
                                  : delta + kJoystickDeadzone) *
      kJoystickOutputMax / span;
  return clampInt(static_cast<int>(scaled), -kJoystickOutputMax,
                  kJoystickOutputMax);
}

void writeJoystickToCore() {
  agcCore.writeErasable(agc::Core::kInputRhcX,
                        agc::Core::fromInt(joystick.normalizedX));
  agcCore.writeErasable(agc::Core::kInputRhcY,
                        agc::Core::fromInt(joystick.normalizedY));
  agcCore.writeErasable(agc::Core::kInputRhcSwitch,
                        agc::Core::fromInt(joystick.switchPressed ? 1 : 0));
}

void sampleJoystick() {
  joystick.rawX = analogRead(kJoystickXPin);
  joystick.rawY = analogRead(kJoystickYPin);
  joystick.normalizedX = normalizeJoystickAxis(joystick.rawX, joystick.centerX);
  joystick.normalizedY = normalizeJoystickAxis(joystick.rawY, joystick.centerY);
  joystick.switchPressed = digitalRead(kJoystickSwitchPin) == LOW;
  writeJoystickToCore();
}

void calibrateJoystick() {
  long xTotal = 0;
  long yTotal = 0;

  for (uint8_t i = 0; i < kJoystickCalibrationSamples; ++i) {
    xTotal += analogRead(kJoystickXPin);
    yTotal += analogRead(kJoystickYPin);
    delay(4);
  }

  joystick.centerX = static_cast<int>(xTotal / kJoystickCalibrationSamples);
  joystick.centerY = static_cast<int>(yTotal / kJoystickCalibrationSamples);
  sampleJoystick();
}

void syncLamps() {
  state.lampMask = manualLampMask;
  dsky::setLamp(&state, dsky::kLampProg, entryMode != EntryMode::Idle);
  dsky::setLamp(&state, dsky::kLampOprErr,
                agcCore.readErasable(agc::Core::kPanelAlarm) != 0 ||
                    agcCore.runState() == agc::Core::RunState::Faulted);
  dsky::setLamp(&state, dsky::kLampKeyRel, state.flashVerbNoun);
  dsky::setLamp(&state, dsky::kLampCompActy, millis() < compActyUntilMs);
  dsky::setLamp(&state, dsky::kLampNoAtt,
                agcCore.runState() == agc::Core::RunState::Halted);
  dsky::setLamp(&state, dsky::kLampStby,
                agcCore.runState() == agc::Core::RunState::Faulted);
}

void refreshDskyFromCore() {
  state.program =
      clampDisplayCode(agcCore.readErasable(agc::Core::kPanelProgram));
  state.verb = clampDisplayCode(agcCore.readErasable(agc::Core::kPanelVerb));
  state.noun = clampDisplayCode(agcCore.readErasable(agc::Core::kPanelNoun));

  if (state.noun == 99) {
    formatAgcWord(state.r1, sizeof(state.r1),
                  agcCore.readErasable(agc::Core::kInputRhcX));
    formatAgcWord(state.r2, sizeof(state.r2),
                  agcCore.readErasable(agc::Core::kInputRhcY));
    formatAgcWord(state.r3, sizeof(state.r3),
                  agcCore.readErasable(agc::Core::kInputRhcSwitch));
  } else {
    formatAgcWord(state.r1, sizeof(state.r1),
                  agcCore.readErasable(agc::Core::kPanelCounter));
    formatAgcWord(state.r2, sizeof(state.r2), agcCore.getA());
    formatAgcWord(state.r3, sizeof(state.r3),
                  agc::Core::fromInt(static_cast<int16_t>(agcCore.getZ())));
  }

  state.alarm = static_cast<uint16_t>(
      abs(agc::Core::toInt(agcCore.readErasable(agc::Core::kPanelAlarm))) %
      10000);
  state.missionSeconds = agcCore.cycles() / 1024UL;
  syncLamps();
  stateDirty = true;
}

void pulseCompActy() {
  compActyUntilMs = millis() + kCompActyPulseMs;
  refreshDskyFromCore();
}

void clearAlarm() {
  agcCore.writeErasable(agc::Core::kPanelAlarm, 0);
  if (entryMode == EntryMode::Idle) {
    state.flashVerbNoun = false;
  }
  refreshDskyFromCore();
}

void raiseAlarm(uint16_t alarmCode) {
  agcCore.writeErasable(agc::Core::kPanelAlarm,
                        agc::Core::fromInt(static_cast<int16_t>(alarmCode)));
  state.flashVerbNoun = true;
  refreshDskyFromCore();
}

void endEntry() {
  entryMode = EntryMode::Idle;
  pendingCount = 0;
  pendingDigits[0] = '0';
  pendingDigits[1] = '0';
  pendingDigits[2] = '\0';

  if (agcCore.readErasable(agc::Core::kPanelAlarm) == 0) {
    state.flashVerbNoun = false;
  }

  refreshDskyFromCore();
}

void beginEntry(EntryMode mode) {
  entryMode = mode;
  pendingCount = 0;
  pendingDigits[0] = '0';
  pendingDigits[1] = '0';
  pendingDigits[2] = '\0';
  state.flashVerbNoun = true;
  refreshDskyFromCore();
}

void applyPendingEntry() {
  const uint8_t value = static_cast<uint8_t>(
      (pendingDigits[0] - '0') * 10 + (pendingDigits[1] - '0'));

  if (entryMode == EntryMode::Verb) {
    setCoreDisplayRegister(agc::Core::kPanelVerb, value);
  } else if (entryMode == EntryMode::Noun) {
    setCoreDisplayRegister(agc::Core::kPanelNoun, value);
  }

  endEntry();
}

void handleDigit(uint8_t digit) {
  if (entryMode == EntryMode::Idle) {
    raiseAlarm(1105);
    return;
  }

  if (pendingCount < 2) {
    pendingDigits[pendingCount++] = static_cast<char>('0' + digit);
  }

  pulseCompActy();

  if (pendingCount == 2) {
    applyPendingEntry();
  }
}

void writeLastKey(dsky::Key key) {
  agcCore.writeErasable(agc::Core::kPanelLastKey,
                        agc::Core::fromInt(static_cast<int16_t>(key)));
}

void handleKey(dsky::Key key) {
  writeLastKey(key);

  switch (key) {
    case dsky::Key::Digit0:
      handleDigit(0);
      break;
    case dsky::Key::Digit1:
      handleDigit(1);
      break;
    case dsky::Key::Digit2:
      handleDigit(2);
      break;
    case dsky::Key::Digit3:
      handleDigit(3);
      break;
    case dsky::Key::Digit4:
      handleDigit(4);
      break;
    case dsky::Key::Digit5:
      handleDigit(5);
      break;
    case dsky::Key::Digit6:
      handleDigit(6);
      break;
    case dsky::Key::Digit7:
      handleDigit(7);
      break;
    case dsky::Key::Digit8:
      handleDigit(8);
      break;
    case dsky::Key::Digit9:
      handleDigit(9);
      break;
    case dsky::Key::Verb:
      beginEntry(EntryMode::Verb);
      pulseCompActy();
      break;
    case dsky::Key::Noun:
      beginEntry(EntryMode::Noun);
      pulseCompActy();
      break;
    case dsky::Key::Clear:
      endEntry();
      clearAlarm();
      pulseCompActy();
      break;
    case dsky::Key::Proceed: {
      const uint8_t program =
          static_cast<uint8_t>((state.program + 1U) % 100U);
      setCoreDisplayRegister(agc::Core::kPanelProgram, program);
      pulseCompActy();
      break;
    }
    case dsky::Key::KeyRelease:
      endEntry();
      clearAlarm();
      pulseCompActy();
      break;
    case dsky::Key::Enter:
      if (entryMode != EntryMode::Idle && pendingCount == 2) {
        applyPendingEntry();
      }
      pulseCompActy();
      break;
    case dsky::Key::Reset:
      clearAlarm();
      pulseCompActy();
      break;
    case dsky::Key::Plus:
      manualLampMask ^= dsky::kLampUplinkActy;
      pulseCompActy();
      break;
    case dsky::Key::Minus:
      manualLampMask ^= dsky::kLampGimbalLock;
      pulseCompActy();
      break;
    default:
      break;
  }
}

void emitStateFrame(Stream& port) {
  char line[dsky::kLineBufferSize];
  if (!dsky::formatStateLine(state, line, sizeof(line))) {
    return;
  }

  port.println(line);
}

void emitCoreStatus(Stream& port) {
  port.print(F("CORE Z="));
  port.print(agcCore.getZ(), OCT);
  port.print(F(" A="));
  port.print(agcCore.getA(), OCT);
  port.print(F(" INST="));
  port.print(agcCore.lastInstruction(), OCT);
  port.print(F(" CYC="));
  port.print(agcCore.cycles());
  port.print(F(" STATE="));
  port.print(static_cast<unsigned int>(agcCore.runState()));
  port.print(F(" FAULT="));
  port.println(static_cast<unsigned int>(agcCore.fault()));
}

void emitJoystickStatus(Stream& port) {
  port.print(F("JOY RAWX="));
  port.print(joystick.rawX);
  port.print(F(" RAWY="));
  port.print(joystick.rawY);
  port.print(F(" X="));
  port.print(joystick.normalizedX);
  port.print(F(" Y="));
  port.print(joystick.normalizedY);
  port.print(F(" SW="));
  port.print(joystick.switchPressed ? 1 : 0);
  port.print(F(" CX="));
  port.print(joystick.centerX);
  port.print(F(" CY="));
  port.println(joystick.centerY);
}

void emitMonitorLine(Stream& port) {
  port.print(F("MONITOR P="));
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
  port.print(F(" LAMPS="));
  port.print(state.lampMask, HEX);
  port.print(F(" JOYX="));
  port.print(joystick.normalizedX);
  port.print(F(" JOYY="));
  port.print(joystick.normalizedY);
  port.print(F(" JOYSW="));
  port.print(joystick.switchPressed ? 1 : 0);
  port.print(F(" Z="));
  port.print(agcCore.getZ(), OCT);
  port.print(F(" A="));
  port.print(agcCore.getA(), OCT);
  port.print(F(" CYCLES="));
  port.print(agcCore.cycles());
  port.print(F(" RUN="));
  port.print(static_cast<unsigned int>(agcCore.runState()));
  port.print(F(" FAULT="));
  port.println(static_cast<unsigned int>(agcCore.fault()));
}

void handleConsoleCommand(char* line) {
  if (strcmp(line, "HELP") == 0) {
    Serial.println(F("Commands:"));
    Serial.println(F("  KEY,<name>"));
    Serial.println(F("  RUN HALT STEP RESET STATE CORE"));
    Serial.println(F("  PEEK,<octal-address>"));
    Serial.println(F("  POKE,<octal-address>,<octal-word>"));
    Serial.println(F("  JOY JOYCAL"));
    Serial.println(F("  ALARM,<code> CLEARALARM"));
    return;
  }

  if (strcmp(line, "RUN") == 0) {
    agcCore.start();
    refreshDskyFromCore();
    return;
  }

  if (strcmp(line, "HALT") == 0) {
    agcCore.halt();
    refreshDskyFromCore();
    return;
  }

  if (strcmp(line, "STEP") == 0) {
    agcCore.step();
    refreshDskyFromCore();
    emitCoreStatus(Serial);
    return;
  }

  if (strcmp(line, "RESET") == 0) {
    agcCore.reset();
    agcCore.start();
    state.flashVerbNoun = false;
    manualLampMask = 0;
    entryMode = EntryMode::Idle;
    refreshDskyFromCore();
    return;
  }

  if (strcmp(line, "STATE") == 0) {
    emitStateFrame(Serial);
    return;
  }

  if (strcmp(line, "CORE") == 0) {
    emitCoreStatus(Serial);
    return;
  }

  if (strcmp(line, "JOY") == 0) {
    emitJoystickStatus(Serial);
    return;
  }

  if (strcmp(line, "JOYCAL") == 0) {
    calibrateJoystick();
    refreshDskyFromCore();
    emitJoystickStatus(Serial);
    return;
  }

  if (strncmp(line, "PEEK,", 5) == 0) {
    const uint16_t address = parseOctal(line + 5);
    Serial.print(F("PEEK,"));
    Serial.print(address, OCT);
    Serial.print(F(","));
    Serial.println(agcCore.read(address), OCT);
    return;
  }

  if (strncmp(line, "POKE,", 5) == 0) {
    char* addressText = line + 5;
    char* valueText = strchr(addressText, ',');
    if (valueText != nullptr) {
      *valueText = '\0';
      valueText++;
      agcCore.write(parseOctal(addressText), parseOctal(valueText));
      refreshDskyFromCore();
    }
    return;
  }

  if (strncmp(line, "ALARM,", 6) == 0) {
    raiseAlarm(static_cast<uint16_t>(strtoul(line + 6, nullptr, 10)));
    return;
  }

  if (strcmp(line, "CLEARALARM") == 0) {
    clearAlarm();
    return;
  }

  Serial.print(F("Unknown command: "));
  Serial.println(line);
}

void handlePanelLine(char* line) {
  dsky::Key key = dsky::Key::None;
  if (dsky::parseKeyLine(line, &key)) {
    handleKey(key);
  }
}

void handleUsbLine(char* line) {
  char scratch[dsky::kLineBufferSize];
  dsky::copyField(scratch, sizeof(scratch), line);

  dsky::Key key = dsky::Key::None;
  if (dsky::parseKeyLine(scratch, &key)) {
    handleKey(key);
    return;
  }

  handleConsoleCommand(line);
}

void pollPanelSerial() {
  while (panelSerial.available() > 0) {
    const char incoming = static_cast<char>(panelSerial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      panelLineBuffer[panelLineLength] = '\0';
      handlePanelLine(panelLineBuffer);
      panelLineLength = 0;
      continue;
    }

    if (panelLineLength + 1 < sizeof(panelLineBuffer)) {
      panelLineBuffer[panelLineLength++] = incoming;
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

}  // namespace

void setup() {
  Serial.begin(kUsbBaud);
  panelSerial.begin(kPanelBaud, SERIAL_8N1, kPanelRxPin, kPanelTxPin);
  pinMode(kJoystickSwitchPin, INPUT_PULLUP);
#if defined(ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(kJoystickXPin, ADC_11db);
  analogSetPinAttenuation(kJoystickYPin, ADC_11db);
#endif

  agcCore.reset();
  agcCore.start();
  calibrateJoystick();
  dsky::initState(&state);
  refreshDskyFromCore();

  delay(250);
  Serial.println(F("ESP32 AGC core layer ready."));
  Serial.println(F("Type HELP or send KEY,<name> over USB serial."));
  emitCoreStatus(Serial);
  emitStateFrame(Serial);
}

void loop() {
  pollPanelSerial();
  pollUsbSerial();

  const unsigned long now = millis();

  if (now - lastJoystickSampleMs >= kJoystickSamplePeriodMs) {
    lastJoystickSampleMs = now;
    sampleJoystick();
  }

  if (agcCore.runState() == agc::Core::RunState::Running) {
    agcCore.runFor(kInstructionsPerLoop);
    refreshDskyFromCore();
  }

  if (millis() >= compActyUntilMs &&
      dsky::lampEnabled(state, dsky::kLampCompActy)) {
    refreshDskyFromCore();
  }

  if (stateDirty || now - lastTelemetryMs >= kTelemetryPeriodMs) {
    lastTelemetryMs = now;
    emitStateFrame(panelSerial);
    emitStateFrame(Serial);
    stateDirty = false;
  }

  if (now - lastMonitorMs >= kMonitorPeriodMs) {
    lastMonitorMs = now;
    emitMonitorLine(Serial);
  }
}
