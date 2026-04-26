#ifndef APOLLO11_EMBEDDED_MISSION_PHYSICS_H
#define APOLLO11_EMBEDDED_MISSION_PHYSICS_H

#include <stdint.h>

namespace mission_physics {

enum class CoastLeg : uint8_t {
  Translunar = 0,
  Transearth,
};

enum class OrbitBody : uint8_t {
  Earth = 0,
  Moon,
};

struct AscentState {
  int16_t altitudeKm;
  int16_t velocityMs;
  int16_t downrangeKm;
  int16_t propellantPct;
};

struct CoastState {
  int16_t distanceKkm;
  int16_t velocityMs;
  int16_t correctionNumber;
  int16_t phaseDeg;
};

struct OrbitState {
  int16_t altitudeKm;
  int16_t velocityMs;
  int16_t phaseDeg;
  int16_t radialRateMs;
};

struct DescentState {
  int16_t altitudeM;
  int16_t velocityMs;
  int16_t radarRangeM;
  int16_t propellantPct;
  int16_t pitchDeg;
};

struct ReentryState {
  int16_t altitudeKm;
  int16_t velocityMs;
  int16_t downrangeKm;
  int16_t gLoadCentiG;
  int16_t status;
};

struct MissionState {
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

struct AscentKeyframe {
  int16_t second;
  int16_t altitudeKm;
  int16_t velocityMs;
  int16_t downrangeKm;
  int16_t propellantPct;
};

inline int32_t clampInt32(int32_t value, int32_t minimum, int32_t maximum) {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

inline int16_t clampInt16(int32_t value, int16_t minimum, int16_t maximum) {
  return static_cast<int16_t>(clampInt32(value, minimum, maximum));
}

inline int32_t progressPermille(int32_t value, int32_t start, int32_t end) {
  if (end <= start) {
    return value >= end ? 1000 : 0;
  }

  if (value <= start) {
    return 0;
  }

  if (value >= end) {
    return 1000;
  }

  return ((value - start) * 1000L) / (end - start);
}

inline int32_t smoothStepPermille(int32_t t) {
  t = clampInt32(t, 0, 1000);
  const int32_t t2 = (t * t + 500L) / 1000L;
  const int32_t t3 = (t2 * t + 500L) / 1000L;
  return clampInt32(3L * t2 - 2L * t3, 0, 1000);
}

inline int32_t easeOutQuadPermille(int32_t t) {
  t = clampInt32(t, 0, 1000);
  return clampInt32((2000L * t - t * t) / 1000L, 0, 1000);
}

inline int32_t lerpPermille(int32_t start, int32_t end, int32_t t) {
  t = clampInt32(t, 0, 1000);
  const int32_t delta = end - start;
  const int32_t adjust = delta >= 0 ? 500L : -500L;
  return start + (delta * t + adjust) / 1000L;
}

inline int32_t triangularWavePermille(int32_t t) {
  t %= 1000L;
  if (t < 0) {
    t += 1000L;
  }

  return t <= 500L ? t * 2L : (1000L - t) * 2L;
}

inline int16_t wrapDegrees(int32_t degrees) {
  degrees %= 360L;
  if (degrees < 0) {
    degrees += 360L;
  }

  return static_cast<int16_t>(degrees);
}

inline int16_t burnSecondsForDeltaV(int16_t deltaVMps,
                                    int16_t accelerationCentiMs2) {
  if (deltaVMps <= 0 || accelerationCentiMs2 <= 0) {
    return 0;
  }

  return clampInt16((static_cast<int32_t>(deltaVMps) * 100L +
                     accelerationCentiMs2 / 2) /
                        accelerationCentiMs2,
                    0, 32767);
}

inline AscentState interpolateAscent(const AscentKeyframe& start,
                                     const AscentKeyframe& end,
                                     int16_t second) {
  const int32_t t = smoothStepPermille(
      progressPermille(second, start.second, end.second));

  AscentState state = {};
  state.altitudeKm = clampInt16(
      lerpPermille(start.altitudeKm, end.altitudeKm, t), 0, 32767);
  state.velocityMs = clampInt16(
      lerpPermille(start.velocityMs, end.velocityMs, t), 0, 32767);
  state.downrangeKm = clampInt16(
      lerpPermille(start.downrangeKm, end.downrangeKm, t), 0, 32767);
  state.propellantPct = clampInt16(
      lerpPermille(start.propellantPct, end.propellantPct, t), 0, 100);
  return state;
}

inline AscentState computeAscentState(int16_t missionSecond) {
  static const AscentKeyframe kAscent[] = {
      {0, 0, 0, 0, 100},
      {83, 14, 520, 23, 82},
      {164, 65, 2700, 160, 61},
      {197, 95, 3200, 280, 58},
      {462, 176, 5291, 1200, 36},
      {555, 187, 7049, 1800, 27},
      {705, 185, 7800, 2520, 16},
  };

  if (missionSecond <= kAscent[0].second) {
    return AscentState{0, 0, 0, 100};
  }

  for (uint8_t i = 1; i < sizeof(kAscent) / sizeof(kAscent[0]); ++i) {
    if (missionSecond <= kAscent[i].second) {
      return interpolateAscent(kAscent[i - 1], kAscent[i], missionSecond);
    }
  }

  const AscentKeyframe& final = kAscent[sizeof(kAscent) / sizeof(kAscent[0]) -
                                       1];
  return AscentState{final.altitudeKm, final.velocityMs, final.downrangeKm,
                     final.propellantPct};
}

inline CoastState computeCoastState(CoastLeg leg, int16_t getMin) {
  CoastState state = {};

  if (leg == CoastLeg::Translunar) {
    const int32_t t = progressPermille(getMin, 164, 4470);
    const int32_t outbound = easeOutQuadPermille(t);
    state.distanceKkm =
        clampInt16(lerpPermille(7, 384, outbound), 0, 400);
    state.velocityMs =
        clampInt16(lerpPermille(10800, 900, smoothStepPermille(t)), 0,
                   32767);
    state.correctionNumber =
        t < 80 ? 1 : (t < 260 ? 2 : (t < 600 ? 3 : 0));
    state.phaseDeg = wrapDegrees((t * 360L) / 1000L);
    return state;
  }

  const int32_t t = progressPermille(getMin, 8080, 11700);
  const int32_t inbound = easeOutQuadPermille(t);
  state.distanceKkm =
      clampInt16(384 - lerpPermille(0, 384, inbound), 0, 400);
  state.velocityMs =
      clampInt16(lerpPermille(1000, 10800, smoothStepPermille(t)), 0,
                 32767);
  state.correctionNumber = t < 180 ? 1 : 0;
  state.phaseDeg = wrapDegrees(180L + (t * 180L) / 1000L);
  return state;
}

inline OrbitState computeOrbitState(OrbitBody body, int16_t orbitMinutes) {
  const int16_t periodMin = body == OrbitBody::Earth ? 88 : 118;
  const int16_t baseAltitudeKm = body == OrbitBody::Earth ? 185 : 111;
  const int16_t altitudeSwingKm = body == OrbitBody::Earth ? 4 : 6;
  const int16_t baseVelocityMs = body == OrbitBody::Earth ? 7800 : 1630;
  const int16_t velocitySwingMs = body == OrbitBody::Earth ? 12 : 8;

  int32_t minuteInOrbit = orbitMinutes;
  if (minuteInOrbit < 0) {
    minuteInOrbit = 0;
  }

  const int32_t orbitT =
      ((minuteInOrbit % periodMin) * 1000L) / periodMin;
  const int32_t radial =
      triangularWavePermille(orbitT + 250L) - 500L;

  OrbitState state = {};
  state.altitudeKm =
      clampInt16(baseAltitudeKm + (radial * altitudeSwingKm) / 500L, 0,
                 32767);
  state.velocityMs =
      clampInt16(baseVelocityMs - (radial * velocitySwingMs) / 500L, 0,
                 32767);
  state.phaseDeg = wrapDegrees((minuteInOrbit * 360L) / periodMin);
  state.radialRateMs = clampInt16((radial * 2L) / 25L, -100, 100);
  return state;
}

inline DescentState computeDescentState(int16_t getMin) {
  DescentState state = {};

  if (getMin <= 6150) {
    const int32_t t = smoothStepPermille(progressPermille(getMin, 6120, 6150));
    state.altitudeM =
        clampInt16(lerpPermille(15000, 150, t), 0, 32767);
    state.velocityMs =
        clampInt16(lerpPermille(1680, 50, t), 0, 32767);
    const int16_t horizontalRangeM =
        clampInt16(lerpPermille(9000, 170, t), 0, 32767);
    state.radarRangeM =
        clampInt16(static_cast<int32_t>(state.altitudeM) + horizontalRangeM,
                   0, 32767);
    state.propellantPct =
        clampInt16(lerpPermille(44, 18, t), 0, 100);
    state.pitchDeg = clampInt16(lerpPermille(70, 5, t), -90, 90);
    return state;
  }

  const int32_t t = progressPermille(getMin, 6150, 6152);
  state.altitudeM = clampInt16(lerpPermille(150, 0, t), 0, 32767);
  state.velocityMs = clampInt16(lerpPermille(50, 0, t), 0, 32767);
  state.radarRangeM = clampInt16(lerpPermille(320, 0, t), 0, 32767);
  state.propellantPct = clampInt16(lerpPermille(18, 17, t), 0, 100);
  state.pitchDeg = clampInt16(lerpPermille(5, 0, t), -90, 90);
  return state;
}

inline ReentryState computeReentryState(int16_t getMin) {
  const int32_t t = smoothStepPermille(progressPermille(getMin, 11700, 11773));
  const int32_t peakG =
      triangularWavePermille(progressPermille(getMin, 11700, 11773));

  ReentryState state = {};
  state.altitudeKm = clampInt16(lerpPermille(122, 0, t), 0, 32767);
  state.velocityMs = clampInt16(lerpPermille(11000, 150, t), 0, 32767);
  state.downrangeKm = clampInt16(lerpPermille(0, 2100, t), 0, 32767);
  state.gLoadCentiG =
      clampInt16(100 + (peakG * 520L) / 1000L, 0, 1000);
  state.status = getMin >= 11773 ? 1 : 0;
  return state;
}

inline MissionState makeMissionState(int16_t getMin) {
  MissionState state = {};
  state.getMin = getMin;
  state.propellantPct = 100;
  return state;
}

inline void setDefaultAttitude(MissionState* state, uint8_t noun) {
  state->imuRollDeg = wrapDegrees(static_cast<int32_t>(state->getMin) * 2L +
                                  static_cast<int32_t>(noun) * 11L);
  state->imuPitchDeg = clampInt16(
      (static_cast<int32_t>(state->getMin) + noun * 5L) % 41L - 20L, -90,
      90);
  state->imuYawDeg = wrapDegrees(static_cast<int32_t>(state->getMin) +
                                 static_cast<int32_t>(noun) * 13L);
}

inline bool isModeledMissionNoun(uint8_t noun) {
  return noun >= 20 && noun <= 35;
}

inline MissionState computeMissionState(uint8_t noun, int16_t getMin) {
  MissionState state = makeMissionState(getMin);
  setDefaultAttitude(&state, noun);

  switch (noun) {
    case 20: {
      const OrbitState orbit =
          computeOrbitState(OrbitBody::Earth,
                            static_cast<int16_t>(getMin - 12));
      state.altitudeKm = orbit.altitudeKm;
      state.velocityMs = orbit.velocityMs;
      state.propellantPct = 92;
      state.status = 1;
      state.imuRollDeg = orbit.phaseDeg;
      state.imuPitchDeg = 0;
      state.imuYawDeg = 90;
      break;
    }
    case 21:
      state.deltaVMps = 3200;
      state.burnSec = burnSecondsForDeltaV(state.deltaVMps, 920);
      state.propellantPct = 72;
      state.imuPitchDeg = 32;
      state.imuYawDeg = 72;
      break;
    case 22:
      state.rangeM = 30;
      state.status = 1;
      state.propellantPct = 71;
      state.imuPitchDeg = 0;
      break;
    case 23: {
      const CoastState coast =
          computeCoastState(CoastLeg::Translunar, getMin);
      state.distanceKkm = coast.distanceKkm;
      state.velocityMs = coast.velocityMs;
      state.mcc = coast.correctionNumber;
      state.propellantPct = 70;
      state.imuRollDeg = coast.phaseDeg;
      state.imuPitchDeg = 12;
      break;
    }
    case 24:
      state.deltaVMps = 900;
      state.burnSec = burnSecondsForDeltaV(state.deltaVMps, 252);
      state.propellantPct = 55;
      state.imuPitchDeg = -18;
      break;
    case 25: {
      const OrbitState orbit = computeOrbitState(
          OrbitBody::Moon, static_cast<int16_t>(getMin - 4470));
      state.altitudeKm = orbit.altitudeKm;
      state.velocityMs = orbit.velocityMs;
      state.propellantPct = 55;
      state.status = 1;
      state.imuRollDeg = orbit.phaseDeg;
      state.imuPitchDeg = 0;
      state.imuYawDeg = 270;
      break;
    }
    case 26: {
      const DescentState descent = computeDescentState(getMin);
      state.altitudeM = descent.altitudeM;
      state.altitudeKm =
          clampInt16((static_cast<int32_t>(descent.altitudeM) + 500L) /
                         1000L,
                     0, 32767);
      state.velocityMs = descent.velocityMs;
      state.radarAltitudeM = descent.altitudeM;
      state.radarRangeM = descent.radarRangeM;
      state.propellantPct = descent.propellantPct;
      state.imuPitchDeg = descent.pitchDeg;
      state.imuYawDeg = 90;
      break;
    }
    case 27: {
      const DescentState descent = computeDescentState(getMin);
      state.altitudeM = descent.altitudeM;
      state.velocityMs = descent.velocityMs;
      state.radarAltitudeM = descent.altitudeM;
      state.radarRangeM = descent.radarRangeM;
      state.propellantPct = descent.propellantPct;
      state.imuPitchDeg = descent.pitchDeg;
      state.imuYawDeg = 90;
      break;
    }
    case 28:
      state.status = 1;
      state.propellantPct = 17;
      state.imuPitchDeg = 0;
      state.imuYawDeg = 90;
      break;
    case 29:
      state.evaMin = 151;
      state.sampleKg = 22;
      state.status = 1;
      state.propellantPct = 17;
      state.imuPitchDeg = 0;
      break;
    case 30:
      state.burnSec = 435;
      state.velocityMs = 1800;
      state.propellantPct = 53;
      state.imuPitchDeg = 72;
      state.imuYawDeg = 90;
      break;
    case 31:
      state.rangeKm = 0;
      state.radarRangeM = 0;
      state.status = 1;
      state.propellantPct = 52;
      state.imuPitchDeg = 0;
      break;
    case 32:
      state.deltaVMps = 1000;
      state.burnSec = burnSecondsForDeltaV(state.deltaVMps, 662);
      state.propellantPct = 41;
      state.imuPitchDeg = 24;
      break;
    case 33: {
      const CoastState coast =
          computeCoastState(CoastLeg::Transearth, getMin);
      state.distanceKkm = coast.distanceKkm;
      state.velocityMs = coast.velocityMs;
      state.mcc = coast.correctionNumber;
      state.propellantPct = 41;
      state.imuRollDeg = coast.phaseDeg;
      state.imuPitchDeg = -10;
      break;
    }
    case 34: {
      const ReentryState entry = computeReentryState(getMin);
      state.altitudeKm = entry.altitudeKm;
      state.velocityMs = entry.velocityMs;
      state.propellantPct = 0;
      state.status = entry.status;
      state.imuRollDeg = wrapDegrees((entry.gLoadCentiG - 100) / 2);
      state.imuPitchDeg = -6;
      state.imuYawDeg = 0;
      break;
    }
    case 35: {
      const ReentryState entry = computeReentryState(getMin);
      state.altitudeKm = entry.altitudeKm;
      state.velocityMs = entry.velocityMs;
      state.propellantPct = 0;
      state.status = 1;
      state.imuRollDeg = 0;
      state.imuPitchDeg = 0;
      state.imuYawDeg = 0;
      break;
    }
    default:
      break;
  }

  return state;
}

}  // namespace mission_physics

#endif
