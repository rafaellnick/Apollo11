#include "../shared/agc_core.h"
#include "../shared/agc_machine_timing.h"

#include <stdio.h>
#include <stdint.h>

#undef assert
#define assert(expr)                                                          \
  do {                                                                        \
    if (!(expr)) {                                                            \
      fprintf(stderr, "assert failed at %s:%d: %s\n", __FILE__, __LINE__,    \
              #expr);                                                         \
      return __LINE__;                                                        \
    }                                                                         \
  } while (0)

int main() {
  agc::Core core;
  core.start();
  core.runFor(4);

  assert(core.readErasable(agc::Core::kPanelCounter) == agc::Core::fromInt(1));
  assert(core.getZ() == agc::Core::kBootAddress);

  core.runFor(40);
  assert(agc::Core::toInt(core.readErasable(agc::Core::kPanelCounter)) == 11);

  assert(agc::Core::addOnesComplement(agc::Core::fromInt(1),
                                      agc::Core::fromInt(-1)) == 077777);
  assert(agc::Core::toInt(agc::Core::fromInt(-42)) == -42);
  assert(agc::Core::toInt(077777) == 0);
  assert(agc::Core::interruptVectorAddress(
             agc::Core::kInterruptDownrupt) == 04040);
  assert(agc::Core::interruptVectorAddress(
             agc::Core::kInterruptHandrupt) == 04050);
  assert(agc::Core::kRegINLINK == 00045);
  assert(agc::Core::kRegRNRAD == 00046);
  assert(agc::Core::kRegOUTLINK == 00057);

  core.writeChannel(0010, 012345);
  assert(core.readChannel(0010) == 012345);
  assert(core.readOutputChannel10(0) == 0);
  core.writeChannel(0777, 076543);
  assert(core.readChannel(0777) == 076543);
  core.writeChannel(01001, 000123);
  assert(core.readChannel(0001) == 000123);
  assert(core.readErasable(agc::Core::kRegL) == 000123);
  core.writeChannel(01002, 000456);
  assert(core.readChannel(0002) == 000456);
  assert(core.readErasable(agc::Core::kRegQ) == 000456);
  assert(core.readChannel(0030) == 037777);
  assert(core.readChannel(0031) == 077777);
  assert(core.readChannel(0032) == 077777);
  assert(core.readChannel(0033) == 077777);

  agc::Core mapCore;
  mapCore.writeErasable(agc::Core::kRegEBANK, 03400);
  mapCore.writeErasable(03400, 001234);
  assert(mapCore.read(01400) == 001234);
  mapCore.writeErasable(agc::Core::kRegFBANK, 012000);
  assert(mapCore.readErasable(agc::Core::kRegBBANK) == 012007);
  mapCore.writeErasable(agc::Core::kRegBBANK, 006002);
  assert(mapCore.readErasable(agc::Core::kRegEBANK) == 001000);
  assert(mapCore.readErasable(agc::Core::kRegFBANK) == 006000);
  mapCore.writeErasable(agc::Core::kRegFBANK, 012000);
  mapCore.writeFixedBank(05, 0, 056000);
  assert(mapCore.read(02000) == 056000);
  mapCore.writeErasable(agc::Core::kRegFBANK, 060000);
  mapCore.writeChannel(0007, 000100);
  mapCore.writeFixedBank(040, 0, 012345);
  assert(mapCore.read(02000) == 012345);
  mapCore.writeFixedBank(03, 0, 067000);
  assert(mapCore.read(06000) == 067000);

  core.writeErasable(agc::Core::kRegZERO, 012345);
  assert(core.readErasable(agc::Core::kRegZERO) == 0);

  core.writeErasable(agc::Core::kRegCYR, 000001);
  assert(core.readErasable(agc::Core::kRegCYR) == 040000);
  assert(core.readErasable(agc::Core::kRegCYR) == 020000);

  core.writeErasable(agc::Core::kRegCYL, 040000);
  assert(core.readErasable(agc::Core::kRegCYL) == 000001);
  assert(core.readErasable(agc::Core::kRegCYL) == 000002);

  core.writeErasable(agc::Core::kRegSR, 040002);
  assert(core.readErasable(agc::Core::kRegSR) == 060001);
  assert(core.readErasable(agc::Core::kRegSR) == 070000);

  core.writeErasable(agc::Core::kRegEDOP, 077777);
  assert(core.readErasable(agc::Core::kRegEDOP) == 000177);
  assert(core.readErasable(agc::Core::kRegEDOP) == 000000);

  agc::Core counterCore;
  counterCore.writeErasable(agc::Core::kRegTIME3, 037777);
  assert(counterCore.counterPinc(agc::Core::kRegTIME3,
                                 agc::Core::kInterruptT3Rupt));
  assert(counterCore.readErasable(agc::Core::kRegTIME3) == 0);
  assert((counterCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptT3Rupt)) != 0);
  counterCore.writeErasable(agc::Core::kRegTIME6, 000001);
  assert(counterCore.counterDinc(agc::Core::kRegTIME6,
                                 agc::Core::kInterruptT6Rupt));
  assert((counterCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptT6Rupt)) != 0);
  counterCore.writeErasable(agc::Core::kRegINLINK, 040000);
  assert(counterCore.counterShift(agc::Core::kRegINLINK, true,
                                  agc::Core::kInterruptUprupt));
  assert(counterCore.readErasable(agc::Core::kRegINLINK) == 000001);
  assert((counterCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptUprupt)) != 0);

  agc::Core uplinkCore;
  uplinkCore.receiveUplinkWord(012345);
  assert(uplinkCore.readErasable(agc::Core::kRegINLINK) == 012345);
  assert((uplinkCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptUprupt)) != 0);
  uplinkCore.receiveUplinkWord(023456);
  assert((uplinkCore.readChannel(0033) &
          agc::Core::kChannel33UplinkTooFast) == 0);

  agc::Core physicalUplinkCore;
  agc::MachineTiming physicalUplinkTiming;
  const uint16_t physicalUplinkWord =
      agc::Core::appendOddParityBit(012345);
  uint16_t strippedUplinkWord = 0;
  assert(agc::Core::hasOddParity16(physicalUplinkWord));
  assert(agc::Core::physicalWordToData(physicalUplinkWord,
                                       &strippedUplinkWord));
  assert(strippedUplinkWord == 012345);
  assert(physicalUplinkTiming.receivePhysicalUplinkWord(physicalUplinkCore,
                                                        physicalUplinkWord));
  assert(physicalUplinkTiming.uplinkWordCount() == 1);
  assert(physicalUplinkCore.readErasable(agc::Core::kRegINLINK) == 012345);
  assert((physicalUplinkCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptUprupt)) != 0);
  assert(!physicalUplinkTiming.receivePhysicalUplinkWord(
      physicalUplinkCore, static_cast<uint16_t>(physicalUplinkWord ^ 1U)));
  assert(physicalUplinkTiming.uplinkParityRejectCount() == 1);
  assert(physicalUplinkCore.restartLight());
  assert((physicalUplinkCore.readChannel(agc::Core::kChannelRestartMonitor) &
          agc::Core::kCh77ParityFail) != 0);

  agc::Core handTrapCore;
  handTrapCore.setA(agc::Core::kChannel13Trap31A);
  handTrapCore.writeFixed(agc::Core::kBootAddress,
                          agc::Core::kInstructionExtend);
  handTrapCore.writeFixed(
      agc::Core::kBootAddress + 1,
      static_cast<uint16_t>(001000 | agc::Core::kChannelCounterEnable));
  handTrapCore.start();
  assert(handTrapCore.step());
  assert(handTrapCore.step());
  assert((handTrapCore.readChannel(agc::Core::kChannelCounterEnable) &
          agc::Core::kChannel13Trap31A) == 0);
  handTrapCore.writeChannel(0031, 077700);
  handTrapCore.serviceHandruptTraps();
  assert((handTrapCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptHandrupt)) != 0);

  agc::Core restartCore;
  restartCore.setRestartMonitorAlarm(agc::Core::kCh77TcTrap);
  assert((restartCore.readChannel(agc::Core::kChannelRestartMonitor) &
          agc::Core::kCh77TcTrap) != 0);
  restartCore.setA(077777);
  restartCore.writeFixed(agc::Core::kBootAddress,
                         agc::Core::kInstructionExtend);
  restartCore.writeFixed(
      agc::Core::kBootAddress + 1,
      static_cast<uint16_t>(001000 | agc::Core::kChannelRestartMonitor));
  restartCore.start();
  assert(restartCore.step());
  assert(restartCore.step());
  assert(restartCore.readChannel(agc::Core::kChannelRestartMonitor) == 0);
  restartCore.setZ(01234);
  restartCore.gojam(agc::Core::kCh77NightWatchman);
  assert(restartCore.getZ() == agc::Core::kBootAddress);
  assert(restartCore.readErasable(agc::Core::kRegQ) == 01234);
  assert(restartCore.restartLight());
  assert((restartCore.readChannel(agc::Core::kChannelRestartMonitor) &
          agc::Core::kCh77NightWatchman) != 0);

  agc::Core downruptCore;
  agc::MachineTiming downruptTiming;
  downruptTiming.setRestartMonitorsEnabled(false);
  downruptCore.writeFixed(agc::Core::kBootAddress,
                          agc::Core::kInstructionExtend);
  downruptCore.writeFixed(
      agc::Core::kBootAddress + 1,
      static_cast<uint16_t>(001000 | agc::Core::kChannelDownlinkWord0));
  downruptCore.writeFixed(agc::Core::kBootAddress + 2,
                          agc::Core::kInstructionExtend);
  downruptCore.writeFixed(
      agc::Core::kBootAddress + 3,
      static_cast<uint16_t>(001000 | agc::Core::kChannelDownlinkWord1));
  downruptCore.setA(012345);
  downruptCore.start();
  for (uint8_t i = 0; i < 4; ++i) {
    const uint32_t before = downruptCore.cycles();
    assert(downruptCore.step());
    downruptTiming.observeCpuCycles(downruptCore,
                                    downruptCore.cycles() - before);
    downruptTiming.observeCoreEvents(downruptCore);
  }
  assert(downruptTiming.downruptCount() == 0);
  assert(downruptTiming.downlinkFrameCount() == 1);
  agc::MachineTiming::DownlinkFrame downlinkFrame = {};
  assert(downruptTiming.popDownlinkFrame(&downlinkFrame));
  assert(downlinkFrame.sequence == 0);
  assert(downlinkFrame.word0 == 012345);
  assert(downlinkFrame.word1 == 012345);
  assert(downruptTiming.downlinkFrameCount() == 0);
  downruptCore.advanceCycles(1707);
  downruptTiming.serviceScheduledEvents(downruptCore);
  assert((downruptCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptDownrupt)) != 0);
  assert(downruptTiming.downruptCount() == 1);

  agc::Core radarCore;
  agc::MachineTiming radarTiming;
  radarTiming.setRestartMonitorsEnabled(false);
  assert(radarTiming.enqueueRadarWord(023456));
  radarCore.writeChannel(agc::Core::kChannelCounterEnable,
                         agc::Core::kChannel13RadarActivity);
  for (uint16_t i = 0; i < 12000 && radarTiming.radarRuptCount() == 0; ++i) {
    radarCore.advanceCycles(1);
    radarTiming.observeCpuCycles(radarCore, 1);
  }
  assert(radarTiming.radarRuptCount() == 1);
  assert((radarCore.pendingInterruptMask() &
          (1U << agc::Core::kInterruptRadarRupt)) != 0);
  assert((radarCore.readChannel(agc::Core::kChannelCounterEnable) &
          agc::Core::kChannel13RadarActivity) == 0);
  assert(radarCore.readErasable(agc::Core::kRegRNRAD) == 023456);

  agc::Core watchdogCore;
  agc::MachineTiming watchdogTiming;
  for (uint16_t i = 0; i < 3000 &&
                       watchdogTiming.restartAlarmCount() == 0; ++i) {
    watchdogCore.advanceCycles(1);
    watchdogTiming.observeCpuCycles(watchdogCore, 1);
  }
  assert(watchdogTiming.restartAlarmCount() == 1);
  assert(watchdogCore.restartLight());
  assert((watchdogCore.readChannel(agc::Core::kChannelRestartMonitor) &
          agc::Core::kCh77TcTrap) != 0);

  agc::Core decodeCore;
  decodeCore.writeErasable(00120, agc::Core::fromInt(42));
  decodeCore.writeFixed(agc::Core::kBootAddress,
                        agc::Core::encodeBasic(3, 00120));
  decodeCore.start();
  assert(decodeCore.step());
  assert(decodeCore.getA() == agc::Core::fromInt(42));
  assert(decodeCore.lastInstructionMct() == 2);

  agc::Core tcfCore;
  tcfCore.writeFixed(agc::Core::kBootAddress,
                     agc::Core::encodeBasic(1,
                                            agc::Core::kBootAddress + 2));
  tcfCore.start();
  assert(tcfCore.step());
  assert(tcfCore.getZ() == agc::Core::kBootAddress + 2);
  assert(tcfCore.lastInstructionMct() == 1);

  agc::Core returnCore;
  returnCore.writeErasable(agc::Core::kRegQ, 02550);
  returnCore.writeFixed(agc::Core::kBootAddress,
                        agc::Core::encodeBasic(0, agc::Core::kRegQ));
  returnCore.start();
  assert(returnCore.step());
  assert(returnCore.getZ() == agc::Core::kRegQ);
  assert(returnCore.readErasable(agc::Core::kRegQ) == 02550);

  agc::Core ccsCore;
  ccsCore.writeErasable(00120, agc::Core::fromInt(3));
  ccsCore.writeFixed(agc::Core::kBootAddress,
                     agc::Core::encodeBasic(1, 00120));
  ccsCore.start();
  assert(ccsCore.step());
  assert(ccsCore.getA() == agc::Core::fromInt(2));
  assert(ccsCore.readErasable(00120) == agc::Core::fromInt(3));

  agc::Core indexMinusZeroCore;
  indexMinusZeroCore.writeErasable(00120, agc::Core::kWordMask);
  indexMinusZeroCore.writeErasable(00123, agc::Core::fromInt(4));
  indexMinusZeroCore.writeFixed(agc::Core::kBootAddress,
                                static_cast<uint16_t>(050000 | 00120));
  indexMinusZeroCore.writeFixed(agc::Core::kBootAddress + 1,
                                agc::Core::encodeBasic(1, 00123));
  indexMinusZeroCore.start();
  assert(indexMinusZeroCore.step());
  assert(indexMinusZeroCore.step());
  assert(indexMinusZeroCore.lastInstruction() ==
         agc::Core::encodeBasic(1, 00123));
  assert(indexMinusZeroCore.getA() == agc::Core::fromInt(3));

  agc::Core lxchCore;
  lxchCore.writeErasable(agc::Core::kRegL, 000123);
  lxchCore.writeErasable(00120, 000456);
  lxchCore.writeFixed(agc::Core::kBootAddress,
                      static_cast<uint16_t>(022000 | 00120));
  lxchCore.start();
  assert(lxchCore.step());
  assert(lxchCore.readErasable(agc::Core::kRegL) == 000456);
  assert(lxchCore.readErasable(00120) == 000123);

  agc::Core qxchCore;
  qxchCore.writeErasable(agc::Core::kRegQ, 000777);
  qxchCore.writeErasable(00120, 000123);
  qxchCore.writeFixed(agc::Core::kBootAddress,
                      agc::Core::kInstructionExtend);
  qxchCore.writeFixed(agc::Core::kBootAddress + 1,
                      static_cast<uint16_t>(022000 | 00120));
  qxchCore.start();
  assert(qxchCore.step());
  assert(qxchCore.step());
  assert(qxchCore.readErasable(agc::Core::kRegQ) == 000123);
  assert(qxchCore.readErasable(00120) == 000777);
  assert(qxchCore.lastInstructionExtended());

  agc::Core ioCore;
  ioCore.writeChannel(0777, 001234);
  ioCore.writeFixed(agc::Core::kBootAddress,
                    agc::Core::kInstructionExtend);
  ioCore.writeFixed(agc::Core::kBootAddress + 1, 000777);
  ioCore.start();
  assert(ioCore.step());
  assert(ioCore.step());
  assert(ioCore.getA() == 001234);

  agc::Core channel10Core;
  channel10Core.setA(060400);
  channel10Core.writeFixed(agc::Core::kBootAddress,
                           agc::Core::kInstructionExtend);
  channel10Core.writeFixed(agc::Core::kBootAddress + 1, 001010);
  channel10Core.start();
  assert(channel10Core.step());
  assert(channel10Core.step());
  assert(channel10Core.readChannel(agc::Core::kChannelDSKY) == 060400);
  assert(channel10Core.readOutputChannel10(014) == 060400);
  assert(channel10Core.readOutputChannel10(0) == 0);

  agc::Core channel33Core;
  channel33Core.setA(040000);
  channel33Core.writeFixed(agc::Core::kBootAddress,
                           agc::Core::kInstructionExtend);
  channel33Core.writeFixed(agc::Core::kBootAddress + 1, 003033);
  channel33Core.writeFixed(agc::Core::kBootAddress + 2,
                           agc::Core::kInstructionExtend);
  channel33Core.writeFixed(agc::Core::kBootAddress + 3, 002033);
  channel33Core.start();
  assert(channel33Core.step());
  assert(channel33Core.step());
  assert(channel33Core.getA() == 040000);
  assert(channel33Core.readChannel(0033) == 077777);
  channel33Core.setA(020000);
  assert(channel33Core.step());
  assert(channel33Core.step());
  assert(channel33Core.getA() == 020000);

  agc::Core tsOverflowCore;
  constexpr uint16_t kTsScratch = 00120;
  constexpr uint16_t kTsConst = agc::Core::kBootAddress + 012;
  tsOverflowCore.writeFixed(agc::Core::kBootAddress,
                            agc::Core::encodeBasic(3, kTsConst));
  tsOverflowCore.writeFixed(agc::Core::kBootAddress + 1,
                            agc::Core::encodeBasic(6, kTsConst));
  tsOverflowCore.writeFixed(agc::Core::kBootAddress + 2,
                            static_cast<uint16_t>(054000 | kTsScratch));
  tsOverflowCore.writeFixed(
      agc::Core::kBootAddress + 3,
      agc::Core::encodeBasic(0, agc::Core::kBootAddress + 7));
  tsOverflowCore.writeFixed(
      agc::Core::kBootAddress + 4,
      agc::Core::encodeBasic(0, agc::Core::kBootAddress + 4));
  tsOverflowCore.writeFixed(kTsConst, 020000);
  tsOverflowCore.start();
  assert(tsOverflowCore.step());
  assert(tsOverflowCore.getA() == 020000);
  assert(tsOverflowCore.step());
  assert(tsOverflowCore.getA() == 040000);
  assert(tsOverflowCore.step());
  assert(tsOverflowCore.getA() == agc::Core::fromInt(1));
  assert(tsOverflowCore.readErasable(kTsScratch) == 0);
  assert(tsOverflowCore.getZ() == agc::Core::kBootAddress + 4);

  agc::Core mpZeroCore;
  constexpr uint16_t kMpConst = agc::Core::kBootAddress + 013;
  mpZeroCore.setA(agc::Core::kWordMask);
  mpZeroCore.writeFixed(agc::Core::kBootAddress,
                        agc::Core::kInstructionExtend);
  mpZeroCore.writeFixed(agc::Core::kBootAddress + 1,
                        agc::Core::encodeBasic(7, kMpConst));
  mpZeroCore.writeFixed(kMpConst, agc::Core::fromInt(1));
  mpZeroCore.start();
  assert(mpZeroCore.step());
  assert(mpZeroCore.step());
  assert(mpZeroCore.getA() == agc::Core::kWordMask);
  assert(mpZeroCore.readErasable(agc::Core::kRegL) ==
         agc::Core::kWordMask);

  agc::Core msuZeroCore;
  constexpr uint16_t kMsuScratch = 00120;
  msuZeroCore.writeErasable(kMsuScratch, 0);
  msuZeroCore.writeFixed(agc::Core::kBootAddress,
                         agc::Core::kInstructionExtend);
  msuZeroCore.writeFixed(agc::Core::kBootAddress + 1,
                         static_cast<uint16_t>(020000 | kMsuScratch));
  msuZeroCore.start();
  assert(msuZeroCore.step());
  assert(msuZeroCore.step());
  assert(msuZeroCore.getA() == 0);
  assert(msuZeroCore.readErasable(kMsuScratch) == 0);

  agc::Core bzfCore;
  bzfCore.setA(0);
  bzfCore.writeFixed(agc::Core::kBootAddress,
                     agc::Core::kInstructionExtend);
  bzfCore.writeFixed(agc::Core::kBootAddress + 1,
                     agc::Core::encodeBasic(
                         1, agc::Core::kBootAddress + 3));
  bzfCore.start();
  assert(bzfCore.step());
  assert(bzfCore.step());
  assert(bzfCore.getZ() == agc::Core::kBootAddress + 3);
  assert(bzfCore.lastInstructionMct() == 1);

  uint16_t bankWords[agc::Core::kFixedBankSize] = {};
  bankWords[0] = agc::Core::encodeBasic(0, agc::Core::kBootAddress);
  assert(core.loadFixedBank(3, bankWords, agc::Core::kFixedBankSize));

  core.requestInterrupt(1);
  assert(core.pendingInterruptMask() != 0);

  agc::Core hintCore;
  hintCore.writeFixed(agc::Core::kBootAddress,
                      agc::Core::kInstructionInhint);
  hintCore.writeFixed(agc::Core::kBootAddress + 1,
                      agc::Core::kInstructionRelint);
  hintCore.writeFixed(agc::Core::kBootAddress + 2,
                      agc::Core::encodeBasic(0, agc::Core::kBootAddress + 2));
  hintCore.writeFixed(04004, agc::Core::encodeBasic(0, 04004));
  hintCore.start();
  assert(hintCore.step());
  assert(!hintCore.interruptsEnabled());
  hintCore.requestInterrupt(0);
  assert(hintCore.step());
  assert(hintCore.interruptsEnabled());
  assert(hintCore.pendingInterruptMask() != 0);
  assert(hintCore.step());
  assert(hintCore.pendingInterruptMask() == 0);
  assert(hintCore.readErasable(agc::Core::kRegZRUPT) ==
         agc::Core::kBootAddress + 2);
  assert(hintCore.readErasable(agc::Core::kRegBRUPT) ==
         agc::Core::encodeBasic(0, agc::Core::kBootAddress + 2));

  agc::Core resumeCore;
  constexpr uint16_t kResumeReturn = agc::Core::kBootAddress + 2;
  constexpr uint16_t kForcedXchQ =
      static_cast<uint16_t>(056000 | agc::Core::kRegQ);
  resumeCore.writeFixed(agc::Core::kBootAddress,
                        agc::Core::encodeBasic(3,
                                               agc::Core::kBootAddress + 010));
  resumeCore.writeFixed(agc::Core::kBootAddress + 010, 012345);
  resumeCore.writeFixed(04004, agc::Core::kInstructionResume);
  resumeCore.start();
  resumeCore.requestInterrupt(0);
  uint16_t interruptedPc = 0;
  uint16_t interruptedInstruction = 0;
  assert(resumeCore.serviceInterruptEntry(&interruptedPc,
                                          &interruptedInstruction));
  assert(interruptedPc == agc::Core::kBootAddress);
  assert(interruptedInstruction ==
         agc::Core::encodeBasic(3, agc::Core::kBootAddress + 010));
  assert(resumeCore.readErasable(agc::Core::kRegZRUPT) ==
         agc::Core::kBootAddress);
  assert(resumeCore.readErasable(agc::Core::kRegBRUPT) ==
         agc::Core::encodeBasic(3, agc::Core::kBootAddress + 010));
  assert(resumeCore.readErasable(agc::Core::kRegARUPT) == 0);
  assert(resumeCore.getZ() ==
         agc::Core::interruptVectorAddress(agc::Core::kInterruptT6Rupt));

  resumeCore.writeErasable(agc::Core::kRegZRUPT, kResumeReturn);
  resumeCore.writeErasable(agc::Core::kRegBRUPT, kForcedXchQ);
  resumeCore.writeErasable(agc::Core::kRegQ, 03335);
  resumeCore.setA(0);
  assert(resumeCore.step());
  assert(resumeCore.getZ() == kResumeReturn);
  resumeCore.requestInterrupt(1);
  interruptedPc = 0;
  interruptedInstruction = 0;
  assert(resumeCore.serviceInterruptEntry(&interruptedPc,
                                          &interruptedInstruction));
  assert(interruptedPc == kResumeReturn);
  assert(interruptedInstruction == kForcedXchQ);
  assert(resumeCore.getA() == 0);
  assert(resumeCore.readErasable(agc::Core::kRegQ) == 03335);
  assert(resumeCore.getZ() ==
         agc::Core::interruptVectorAddress(agc::Core::kInterruptT5Rupt));
  assert(resumeCore.pendingInterruptMask() == 0);
  assert(resumeCore.readErasable(agc::Core::kRegZRUPT) ==
         kResumeReturn);
  assert(resumeCore.readErasable(agc::Core::kRegBRUPT) ==
         kForcedXchQ);

  return 0;
}
