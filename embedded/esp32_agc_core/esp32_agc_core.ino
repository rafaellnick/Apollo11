#include "agc_core.h"
#include "dsky_protocol.h"

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
constexpr unsigned long kLaunchEventMonitorMs = 1000;
constexpr uint16_t kInstructionsPerLoop = 64;
constexpr int kAdcMax = 4095;
constexpr int kJoystickDeadzone = 80;
constexpr int kJoystickOutputMax = 1000;
constexpr uint8_t kJoystickCalibrationSamples = 32;
constexpr int16_t kLaunchStartSecond = -10;
constexpr int16_t kLaunchOrbitInsertionSecond = 705;
constexpr uint8_t kLaunchDefaultTimeScale = 20;

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

enum class UsbOutputMode : uint8_t {
  Quiet = 0,
  Clean,
  Raw,
  Both,
};

enum class LaunchMode : uint8_t {
  Off = 0,
  Running,
  Complete,
};

struct LaunchEvent {
  int16_t second;
  const char* label;
};

constexpr LaunchEvent kLaunchEvents[] = {
    {-10, "TERMINAL COUNT"},
    {-8, "F-1 IGNITION"},
    {0, "LIFTOFF"},
    {13, "ROLL PROGRAM"},
    {34, "ROLL COMPLETE"},
    {83, "MAX-Q"},
    {137, "S-IC INBOARD CUTOFF"},
    {164, "S-IC/S-II STAGING"},
    {193, "INTERSTAGE SEP"},
    {197, "LES JETTISON"},
    {327, "S-IVB TO COI"},
    {462, "S-II INBOARD CUTOFF"},
    {502, "MIXTURE SHIFT"},
    {540, "ABORT MODE IV"},
    {555, "S-II/S-IVB STAGING"},
    {705, "PARKING ORBIT"},
};

EntryMode entryMode = EntryMode::Idle;
UsbOutputMode usbOutputMode = UsbOutputMode::Clean;
LaunchMode launchMode = LaunchMode::Off;
char pendingDigits[3] = {'0', '0', '\0'};
uint8_t pendingCount = 0;
uint32_t manualLampMask = 0;
uint32_t launchLampMask = 0;
unsigned long launchStartMs = 0;
unsigned long lastLaunchMonitorMs = 0;
int16_t launchSecond = 0;
int16_t launchAltitudeKm = 0;
int16_t launchVelocityMs = 0;
uint8_t launchTimeScale = kLaunchDefaultTimeScale;
int8_t lastLaunchEventIndex = -1;
char launchPhase[24] = "IDLE";

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
  state.lampMask = manualLampMask | launchLampMask;
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
  } else if (launchMode != LaunchMode::Off && state.noun == 62) {
    formatAgcWord(state.r1, sizeof(state.r1),
                  agc::Core::fromInt(launchSecond));
    formatAgcWord(state.r2, sizeof(state.r2),
                  agc::Core::fromInt(launchAltitudeKm));
    formatAgcWord(state.r3, sizeof(state.r3),
                  agc::Core::fromInt(launchVelocityMs));
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
  state.missionSeconds =
      launchMode == LaunchMode::Off
          ? agcCore.cycles() / 1024UL
          : static_cast<uint32_t>(launchSecond > 0 ? launchSecond : 0);
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

int32_t lerpInt(int32_t startValue, int32_t endValue, int16_t second,
                int16_t startSecond, int16_t endSecond) {
  if (second <= startSecond) {
    return startValue;
  }

  if (second >= endSecond || endSecond == startSecond) {
    return endValue;
  }

  return startValue +
         ((endValue - startValue) * (second - startSecond)) /
             (endSecond - startSecond);
}

int8_t launchEventIndexForSecond(int16_t second) {
  int8_t index = -1;
  for (uint8_t i = 0; i < sizeof(kLaunchEvents) / sizeof(kLaunchEvents[0]);
       ++i) {
    if (second >= kLaunchEvents[i].second) {
      index = static_cast<int8_t>(i);
    }
  }

  return index;
}

int16_t currentScaledLaunchSecond() {
  const uint64_t elapsedMs = millis() - launchStartMs;
  const uint64_t scaledSeconds =
      (elapsedMs * static_cast<uint64_t>(launchTimeScale)) / 1000ULL;
  const int32_t second =
      static_cast<int32_t>(kLaunchStartSecond) +
      static_cast<int32_t>(scaledSeconds);

  if (second < kLaunchStartSecond) {
    return kLaunchStartSecond;
  }

  if (second > kLaunchOrbitInsertionSecond) {
    return kLaunchOrbitInsertionSecond;
  }

  return static_cast<int16_t>(second);
}

void computeLaunchTelemetry(int16_t second) {
  const int8_t eventIndex = launchEventIndexForSecond(second);
  if (eventIndex >= 0) {
    dsky::copyField(launchPhase, sizeof(launchPhase),
                    kLaunchEvents[eventIndex].label);
  }

  if (second < 0) {
    launchAltitudeKm = 0;
    launchVelocityMs = 0;
    launchLampMask = dsky::kLampProg | dsky::kLampKeyRel;
    return;
  }

  launchLampMask = dsky::kLampProg | dsky::kLampTracker;

  if (second < kLaunchOrbitInsertionSecond) {
    launchLampMask |= dsky::kLampUplinkActy;
  }

  if (second <= 83) {
    launchAltitudeKm =
        static_cast<int16_t>(lerpInt(0, 14, second, 0, 83));
    launchVelocityMs =
        static_cast<int16_t>(lerpInt(0, 520, second, 0, 83));
    return;
  }

  if (second <= 164) {
    launchAltitudeKm =
        static_cast<int16_t>(lerpInt(14, 65, second, 83, 164));
    launchVelocityMs =
        static_cast<int16_t>(lerpInt(520, 2700, second, 83, 164));
    return;
  }

  if (second <= 197) {
    launchAltitudeKm =
        static_cast<int16_t>(lerpInt(65, 95, second, 164, 197));
    launchVelocityMs =
        static_cast<int16_t>(lerpInt(2700, 3200, second, 164, 197));
    return;
  }

  if (second <= 462) {
    launchAltitudeKm =
        static_cast<int16_t>(lerpInt(95, 176, second, 197, 462));
    launchVelocityMs =
        static_cast<int16_t>(lerpInt(3200, 5291, second, 197, 462));
    return;
  }

  if (second <= 555) {
    launchAltitudeKm =
        static_cast<int16_t>(lerpInt(176, 187, second, 462, 555));
    launchVelocityMs =
        static_cast<int16_t>(lerpInt(5291, 7049, second, 462, 555));
    return;
  }

  launchAltitudeKm =
      static_cast<int16_t>(lerpInt(187, 185, second, 555,
                                  kLaunchOrbitInsertionSecond));
  launchVelocityMs =
      static_cast<int16_t>(lerpInt(7049, 7800, second, 555,
                                  kLaunchOrbitInsertionSecond));
}

void configureLaunchDisplay() {
  setCoreDisplayRegister(agc::Core::kPanelProgram, 11);
  setCoreDisplayRegister(agc::Core::kPanelVerb, 16);
  setCoreDisplayRegister(agc::Core::kPanelNoun, 62);
  agcCore.writeErasable(agc::Core::kPanelAlarm, 0);
  state.flashVerbNoun = false;
}

void printLaunchTime(Stream& port, int16_t second) {
  port.print(second < 0 ? F("T-") : F("T+"));
  int16_t remaining = second < 0 ? static_cast<int16_t>(-second) : second;
  const uint8_t minutes = static_cast<uint8_t>(remaining / 60);
  const uint8_t seconds = static_cast<uint8_t>(remaining % 60);

  if (minutes < 10) {
    port.print('0');
  }
  port.print(minutes);
  port.print(':');
  if (seconds < 10) {
    port.print('0');
  }
  port.print(seconds);
}

void emitLaunchStatus(Stream& port) {
  port.print(F("LAUNCH "));
  printLaunchTime(port, launchSecond);
  port.print(F(" "));
  port.print(launchPhase);
  port.print(F(" | P11 V16 N62"));
  port.print(F(" | R1 T"));
  port.print(launchSecond >= 0 ? '+' : '-');
  port.print(abs(launchSecond));
  port.print(F(" R2 ALT_KM "));
  port.print(launchAltitudeKm);
  port.print(F(" R3 VEL_MS "));
  port.print(launchVelocityMs);
  port.print(F(" | x"));
  port.print(launchTimeScale);
  port.println(launchMode == LaunchMode::Complete ? F(" COMPLETE") : F(""));
}

void startLaunchSimulation() {
  launchMode = LaunchMode::Running;
  launchStartMs = millis();
  lastLaunchMonitorMs = 0;
  lastLaunchEventIndex = -1;
  launchSecond = kLaunchStartSecond;
  computeLaunchTelemetry(launchSecond);
  configureLaunchDisplay();
  refreshDskyFromCore();

  Serial.print(F("Apollo 11 launch simulation started at x"));
  Serial.println(launchTimeScale);
  emitLaunchStatus(Serial);
}

void stopLaunchSimulation(bool resetPanel) {
  launchMode = LaunchMode::Off;
  launchLampMask = 0;
  lastLaunchEventIndex = -1;
  dsky::copyField(launchPhase, sizeof(launchPhase), "IDLE");

  if (resetPanel) {
    setCoreDisplayRegister(agc::Core::kPanelProgram, 0);
    setCoreDisplayRegister(agc::Core::kPanelVerb, 16);
    setCoreDisplayRegister(agc::Core::kPanelNoun, 36);
  }

  refreshDskyFromCore();
  Serial.println(F("Apollo 11 launch simulation stopped."));
}

void setLaunchTimeScale(uint8_t scale) {
  if (scale == 0) {
    scale = 1;
  }

  if (scale > 100) {
    scale = 100;
  }

  int16_t currentSecond = launchSecond;
  if (launchMode == LaunchMode::Running) {
    currentSecond = currentScaledLaunchSecond();
  }

  launchTimeScale = scale;

  if (launchMode == LaunchMode::Running) {
    const uint32_t elapsedMs =
        static_cast<uint32_t>(
            (static_cast<uint32_t>(currentSecond - kLaunchStartSecond) *
             1000UL) /
            launchTimeScale);
    launchStartMs = millis() - elapsedMs;
  }

  Serial.print(F("Launch simulation speed x"));
  Serial.println(launchTimeScale);
}

void updateLaunchSimulation() {
  if (launchMode != LaunchMode::Running) {
    return;
  }

  const int16_t nextSecond = currentScaledLaunchSecond();
  const unsigned long now = millis();
  const bool secondChanged = nextSecond != launchSecond;
  const bool monitorDue =
      lastLaunchMonitorMs == 0 ||
      now - lastLaunchMonitorMs >= kLaunchEventMonitorMs;

  if (!secondChanged && !monitorDue) {
    return;
  }

  launchSecond = nextSecond;
  computeLaunchTelemetry(launchSecond);
  const bool reachedOrbit = launchSecond >= kLaunchOrbitInsertionSecond;
  if (reachedOrbit) {
    launchMode = LaunchMode::Complete;
    launchLampMask = dsky::kLampProg | dsky::kLampTracker;
    dsky::copyField(launchPhase, sizeof(launchPhase), "PARKING ORBIT");
  }

  configureLaunchDisplay();
  refreshDskyFromCore();

  const int8_t eventIndex = launchEventIndexForSecond(launchSecond);
  if (eventIndex != lastLaunchEventIndex) {
    lastLaunchEventIndex = eventIndex;
    pulseCompActy();
    emitLaunchStatus(Serial);
  } else if (monitorDue) {
    emitLaunchStatus(Serial);
  }

  lastLaunchMonitorMs = now;
}

void handleDskyEnterCommand() {
  const uint8_t verb =
      clampDisplayCode(agcCore.readErasable(agc::Core::kPanelVerb));
  const uint8_t noun =
      clampDisplayCode(agcCore.readErasable(agc::Core::kPanelNoun));

  if (verb == 37 && noun == 11) {
    startLaunchSimulation();
    return;
  }

  if (verb == 37 && noun == 0) {
    stopLaunchSimulation(true);
    return;
  }

  if (verb == 37 && noun == 12) {
    setLaunchTimeScale(1);
    startLaunchSimulation();
    return;
  }
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
      } else if (entryMode == EntryMode::Idle) {
        handleDskyEnterCommand();
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

const __FlashStringHelper* runStateText() {
  switch (agcCore.runState()) {
    case agc::Core::RunState::Running:
      return F("RUN");
    case agc::Core::RunState::Halted:
      return F("HALT");
    case agc::Core::RunState::Faulted:
      return F("FAULT");
  }

  return F("?");
}

void printTwoDigits(Stream& port, uint8_t value) {
  if (value < 10) {
    port.print('0');
  }
  port.print(value);
}

void printFourDigits(Stream& port, uint16_t value) {
  value %= 10000;

  if (value < 1000) {
    port.print('0');
  }
  if (value < 100) {
    port.print('0');
  }
  if (value < 10) {
    port.print('0');
  }
  port.print(value);
}

void printSignedFour(Stream& port, int value) {
  if (value < 0) {
    port.print('-');
    value = -value;
  } else {
    port.print('+');
  }

  value %= 10000;
  printFourDigits(port, static_cast<uint16_t>(value));
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

void emitCleanStatus(Stream& port) {
  port.print(F("AGC P"));
  printTwoDigits(port, state.program);
  port.print(F(" V"));
  printTwoDigits(port, state.verb);
  port.print(F(" N"));
  printTwoDigits(port, state.noun);
  port.print(F(" | R1 "));
  port.print(state.r1);
  port.print(F(" R2 "));
  port.print(state.r2);
  port.print(F(" R3 "));
  port.print(state.r3);
  port.print(F(" | ALM "));
  printFourDigits(port, state.alarm);
  port.print(F(" | JOY "));
  printSignedFour(port, joystick.normalizedX);
  port.print(',');
  printSignedFour(port, joystick.normalizedY);
  port.print(',');
  port.print(joystick.switchPressed ? 1 : 0);
  port.print(F(" | Z "));
  port.print(agcCore.getZ(), OCT);
  port.print(F(" A "));
  port.print(agcCore.getA(), OCT);
  port.print(F(" | CYC "));
  port.print(agcCore.cycles());
  port.print(F(" | "));
  port.print(runStateText());
  if (launchMode != LaunchMode::Off) {
    port.print(F(" | ASC "));
    printLaunchTime(port, launchSecond);
    port.print(' ');
    port.print(launchPhase);
    port.print(F(" ALT "));
    port.print(launchAltitudeKm);
    port.print(F("km VEL "));
    port.print(launchVelocityMs);
    port.print(F("m/s x"));
    port.print(launchTimeScale);
  }
  port.println();
}

bool usbWantsRawState() {
  return usbOutputMode == UsbOutputMode::Raw ||
         usbOutputMode == UsbOutputMode::Both;
}

bool usbWantsCleanStatus() {
  return usbOutputMode == UsbOutputMode::Clean ||
         usbOutputMode == UsbOutputMode::Both;
}

void setUsbOutputMode(UsbOutputMode mode) {
  usbOutputMode = mode;

  Serial.print(F("USB output: "));
  switch (usbOutputMode) {
    case UsbOutputMode::Quiet:
      Serial.println(F("QUIET"));
      break;
    case UsbOutputMode::Clean:
      Serial.println(F("CLEAN"));
      break;
    case UsbOutputMode::Raw:
      Serial.println(F("RAW"));
      break;
    case UsbOutputMode::Both:
      Serial.println(F("BOTH"));
      break;
  }
}

void handleConsoleCommand(char* line) {
  if (strcmp(line, "HELP") == 0) {
    Serial.println(F("Commands:"));
    Serial.println(F("  KEY,<name>"));
    Serial.println(F("  RUN HALT STEP RESET STATE CORE"));
    Serial.println(F("  PEEK,<octal-address>"));
    Serial.println(F("  POKE,<octal-address>,<octal-word>"));
    Serial.println(F("  JOY JOYCAL"));
    Serial.println(F("  STATUS"));
    Serial.println(F("  LAUNCH LAUNCH,STOP LAUNCH,STATUS"));
    Serial.println(F("  LAUNCH,SPEED,<1-100> LAUNCH,REALTIME"));
    Serial.println(F("  USB,CLEAN USB,RAW USB,BOTH USB,QUIET"));
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
    launchMode = LaunchMode::Off;
    launchLampMask = 0;
    lastLaunchEventIndex = -1;
    dsky::copyField(launchPhase, sizeof(launchPhase), "IDLE");
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

  if (strcmp(line, "STATUS") == 0) {
    emitCleanStatus(Serial);
    return;
  }

  if (strcmp(line, "CORE") == 0) {
    emitCoreStatus(Serial);
    return;
  }

  if (strcmp(line, "USB,CLEAN") == 0) {
    setUsbOutputMode(UsbOutputMode::Clean);
    return;
  }

  if (strcmp(line, "USB,RAW") == 0) {
    setUsbOutputMode(UsbOutputMode::Raw);
    return;
  }

  if (strcmp(line, "USB,BOTH") == 0) {
    setUsbOutputMode(UsbOutputMode::Both);
    return;
  }

  if (strcmp(line, "USB,QUIET") == 0) {
    setUsbOutputMode(UsbOutputMode::Quiet);
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

  if (strcmp(line, "LAUNCH") == 0) {
    startLaunchSimulation();
    return;
  }

  if (strcmp(line, "LAUNCH,STOP") == 0) {
    stopLaunchSimulation(true);
    return;
  }

  if (strcmp(line, "LAUNCH,STATUS") == 0) {
    emitLaunchStatus(Serial);
    return;
  }

  if (strcmp(line, "LAUNCH,REALTIME") == 0) {
    setLaunchTimeScale(1);
    return;
  }

  if (strncmp(line, "LAUNCH,SPEED,", 13) == 0) {
    setLaunchTimeScale(
        static_cast<uint8_t>(strtoul(line + 13, nullptr, 10)));
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
  char scratch[dsky::kLineBufferSize];
  dsky::copyField(scratch, sizeof(scratch), line);

  dsky::Key key = dsky::Key::None;
  if (dsky::parseKeyLine(scratch, &key)) {
    handleKey(key);
    return;
  }

  handleConsoleCommand(line);
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
  Serial.println(F("USB output: CLEAN. Type HELP for commands."));
  emitCleanStatus(Serial);
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

  updateLaunchSimulation();

  if (millis() >= compActyUntilMs &&
      dsky::lampEnabled(state, dsky::kLampCompActy)) {
    refreshDskyFromCore();
  }

  if (stateDirty || now - lastTelemetryMs >= kTelemetryPeriodMs) {
    lastTelemetryMs = now;
    emitStateFrame(panelSerial);
    if (usbWantsRawState()) {
      emitStateFrame(Serial);
    }
    stateDirty = false;
  }

  if (now - lastMonitorMs >= kMonitorPeriodMs) {
    lastMonitorMs = now;
    if (usbWantsCleanStatus()) {
      emitCleanStatus(Serial);
    }
  }
}
