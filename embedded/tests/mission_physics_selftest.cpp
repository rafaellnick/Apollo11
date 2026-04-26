#include "../shared/mission_physics.h"

#include <assert.h>
#include <stdint.h>

namespace {

void assertBetween(int16_t value, int16_t minimum, int16_t maximum) {
  assert(value >= minimum);
  assert(value <= maximum);
}

}  // namespace

int main() {
  const mission_physics::AscentState countdown =
      mission_physics::computeAscentState(-5);
  assert(countdown.altitudeKm == 0);
  assert(countdown.velocityMs == 0);
  assert(countdown.propellantPct == 100);

  const mission_physics::AscentState maxQ =
      mission_physics::computeAscentState(83);
  assert(maxQ.altitudeKm == 14);
  assert(maxQ.velocityMs == 520);

  const mission_physics::AscentState ascentMid =
      mission_physics::computeAscentState(120);
  assertBetween(ascentMid.altitudeKm, 15, 64);
  assertBetween(ascentMid.velocityMs, 521, 2699);

  const mission_physics::AscentState parking =
      mission_physics::computeAscentState(705);
  assert(parking.altitudeKm == 185);
  assert(parking.velocityMs == 7800);
  assert(parking.propellantPct == 16);

  const mission_physics::CoastState outbound =
      mission_physics::computeCoastState(
          mission_physics::CoastLeg::Translunar, 720);
  assertBetween(outbound.distanceKkm, 80, 140);
  assert(outbound.correctionNumber == 2);
  assertBetween(outbound.phaseDeg, 1, 90);

  const mission_physics::CoastState homebound =
      mission_physics::computeCoastState(
          mission_physics::CoastLeg::Transearth, 9000);
  assertBetween(homebound.distanceKkm, 180, 260);
  assert(homebound.correctionNumber == 0);
  assertBetween(homebound.phaseDeg, 180, 360);

  const mission_physics::OrbitState earthOrbit =
      mission_physics::computeOrbitState(mission_physics::OrbitBody::Earth, 0);
  assert(earthOrbit.altitudeKm == 185);
  assert(earthOrbit.velocityMs == 7800);
  assert(earthOrbit.phaseDeg == 0);

  const mission_physics::OrbitState earthQuarterOrbit =
      mission_physics::computeOrbitState(mission_physics::OrbitBody::Earth, 22);
  assertBetween(earthQuarterOrbit.altitudeKm, 188, 189);
  assertBetween(earthQuarterOrbit.velocityMs, 7788, 7790);
  assert(earthQuarterOrbit.phaseDeg == 90);

  const mission_physics::OrbitState lunarOrbit =
      mission_physics::computeOrbitState(mission_physics::OrbitBody::Moon, 0);
  assert(lunarOrbit.altitudeKm == 111);
  assert(lunarOrbit.velocityMs == 1630);

  const mission_physics::DescentState descentStart =
      mission_physics::computeDescentState(6120);
  assert(descentStart.altitudeM == 15000);
  assert(descentStart.velocityMs == 1680);
  assert(descentStart.radarRangeM == 24000);
  assert(descentStart.propellantPct == 44);

  const mission_physics::DescentState descentFinal =
      mission_physics::computeDescentState(6150);
  assert(descentFinal.altitudeM == 150);
  assert(descentFinal.velocityMs == 50);
  assert(descentFinal.radarRangeM == 320);
  assert(descentFinal.propellantPct == 18);

  const mission_physics::DescentState touchdown =
      mission_physics::computeDescentState(6152);
  assert(touchdown.altitudeM == 0);
  assert(touchdown.velocityMs == 0);
  assert(touchdown.propellantPct == 17);

  const mission_physics::ReentryState entry =
      mission_physics::computeReentryState(11700);
  assert(entry.altitudeKm == 122);
  assert(entry.velocityMs == 11000);
  assert(entry.status == 0);

  const mission_physics::ReentryState entryMid =
      mission_physics::computeReentryState(11736);
  assertBetween(entryMid.altitudeKm, 1, 121);
  assert(entryMid.gLoadCentiG > 100);

  const mission_physics::ReentryState splashdown =
      mission_physics::computeReentryState(11773);
  assert(splashdown.altitudeKm == 0);
  assert(splashdown.status == 1);

  const mission_physics::MissionState earthParking =
      mission_physics::computeMissionState(20, 12);
  assert(earthParking.altitudeKm == 185);
  assert(earthParking.velocityMs == 7800);
  assert(earthParking.status == 1);

  const mission_physics::MissionState tli =
      mission_physics::computeMissionState(21, 164);
  assert(tli.burnSec == 348);
  assert(tli.deltaVMps == 3200);

  const mission_physics::MissionState poweredDescent =
      mission_physics::computeMissionState(26, 6120);
  assert(poweredDescent.altitudeKm == 15);
  assert(poweredDescent.radarAltitudeM == 15000);
  assert(poweredDescent.radarRangeM == 24000);

  const mission_physics::MissionState finalApproach =
      mission_physics::computeMissionState(27, 6150);
  assert(finalApproach.altitudeM == 150);
  assert(finalApproach.velocityMs == 50);
  assert(finalApproach.radarRangeM == 320);

  const mission_physics::MissionState lunarAscent =
      mission_physics::computeMissionState(30, 7460);
  assert(lunarAscent.burnSec == 435);
  assert(lunarAscent.velocityMs == 1800);

  const mission_physics::MissionState transearthCoast =
      mission_physics::computeMissionState(33, 9000);
  assertBetween(transearthCoast.distanceKkm, 180, 260);
  assert(transearthCoast.mcc == 0);

  const mission_physics::MissionState entryInterface =
      mission_physics::computeMissionState(34, 11700);
  assert(entryInterface.altitudeKm == 122);
  assert(entryInterface.velocityMs == 11000);
  assert(entryInterface.propellantPct == 0);

  const mission_physics::MissionState recovery =
      mission_physics::computeMissionState(35, 11773);
  assert(recovery.altitudeKm == 0);
  assert(recovery.status == 1);

  return 0;
}
