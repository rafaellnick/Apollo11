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

  uint16_t bankWords[agc::Core::kFixedBankSize] = {};
  bankWords[0] = agc::Core::encodeBasic(0, agc::Core::kBootAddress);
  assert(core.loadFixedBank(3, bankWords, agc::Core::kFixedBankSize));

  core.requestInterrupt(1);
  assert(core.pendingInterruptMask() != 0);

  return 0;
}
