#include "agc_core.h"
#include "dsky_protocol.h"
#include "pinball_nouns.h"
#include "rope_image.h"

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
dsky::Phase phase;

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

struct MissionScenario {
  uint8_t noun;
  uint8_t program;
  uint8_t verb;
  const char* label;
  const char* r1Label;
  const char* r2Label;
  const char* r3Label;
  int16_t r1;
  int16_t r2;
  int16_t r3;
  uint16_t alarm;
  uint32_t lampMask;
};

struct MissionTelemetry {
  int16_t getMin;
  int16_t altitudeKm;
  int16_t altitudeM;
  int16_t velocityMs;
  int16_t distanceKkm;
  int16_t rangeM;
  int16_t rangeKm;
  int16_t burnSec;
  int16_t deltaVMps;
  int16_t imuRollDeg;
  int16_t imuPitchDeg;
  int16_t imuYawDeg;
  int16_t radarAltitudeM;
  int16_t radarRangeM;
  int16_t propellantPct;
  int16_t status;
  int16_t evaMin;
  int16_t sampleKg;
  int16_t mcc;
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

constexpr MissionScenario kMissionScenarios[] = {
    {20, 11, 16, "EARTH PARKING ORBIT", "GET_MIN", "ALT_KM", "VEL_MS", 12,
     185, 7800, 0, dsky::kLampProg | dsky::kLampTracker},
    {21, 15, 16, "TRANSLUNAR INJECTION", "GET_MIN", "BURN_SEC", "DV_MPS",
     164, 348, 3200, 0, dsky::kLampProg | dsky::kLampUplinkActy},
    {22, 17, 16, "CSM TRANSPOSE DOCK", "GET_MIN", "RANGE_M", "DOCKED",
     190, 30, 1, 0, dsky::kLampProg | dsky::kLampCompActy},
    {23, 23, 16, "TRANSLUNAR COAST", "GET_MIN", "DIST_KKM", "MCC", 720,
     120, 2, 0, dsky::kLampTracker},
    {24, 40, 16, "LUNAR ORBIT INSERTION", "GET_MIN", "BURN_SEC", "DV_MPS",
     4470, 357, 900, 0, dsky::kLampProg | dsky::kLampUplinkActy},
    {25, 20, 16, "LUNAR ORBIT", "GET_MIN", "ALT_KM", "ORBIT", 4520, 111,
     1, 0, dsky::kLampTracker},
    {26, 63, 16, "POWERED DESCENT", "GET_MIN", "ALT_KM", "VEL_MS", 6120,
     15, 1680, 0, dsky::kLampProg | dsky::kLampTracker},
    {27, 66, 16, "LANDING FINAL", "GET_MIN", "ALT_M", "VEL_MS", 6150,
     150, 50, 0, dsky::kLampProg | dsky::kLampKeyRel},
    {28, 68, 16, "LANDED SURFACE", "GET_MIN", "ALT_M", "LANDED", 6155,
     0, 1, 0, dsky::kLampProg | dsky::kLampTracker},
    {29, 0, 16, "LUNAR SURFACE EVA", "GET_MIN", "EVA_MIN", "SAMPLE_KG",
     6540, 151, 22, 0, dsky::kLampTracker},
    {30, 12, 16, "LUNAR ASCENT", "GET_MIN", "BURN_SEC", "VEL_MS", 7460,
     435, 1800, 0, dsky::kLampProg | dsky::kLampUplinkActy},
    {31, 20, 16, "RENDEZVOUS DOCKING", "GET_MIN", "RANGE_KM", "DOCKED",
     7560, 0, 1, 0, dsky::kLampProg | dsky::kLampCompActy},
    {32, 40, 16, "TRANSEARTH INJECTION", "GET_MIN", "BURN_SEC", "DV_MPS",
     8080, 151, 1000, 0, dsky::kLampProg | dsky::kLampUplinkActy},
    {33, 23, 16, "TRANSEARTH COAST", "GET_MIN", "DIST_KKM", "MCC", 9000,
     250, 0, 0, dsky::kLampTracker},
    {34, 61, 16, "ENTRY INTERFACE", "GET_MIN", "ALT_KM", "VEL_MS", 11700,
     122, 11000, 0, dsky::kLampProg | dsky::kLampTemp},
    {35, 67, 16, "SPLASHDOWN", "GET_MIN", "ALT_KM", "RECOVERY", 11773, 0,
     1, 0, dsky::kLampProg | dsky::kLampTracker},
    {40, 70, 16, "LAUNCH ABORT MODE I", "GET_MIN", "MODE", "STATUS", 1, 1,
     40, 0, dsky::kLampOprErr | dsky::kLampStby},
    {41, 37, 16, "EARTH ORBIT ABORT", "GET_MIN", "DV_MPS", "STATUS", 180,
     200, 41, 0, dsky::kLampOprErr | dsky::kLampUplinkActy},
    {42, 37, 16, "FREE RETURN ABORT", "GET_MIN", "MCC", "STATUS", 1000, 1,
     42, 0, dsky::kLampOprErr | dsky::kLampTracker},
    {43, 71, 16, "LUNAR DESCENT ABORT", "GET_MIN", "ALT_KM", "STATUS",
     6140, 3, 43, 0, dsky::kLampOprErr | dsky::kLampProg},
    {44, 63, 16, "PROGRAM ALARM 1202", "ALARM", "RECYCLE", "STATUS", 1202,
     1, 0, 1202, dsky::kLampOprErr | dsky::kLampKeyRel},
    {45, 63, 16, "PROGRAM ALARM 1201", "ALARM", "RECYCLE", "STATUS", 1201,
     1, 0, 1201, dsky::kLampOprErr | dsky::kLampKeyRel},
    {46, 0, 16, "COMMUNICATION LOSS", "GET_MIN", "UPLINK", "STATUS", 300,
     0, 46, 0, dsky::kLampOprErr},
    {47, 52, 16, "IMU REALIGN", "GET_MIN", "STAR", "STATUS", 300, 1, 52,
     0, dsky::kLampProg | dsky::kLampTracker},
    {48, 0, 16, "MANUAL ATTITUDE", "GET_MIN", "RHC", "STATUS", 300, 99,
     48, 0, dsky::kLampProg | dsky::kLampKeyRel},
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
int8_t activeMissionScenarioIndex = -1;
uint32_t missionScenarioLampMask = 0;

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

const MissionScenario* activeMissionScenario() {
  if (activeMissionScenarioIndex < 0) {
    return nullptr;
  }

  return &kMissionScenarios[activeMissionScenarioIndex];
}

int8_t missionScenarioIndexForNoun(uint8_t noun) {
  for (uint8_t i = 0;
       i < sizeof(kMissionScenarios) / sizeof(kMissionScenarios[0]); ++i) {
    if (kMissionScenarios[i].noun == noun) {
      return static_cast<int8_t>(i);
    }
  }

  return -1;
}

void clearMissionScenario() {
  activeMissionScenarioIndex = -1;
  missionScenarioLampMask = 0;
}

void setPhaseText(const char* label, const char* r1Label,
                  const char* r2Label, const char* r3Label) {
  dsky::copyField(phase.label, sizeof(phase.label), label);
  dsky::copyField(phase.r1Label, sizeof(phase.r1Label), r1Label);
  dsky::copyField(phase.r2Label, sizeof(phase.r2Label), r2Label);
  dsky::copyField(phase.r3Label, sizeof(phase.r3Label), r3Label);
}

MissionTelemetry computeTelemetryModel(const MissionScenario& scenario) {
  MissionTelemetry telemetry = {};
  telemetry.getMin = scenario.r1;
  telemetry.altitudeKm = 0;
  telemetry.altitudeM = 0;
  telemetry.velocityMs = 0;
  telemetry.distanceKkm = 0;
  telemetry.rangeM = 0;
  telemetry.rangeKm = 0;
  telemetry.burnSec = 0;
  telemetry.deltaVMps = 0;
  telemetry.imuRollDeg = static_cast<int16_t>((scenario.noun * 7) % 360);
  telemetry.imuPitchDeg = static_cast<int16_t>((scenario.noun * 3) % 180);
  telemetry.imuYawDeg = static_cast<int16_t>((scenario.noun * 11) % 360);
  telemetry.radarAltitudeM = 0;
  telemetry.radarRangeM = 0;
  telemetry.propellantPct = 100;
  telemetry.status = scenario.r3;
  telemetry.evaMin = 0;
  telemetry.sampleKg = 0;
  telemetry.mcc = 0;

  switch (scenario.noun) {
    case 20:
      telemetry.altitudeKm = 185;
      telemetry.velocityMs = 7800;
      telemetry.propellantPct = 92;
      break;
    case 21:
      telemetry.burnSec = 348;
      telemetry.deltaVMps = 3200;
      telemetry.propellantPct = 72;
      break;
    case 22:
      telemetry.rangeM = 30;
      telemetry.status = 1;
      telemetry.propellantPct = 71;
      break;
    case 23:
      telemetry.distanceKkm = 120;
      telemetry.mcc = 2;
      telemetry.propellantPct = 70;
      break;
    case 24:
      telemetry.burnSec = 357;
      telemetry.deltaVMps = 900;
      telemetry.propellantPct = 55;
      break;
    case 25:
      telemetry.altitudeKm = 111;
      telemetry.status = 1;
      break;
    case 26:
      telemetry.altitudeKm = 15;
      telemetry.velocityMs = 1680;
      telemetry.radarAltitudeM = 15000;
      telemetry.radarRangeM = 24000;
      telemetry.propellantPct = 44;
      break;
    case 27:
      telemetry.altitudeM = 150;
      telemetry.velocityMs = 50;
      telemetry.radarAltitudeM = 150;
      telemetry.radarRangeM = 320;
      telemetry.propellantPct = 18;
      break;
    case 28:
      telemetry.altitudeM = 0;
      telemetry.status = 1;
      telemetry.radarAltitudeM = 0;
      telemetry.propellantPct = 17;
      break;
    case 29:
      telemetry.evaMin = 151;
      telemetry.sampleKg = 22;
      telemetry.status = 1;
      break;
    case 30:
      telemetry.burnSec = 435;
      telemetry.velocityMs = 1800;
      telemetry.propellantPct = 53;
      break;
    case 31:
      telemetry.rangeKm = 0;
      telemetry.status = 1;
      telemetry.radarRangeM = 0;
      break;
    case 32:
      telemetry.burnSec = 151;
      telemetry.deltaVMps = 1000;
      telemetry.propellantPct = 41;
      break;
    case 33:
      telemetry.distanceKkm = 250;
      telemetry.mcc = 0;
      break;
    case 34:
      telemetry.altitudeKm = 122;
      telemetry.velocityMs = 11000;
      telemetry.propellantPct = 0;
      break;
    case 35:
      telemetry.altitudeKm = 0;
      telemetry.status = 1;
      break;
    default:
      telemetry.altitudeKm = scenario.r2;
      telemetry.velocityMs = scenario.r3;
      break;
  }

  return telemetry;
}

int16_t telemetryValueForLabel(const MissionTelemetry& telemetry,
                               const char* label,
                               int16_t fallback) {
  if (strcmp(label, "GET_MIN") == 0) {
    return telemetry.getMin;
  }
  if (strcmp(label, "ALT_KM") == 0) {
    return telemetry.altitudeKm;
  }
  if (strcmp(label, "ALT_M") == 0) {
    return telemetry.altitudeM;
  }
  if (strcmp(label, "VEL_MS") == 0) {
    return telemetry.velocityMs;
  }
  if (strcmp(label, "DIST_KKM") == 0) {
    return telemetry.distanceKkm;
  }
  if (strcmp(label, "RANGE_M") == 0) {
    return telemetry.rangeM;
  }
  if (strcmp(label, "RANGE_KM") == 0) {
    return telemetry.rangeKm;
  }
  if (strcmp(label, "BURN_SEC") == 0) {
    return telemetry.burnSec;
  }
  if (strcmp(label, "DV_MPS") == 0) {
    return telemetry.deltaVMps;
  }
  if (strcmp(label, "DOCKED") == 0 || strcmp(label, "LANDED") == 0 ||
      strcmp(label, "RECOVERY") == 0 || strcmp(label, "STATUS") == 0 ||
      strcmp(label, "ORBIT") == 0) {
    return telemetry.status;
  }
  if (strcmp(label, "EVA_MIN") == 0) {
    return telemetry.evaMin;
  }
  if (strcmp(label, "SAMPLE_KG") == 0) {
    return telemetry.sampleKg;
  }
  if (strcmp(label, "MCC") == 0) {
    return telemetry.mcc;
  }
  if (strcmp(label, "ALARM") == 0) {
    return fallback;
  }
  if (strcmp(label, "RECYCLE") == 0) {
    return 1;
  }
  if (strcmp(label, "UPLINK") == 0) {
    return 0;
  }
  if (strcmp(label, "STAR") == 0) {
    return 1;
  }
  if (strcmp(label, "RHC") == 0) {
    return 99;
  }
  if (strcmp(label, "MODE") == 0) {
    return fallback;
  }

  return fallback;
}

void syncLamps() {
  state.lampMask = manualLampMask | launchLampMask | missionScenarioLampMask;
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
    setPhaseText("JOYSTICK", "RHC_X", "RHC_Y", "RHC_SW");
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
  } else if (activeMissionScenario() != nullptr) {
    const MissionScenario* scenario = activeMissionScenario();
    const MissionTelemetry telemetry = computeTelemetryModel(*scenario);
    formatAgcWord(state.r1, sizeof(state.r1),
                  agc::Core::fromInt(telemetryValueForLabel(
                      telemetry, scenario->r1Label, scenario->r1)));
    formatAgcWord(state.r2, sizeof(state.r2),
                  agc::Core::fromInt(telemetryValueForLabel(
                      telemetry, scenario->r2Label, scenario->r2)));
    formatAgcWord(state.r3, sizeof(state.r3),
                  agc::Core::fromInt(telemetryValueForLabel(
                      telemetry, scenario->r3Label, scenario->r3)));
  } else {
    setPhaseText("AGC CORE", "COUNT", "A", "Z");
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
          ? (activeMissionScenario() == nullptr
                 ? agcCore.cycles() / 1024UL
                 : static_cast<uint32_t>(activeMissionScenario()->r1) * 60UL)
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

void clearLaunchState() {
  launchMode = LaunchMode::Off;
  launchLampMask = 0;
  lastLaunchEventIndex = -1;
  dsky::copyField(launchPhase, sizeof(launchPhase), "IDLE");
  if (activeMissionScenario() == nullptr) {
    setPhaseText("IDLE", "R1", "R2", "R3");
  }
}

void configureMissionDisplay(const MissionScenario& scenario) {
  setCoreDisplayRegister(agc::Core::kPanelProgram, scenario.program);
  setCoreDisplayRegister(agc::Core::kPanelVerb, scenario.verb);
  setCoreDisplayRegister(agc::Core::kPanelNoun, scenario.noun);
  agcCore.writeErasable(
      agc::Core::kPanelAlarm,
      agc::Core::fromInt(static_cast<int16_t>(scenario.alarm)));
  state.flashVerbNoun = scenario.alarm != 0;
  setPhaseText(scenario.label, scenario.r1Label, scenario.r2Label,
               scenario.r3Label);
}

void emitMissionStatus(Stream& port) {
  const MissionScenario* scenario = activeMissionScenario();
  if (scenario == nullptr) {
    port.println(F("APOLLO11 IDLE"));
    return;
  }

  const MissionTelemetry telemetry = computeTelemetryModel(*scenario);
  port.print(F("APOLLO11 N"));
  printTwoDigits(port, scenario->noun);
  port.print(F(" P"));
  printTwoDigits(port, scenario->program);
  port.print(F(" V"));
  printTwoDigits(port, scenario->verb);
  port.print(F(" "));
  port.print(scenario->label);
  port.print(F(" | "));
  port.print(scenario->r1Label);
  port.print(' ');
  port.print(telemetryValueForLabel(telemetry, scenario->r1Label,
                                    scenario->r1));
  port.print(F(" | "));
  port.print(scenario->r2Label);
  port.print(' ');
  port.print(telemetryValueForLabel(telemetry, scenario->r2Label,
                                    scenario->r2));
  port.print(F(" | "));
  port.print(scenario->r3Label);
  port.print(' ');
  port.print(telemetryValueForLabel(telemetry, scenario->r3Label,
                                    scenario->r3));
  port.print(F(" | ALM "));
  printFourDigits(port, scenario->alarm);
  port.print(F(" | IMU R/P/Y "));
  port.print(telemetry.imuRollDeg);
  port.print('/');
  port.print(telemetry.imuPitchDeg);
  port.print('/');
  port.print(telemetry.imuYawDeg);
  port.print(F(" | RAD ALT/RNG "));
  port.print(telemetry.radarAltitudeM);
  port.print('/');
  port.print(telemetry.radarRangeM);
  port.print(F(" | PROP "));
  port.print(telemetry.propellantPct);
  port.print('%');
  port.println();
}

void printMissionCommandList(Stream& port) {
  port.println(F("Apollo 11 mission commands:"));
  port.println(F("  V37 N00 ENTR  STOP / P00"));
  port.println(F("  V37 N11 ENTR  LAUNCH ASCENT x20"));
  port.println(F("  V37 N12 ENTR  LAUNCH ASCENT x1"));

  for (uint8_t i = 0;
       i < sizeof(kMissionScenarios) / sizeof(kMissionScenarios[0]); ++i) {
    port.print(F("  V37 N"));
    printTwoDigits(port, kMissionScenarios[i].noun);
    port.print(F(" ENTR  "));
    port.println(kMissionScenarios[i].label);
  }
}

void activateMissionScenario(uint8_t noun) {
  const int8_t index = missionScenarioIndexForNoun(noun);
  if (index < 0) {
    raiseAlarm(1106);
    Serial.print(F("Unknown Apollo 11 mission noun: "));
    printTwoDigits(Serial, noun);
    Serial.println();
    return;
  }

  clearLaunchState();
  activeMissionScenarioIndex = index;
  const MissionScenario& scenario = kMissionScenarios[index];
  missionScenarioLampMask = scenario.lampMask;
  configureMissionDisplay(scenario);
  refreshDskyFromCore();
  emitMissionStatus(Serial);
}

void startLaunchSimulation() {
  clearMissionScenario();
  launchMode = LaunchMode::Running;
  launchStartMs = millis();
  lastLaunchMonitorMs = 0;
  lastLaunchEventIndex = -1;
  launchSecond = kLaunchStartSecond;
  computeLaunchTelemetry(launchSecond);
  setPhaseText(launchPhase, "T_SEC", "ALT_KM", "VEL_MS");
  configureLaunchDisplay();
  refreshDskyFromCore();

  Serial.print(F("Apollo 11 launch simulation started at x"));
  Serial.println(launchTimeScale);
  emitLaunchStatus(Serial);
}

void stopLaunchSimulation(bool resetPanel) {
  clearLaunchState();

  if (resetPanel) {
    setCoreDisplayRegister(agc::Core::kPanelProgram, 0);
    setCoreDisplayRegister(agc::Core::kPanelVerb, 16);
    setCoreDisplayRegister(agc::Core::kPanelNoun, 36);
  }

  setPhaseText("IDLE", "R1", "R2", "R3");
  refreshDskyFromCore();
  Serial.println(F("Apollo 11 launch simulation stopped."));
}

void stopApollo11MissionProgram(bool resetPanel) {
  clearMissionScenario();
  clearLaunchState();

  if (resetPanel) {
    setCoreDisplayRegister(agc::Core::kPanelProgram, 0);
    setCoreDisplayRegister(agc::Core::kPanelVerb, 16);
    setCoreDisplayRegister(agc::Core::kPanelNoun, 36);
  }

  setPhaseText("IDLE", "R1", "R2", "R3");
  clearAlarm();
  refreshDskyFromCore();
  Serial.println(F("Apollo 11 mission program stopped."));
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
  setPhaseText(launchPhase, "T_SEC", "ALT_KM", "VEL_MS");
  const bool reachedOrbit = launchSecond >= kLaunchOrbitInsertionSecond;
  if (reachedOrbit) {
    launchMode = LaunchMode::Complete;
    launchLampMask = dsky::kLampProg | dsky::kLampTracker;
    dsky::copyField(launchPhase, sizeof(launchPhase), "PARKING ORBIT");
    setPhaseText(launchPhase, "T_SEC", "ALT_KM", "VEL_MS");
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

void executeApollo11MissionNoun(uint8_t noun) {
  if (noun == 0) {
    stopApollo11MissionProgram(true);
    return;
  }

  if (noun == 11) {
    setLaunchTimeScale(kLaunchDefaultTimeScale);
    startLaunchSimulation();
    return;
  }

  if (noun == 12) {
    setLaunchTimeScale(1);
    startLaunchSimulation();
    return;
  }

  activateMissionScenario(noun);
}

void handleDskyEnterCommand() {
  const uint8_t verb =
      clampDisplayCode(agcCore.readErasable(agc::Core::kPanelVerb));
  const uint8_t noun =
      clampDisplayCode(agcCore.readErasable(agc::Core::kPanelNoun));

  if (verb != 37) {
    return;
  }

  executeApollo11MissionNoun(noun);
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

void emitPhaseFrame(Stream& port) {
  char line[dsky::kLineBufferSize];
  if (!dsky::formatPhaseLine(phase, line, sizeof(line))) {
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
  port.print(static_cast<unsigned int>(agcCore.fault()));
  port.print(F(" EXT="));
  port.print(agcCore.lastInstructionExtended() ? 1 : 0);
  port.print(F(" IRQ="));
  port.print(agcCore.pendingInterruptMask(), BIN);
  port.print(F(" CH10="));
  port.println(agcCore.readChannel(agc::Core::kChannelDSKY), OCT);
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
  } else if (activeMissionScenario() != nullptr) {
    const MissionScenario* scenario = activeMissionScenario();
    const MissionTelemetry telemetry = computeTelemetryModel(*scenario);
    port.print(F(" | MSN N"));
    printTwoDigits(port, scenario->noun);
    port.print(' ');
    port.print(scenario->label);
    port.print(F(" | "));
    port.print(scenario->r1Label);
    port.print(' ');
    port.print(telemetryValueForLabel(telemetry, scenario->r1Label,
                                      scenario->r1));
    port.print(F(" | "));
    port.print(scenario->r2Label);
    port.print(' ');
    port.print(telemetryValueForLabel(telemetry, scenario->r2Label,
                                      scenario->r2));
    port.print(F(" | "));
    port.print(scenario->r3Label);
    port.print(' ');
    port.print(telemetryValueForLabel(telemetry, scenario->r3Label,
                                      scenario->r3));
    port.print(F(" | IMU "));
    port.print(telemetry.imuRollDeg);
    port.print('/');
    port.print(telemetry.imuPitchDeg);
    port.print('/');
    port.print(telemetry.imuYawDeg);
    port.print(F(" PROP "));
    port.print(telemetry.propellantPct);
    port.print('%');
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
    Serial.println(F("  ROPE,INFO ROPE,LOAD"));
    Serial.println(F("  CHAN,<octal> CHAN,<octal>,<octal> IRQ,<0-7>"));
    Serial.println(F("  PINBALL,<noun> PINBALL,LIST"));
    Serial.println(F("  PEEK,<octal-address>"));
    Serial.println(F("  POKE,<octal-address>,<octal-word>"));
    Serial.println(F("  JOY JOYCAL"));
    Serial.println(F("  STATUS"));
    Serial.println(F("  APOLLO11,LIST APOLLO11,STATUS APOLLO11,STOP"));
    Serial.println(F("  APOLLO11,<noun> or MISSION,<noun>"));
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
    clearMissionScenario();
    clearLaunchState();
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

  if (strcmp(line, "ROPE,INFO") == 0) {
    Serial.print(F("ROPE NAME="));
    Serial.print(embedded_rope::kImage.name == nullptr
                     ? "none"
                     : embedded_rope::kImage.name);
    Serial.print(F(" BANKS="));
    Serial.print(static_cast<unsigned int>(embedded_rope::kImage.bankCount));
    Serial.print(F(" WORDS="));
    Serial.println(static_cast<unsigned long>(embedded_rope::kImage.bankCount) *
                   agc::Core::kFixedBankSize);
    return;
  }

  if (strcmp(line, "ROPE,LOAD") == 0) {
    if (embedded_rope::kImage.words == nullptr ||
        embedded_rope::kImage.bankCount == 0) {
      Serial.println(F("ROPE no embedded image. Generate rope_image.h first."));
      return;
    }

    if (agcCore.loadRopeImage(embedded_rope::kImage)) {
      agcCore.start();
      clearMissionScenario();
      clearLaunchState();
      refreshDskyFromCore();
      Serial.println(F("ROPE loaded into fixed memory."));
    } else {
      Serial.println(F("ROPE load failed."));
    }
    return;
  }

  if (strncmp(line, "CHAN,", 5) == 0) {
    char* channelText = line + 5;
    char* valueText = strchr(channelText, ',');
    const uint8_t channel = static_cast<uint8_t>(parseOctal(channelText));
    if (valueText != nullptr) {
      *valueText = '\0';
      valueText++;
      agcCore.writeChannel(channel, parseOctal(valueText));
    }
    Serial.print(F("CHAN,"));
    Serial.print(channel, OCT);
    Serial.print(F(","));
    Serial.println(agcCore.readChannel(channel), OCT);
    return;
  }

  if (strncmp(line, "IRQ,", 4) == 0) {
    const uint8_t irq =
        static_cast<uint8_t>(strtoul(line + 4, nullptr, 10));
    agcCore.requestInterrupt(irq);
    Serial.print(F("IRQ pending mask="));
    Serial.println(agcCore.pendingInterruptMask(), BIN);
    return;
  }

  if (strcmp(line, "PINBALL,LIST") == 0) {
    for (uint8_t i = 0; i < sizeof(pinball::kNouns) / sizeof(pinball::kNouns[0]);
         ++i) {
      Serial.print(F("N"));
      printTwoDigits(Serial, pinball::kNouns[i].noun);
      Serial.print(F(" "));
      Serial.print(pinball::kNouns[i].name);
      Serial.print(F(" | "));
      Serial.println(pinball::kNouns[i].components);
    }
    return;
  }

  if (strncmp(line, "PINBALL,", 8) == 0) {
    const uint8_t noun =
        static_cast<uint8_t>(strtoul(line + 8, nullptr, 10));
    const pinball::NounInfo* info = pinball::find(noun);
    if (info == nullptr) {
      Serial.print(F("PINBALL N"));
      printTwoDigits(Serial, noun);
      Serial.println(F(" not mapped yet."));
    } else {
      Serial.print(F("PINBALL N"));
      printTwoDigits(Serial, info->noun);
      Serial.print(F(" "));
      Serial.print(info->name);
      Serial.print(F(" | "));
      Serial.println(info->components);
    }
    return;
  }

  if (strcmp(line, "APOLLO11,LIST") == 0 ||
      strcmp(line, "MISSION,LIST") == 0) {
    printMissionCommandList(Serial);
    return;
  }

  if (strcmp(line, "APOLLO11,STATUS") == 0 ||
      strcmp(line, "MISSION,STATUS") == 0) {
    if (launchMode != LaunchMode::Off) {
      emitLaunchStatus(Serial);
    } else {
      emitMissionStatus(Serial);
    }
    return;
  }

  if (strcmp(line, "APOLLO11,STOP") == 0 ||
      strcmp(line, "MISSION,STOP") == 0) {
    stopApollo11MissionProgram(true);
    return;
  }

  if (strncmp(line, "APOLLO11,", 9) == 0) {
    executeApollo11MissionNoun(
        static_cast<uint8_t>(strtoul(line + 9, nullptr, 10)));
    return;
  }

  if (strncmp(line, "MISSION,", 8) == 0) {
    executeApollo11MissionNoun(
        static_cast<uint8_t>(strtoul(line + 8, nullptr, 10)));
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
  dsky::initPhase(&phase);
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
    emitPhaseFrame(panelSerial);
    if (usbWantsRawState()) {
      emitStateFrame(Serial);
      emitPhaseFrame(Serial);
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
