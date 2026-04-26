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

  core.writeErasable(agc::Core::kRegZERO, 012345);
  assert(core.readErasable(agc::Core::kRegZERO) == 0);

  core.writeErasable(agc::Core::kRegCYR, 000001);
  assert(core.readErasable(agc::Core::kRegCYR) == 040000);

  core.writeErasable(agc::Core::kRegCYL, 040000);
  assert(core.readErasable(agc::Core::kRegCYL) == 000001);

  core.writeErasable(agc::Core::kRegSR, 040002);
  assert(core.readErasable(agc::Core::kRegSR) == 060001);

  core.writeErasable(agc::Core::kRegEDOP, 077777);
  assert(core.readErasable(agc::Core::kRegEDOP) == 000177);

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
