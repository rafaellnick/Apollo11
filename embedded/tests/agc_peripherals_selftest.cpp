#include "../shared/agc_peripherals.h"

#include <assert.h>
#include <stdint.h>

int main() {
  agc::Core core;
  agc::Peripherals::Config config = agc::Peripherals::defaultConfig();
  config.requestScheduledInterrupts = true;
  agc::Peripherals peripherals(config);

  core.start();
  peripherals.reset(core);

  assert(peripherals.enqueueKey(0123));
  assert(peripherals.tick(core));
  assert(core.readChannel(agc::Peripherals::kChannelKeyInput) == 0123);
  assert(peripherals.keyQueueDepth() == 0);
  assert(peripherals.keyruptCount() == 1);
  assert((core.pendingInterruptMask() &
          (1U << agc::Peripherals::kInterruptKeyrupt)) != 0);

  peripherals.writeChannel(core, agc::Peripherals::kChannelDownlink0, 04567);
  assert(peripherals.downlinkQueueDepth() == 1);

  agc::Peripherals::DownlinkWord word = {};
  assert(peripherals.popDownlink(&word));
  assert(word.channel == agc::Peripherals::kChannelDownlink0);
  assert(word.word == 04567);
  assert(word.reason == agc::Peripherals::kDownlinkChannelChange);

  peripherals.writeChannel(core, 0777, 012345);
  assert(peripherals.readChannel(core, 0777) == 012345);

  core.reset();
  core.start();
  peripherals.reset(core);
  core.writeChannel(0777, 076543);
  assert(peripherals.tick(core));
  assert(peripherals.channelChangeCount() == 1);

  core.reset();
  core.start();
  peripherals.reset(core);
  core.runFor(static_cast<uint16_t>(
      agc::Peripherals::kDefaultDownlinkPeriodCycles));
  assert(peripherals.tick(core));
  assert(peripherals.downlinkQueueDepth() == 1);
  assert(peripherals.popDownlink(&word));
  assert(word.reason == agc::Peripherals::kDownlinkPeriodic);

  core.reset();
  core.start();
  peripherals.reset(core);
  core.runFor(static_cast<uint16_t>(
      agc::Peripherals::kDefaultDownruptPeriodCycles));
  assert(peripherals.tick(core));
  assert(peripherals.downruptCount() == 1);
  assert((core.pendingInterruptMask() &
          (1U << agc::Peripherals::kInterruptDownrupt)) != 0);

  return 0;
}
