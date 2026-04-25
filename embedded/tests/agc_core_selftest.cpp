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

  return 0;
}
