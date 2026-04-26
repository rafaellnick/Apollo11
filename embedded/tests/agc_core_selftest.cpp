#include "../shared/agc_core.h"

#include <assert.h>
#include <stdint.h>

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

  core.writeChannel(0010, 012345);
  assert(core.readChannel(0010) == 012345);
  core.writeChannel(0777, 076543);
  assert(core.readChannel(0777) == 076543);
  core.writeChannel(01001, 000123);
  assert(core.readChannel(0001) == 000123);
  assert(core.readErasable(agc::Core::kRegL) == 000123);
  core.writeChannel(01002, 000456);
  assert(core.readChannel(0002) == 000456);
  assert(core.readErasable(agc::Core::kRegQ) == 000456);

  agc::Core mapCore;
  mapCore.writeErasable(agc::Core::kRegEBANK, 000007);
  mapCore.writeErasable(03400, 001234);
  assert(mapCore.read(01400) == 001234);
  mapCore.writeErasable(agc::Core::kRegFBANK, 000005);
  mapCore.writeFixedBank(05, 0, 056000);
  assert(mapCore.read(02000) == 056000);
  mapCore.writeErasable(agc::Core::kRegFBANK, 000030);
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

  agc::Core ccsCore;
  ccsCore.writeErasable(00120, agc::Core::fromInt(3));
  ccsCore.writeFixed(agc::Core::kBootAddress,
                     agc::Core::encodeBasic(1, 00120));
  ccsCore.start();
  assert(ccsCore.step());
  assert(ccsCore.getA() == agc::Core::fromInt(2));
  assert(ccsCore.readErasable(00120) == agc::Core::fromInt(3));

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
  resumeCore.setA(agc::Core::fromInt(7));
  resumeCore.writeFixed(agc::Core::kBootAddress,
                        agc::Core::encodeBasic(0,
                                               agc::Core::kBootAddress + 2));
  resumeCore.writeFixed(agc::Core::kBootAddress + 2,
                        agc::Core::encodeBasic(0,
                                               agc::Core::kBootAddress + 2));
  resumeCore.writeFixed(04004, agc::Core::kInstructionResume);
  resumeCore.start();
  resumeCore.requestInterrupt(0);
  assert(resumeCore.step());
  assert(resumeCore.readErasable(agc::Core::kRegZRUPT) ==
         agc::Core::kBootAddress);
  assert(resumeCore.readErasable(agc::Core::kRegBRUPT) ==
         agc::Core::encodeBasic(0, agc::Core::kBootAddress + 2));
  assert(resumeCore.readErasable(agc::Core::kRegARUPT) == 0);
  assert(resumeCore.getZ() == agc::Core::kBootAddress);
  resumeCore.requestInterrupt(1);
  assert(resumeCore.step());
  assert(resumeCore.getZ() == agc::Core::kBootAddress + 2);
  assert(resumeCore.pendingInterruptMask() != 0);

  return 0;
}
