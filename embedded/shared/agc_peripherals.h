#ifndef APOLLO11_EMBEDDED_AGC_PERIPHERALS_H
#define APOLLO11_EMBEDDED_AGC_PERIPHERALS_H

#include "agc_core.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace agc {

class Peripherals {
 public:
  static constexpr uint16_t kChannelCount = Core::kIoChannels;
  static constexpr uint8_t kDownlinkChannelCount = 6;
  static constexpr uint8_t kDownlinkQueueSize = 32;
  static constexpr uint8_t kKeyQueueSize = 16;

  static constexpr uint16_t kChannelDownlink0 = Core::kChannelOut0;
  static constexpr uint16_t kChannelDownlink1 = Core::kChannelOut1;
  static constexpr uint16_t kChannelDownlink2 = Core::kChannelOut2;
  static constexpr uint16_t kChannelDownlink3 = Core::kChannelOut3;
  static constexpr uint16_t kChannelDownlinkWord0 = Core::kChannelDownlinkWord0;
  static constexpr uint16_t kChannelDownlinkWord1 = Core::kChannelDownlinkWord1;
  static constexpr uint16_t kChannelKeyInput = Core::kChannelKey;

  static constexpr uint8_t kInterruptDownrupt = Core::kInterruptDownrupt;
  static constexpr uint8_t kInterruptKeyrupt = Core::kInterruptKeyrupt1;

  static constexpr uint32_t kAgcMctPerSecond = 85333;
  static constexpr uint32_t kDefaultTimerPeriodCycles =
      kAgcMctPerSecond / 100;
  static constexpr uint32_t kDefaultTime6PeriodCycles =
      kAgcMctPerSecond / 1600;
  static constexpr uint32_t kDefaultDownlinkPeriodCycles = 1024;
  static constexpr uint32_t kDefaultDownruptPeriodCycles = 4096;
  static constexpr uint32_t kDefaultKeyPeriodCycles = 128;

  enum DownlinkReason : uint8_t {
    kDownlinkPeriodic = 0,
    kDownlinkChannelChange,
  };

  struct Config {
    uint32_t downlinkPeriodCycles;
    uint32_t downruptPeriodCycles;
    uint32_t keyPeriodCycles;
    uint32_t timerPeriodCycles;
    uint32_t time6PeriodCycles;
    uint8_t downruptInterrupt;
    uint8_t keyruptInterrupt;
    bool captureChannelChanges;
    bool scheduleDownrupt;
    bool requestScheduledInterrupts;
    bool scheduleCounters;
  };

  struct DownlinkWord {
    uint32_t cycle;
    uint16_t sequence;
    uint16_t channel;
    uint16_t word;
    DownlinkReason reason;
  };

  Peripherals() : config_(defaultConfig()) { reset(0); }

  explicit Peripherals(const Config& config)
      : config_(sanitizeConfig(config)) {
    reset(0);
  }

  static Config defaultConfig() {
    Config config = {kDefaultDownlinkPeriodCycles,
                     kDefaultDownruptPeriodCycles,
                     kDefaultKeyPeriodCycles,
                     kDefaultTimerPeriodCycles,
                     kDefaultTime6PeriodCycles,
                     kInterruptDownrupt,
                     kInterruptKeyrupt,
                     true,
                     true,
                     false,
                     true};
    return config;
  }

  void setConfig(const Config& config) {
    const uint32_t cycle = lastCycle_;
    config_ = sanitizeConfig(config);
    reset(cycle);
  }

  const Config& config() const { return config_; }

  void setRequestScheduledInterrupts(bool enabled) {
    config_.requestScheduledInterrupts = enabled;
  }

  void reset(uint32_t cycle) {
    memset(lastChannels_, 0, sizeof(lastChannels_));
    memset(downlinkQueue_, 0, sizeof(downlinkQueue_));
    memset(keyQueue_, 0, sizeof(keyQueue_));
    lastCycle_ = cycle;
    nextDownlinkCycle_ = cycle + config_.downlinkPeriodCycles;
    nextDownruptCycle_ = cycle + config_.downruptPeriodCycles;
    nextKeyCycle_ = cycle;
    nextTimerCycle_ = cycle + config_.timerPeriodCycles;
    nextTime6Cycle_ = cycle + config_.time6PeriodCycles;
    downlinkHead_ = 0;
    downlinkCount_ = 0;
    keyHead_ = 0;
    keyCount_ = 0;
    downlinkSequence_ = 0;
    nextDownlinkChannelIndex_ = 0;
    channelChangeCount_ = 0;
    downlinkDropped_ = 0;
    keyDropped_ = 0;
    downruptCount_ = 0;
    keyruptCount_ = 0;
    counterPulseCount_ = 0;
    time6PulseCount_ = 0;
    lastDownruptCycle_ = 0;
    lastKeyCycle_ = 0;
    lastCounterCycle_ = 0;
    lastTime6Cycle_ = 0;
    lastKeyWord_ = 0;
    lastDownlinkWord_ = 0;
    lastDownlinkChannel_ = 0;
    lastDownlinkCycle_ = 0;
  }

  void reset(const Core& core) {
    reset(core.cycles());
    snapshotChannels(core);
  }

  bool tick(Core& core) {
    const uint32_t cycle = core.cycles();
    bool changed = false;

    if (serviceKeyQueue(core, cycle)) {
      changed = true;
    }

    if (config_.scheduleCounters && serviceCounters(core, cycle)) {
      changed = true;
    }

    if (config_.captureChannelChanges && captureChangedChannels(core, cycle)) {
      changed = true;
    }

    while (cycleReached(cycle, nextDownlinkCycle_)) {
      capturePeriodicDownlink(core, nextDownlinkCycle_);
      nextDownlinkCycle_ += config_.downlinkPeriodCycles;
      changed = true;
    }

    while (config_.scheduleDownrupt &&
           cycleReached(cycle, nextDownruptCycle_)) {
      if (config_.requestScheduledInterrupts) {
        core.requestInterrupt(config_.downruptInterrupt);
      }
      downruptCount_++;
      lastDownruptCycle_ = nextDownruptCycle_;
      nextDownruptCycle_ += config_.downruptPeriodCycles;
      changed = true;
    }

    lastCycle_ = cycle;
    return changed;
  }

  uint16_t readChannel(const Core& core, uint16_t channel) const {
    return core.readChannel(channel);
  }

  void writeChannel(Core& core, uint16_t channel, uint16_t value) {
    channel &= (Core::kIoChannels - 1);
    value &= Core::kWordMask;
    core.writeChannel(channel, value);

    if (lastChannels_[channel] == value) {
      return;
    }

    lastChannels_[channel] = value;
    channelChangeCount_++;

    if (isDownlinkChannel(channel)) {
      pushDownlink(channel, value, core.cycles(), kDownlinkChannelChange);
    }
  }

  bool enqueueKey(uint16_t keyWord) {
    if (keyCount_ >= kKeyQueueSize) {
      keyDropped_++;
      return false;
    }

    const uint8_t index =
        static_cast<uint8_t>((keyHead_ + keyCount_) % kKeyQueueSize);
    keyQueue_[index] = keyWord & Core::kWordMask;
    keyCount_++;
    return true;
  }

  bool popDownlink(DownlinkWord* word) {
    if (downlinkCount_ == 0) {
      return false;
    }

    if (word != nullptr) {
      *word = downlinkQueue_[downlinkHead_];
    }

    downlinkHead_ = static_cast<uint8_t>((downlinkHead_ + 1) %
                                         kDownlinkQueueSize);
    downlinkCount_--;
    return true;
  }

  void clearDownlinkQueue() {
    downlinkHead_ = 0;
    downlinkCount_ = 0;
  }

  uint8_t downlinkQueueDepth() const { return downlinkCount_; }
  uint8_t keyQueueDepth() const { return keyCount_; }
  uint32_t lastCycle() const { return lastCycle_; }
  uint32_t channelChangeCount() const { return channelChangeCount_; }
  uint32_t downlinkDropped() const { return downlinkDropped_; }
  uint32_t keyDropped() const { return keyDropped_; }
  uint32_t downruptCount() const { return downruptCount_; }
  uint32_t keyruptCount() const { return keyruptCount_; }
  uint32_t counterPulseCount() const { return counterPulseCount_; }
  uint32_t time6PulseCount() const { return time6PulseCount_; }
  uint32_t lastDownruptCycle() const { return lastDownruptCycle_; }
  uint32_t lastKeyCycle() const { return lastKeyCycle_; }
  uint32_t lastCounterCycle() const { return lastCounterCycle_; }
  uint32_t lastTime6Cycle() const { return lastTime6Cycle_; }
  uint16_t lastKeyWord() const { return lastKeyWord_; }
  uint16_t lastDownlinkWord() const { return lastDownlinkWord_; }
  uint16_t lastDownlinkChannel() const { return lastDownlinkChannel_; }
  uint32_t lastDownlinkCycle() const { return lastDownlinkCycle_; }
  uint16_t nextDownlinkSequence() const { return downlinkSequence_; }

  static const char* downlinkReasonName(DownlinkReason reason) {
    switch (reason) {
      case kDownlinkPeriodic:
        return "PERIOD";
      case kDownlinkChannelChange:
        return "CHANGE";
    }

    return "?";
  }

  static const char* interruptName(uint8_t interruptNumber) {
    switch (interruptNumber) {
      case Core::kInterruptT6Rupt:
        return "T6RUPT";
      case Core::kInterruptT5Rupt:
        return "T5RUPT";
      case Core::kInterruptT3Rupt:
        return "T3RUPT";
      case Core::kInterruptT4Rupt:
        return "T4RUPT";
      case Core::kInterruptKeyrupt1:
        return "KEYRUPT1";
      case Core::kInterruptKeyrupt2:
        return "KEYRUPT2";
      case Core::kInterruptUprupt:
        return "UPRUPT";
      case kInterruptDownrupt:
        return "DOWNRUPT";
      case Core::kInterruptRadarRupt:
        return "RADAR";
      case Core::kInterruptHandrupt:
        return "HANDRUPT";
      default:
        return "IRQ";
    }
  }

 private:
  static Config sanitizeConfig(Config config) {
    if (config.downlinkPeriodCycles == 0) {
      config.downlinkPeriodCycles = kDefaultDownlinkPeriodCycles;
    }
    if (config.downruptPeriodCycles == 0) {
      config.downruptPeriodCycles = kDefaultDownruptPeriodCycles;
    }
    if (config.keyPeriodCycles == 0) {
      config.keyPeriodCycles = kDefaultKeyPeriodCycles;
    }
    if (config.timerPeriodCycles == 0) {
      config.timerPeriodCycles = kDefaultTimerPeriodCycles;
    }
    if (config.time6PeriodCycles == 0) {
      config.time6PeriodCycles = kDefaultTime6PeriodCycles;
    }
    if (config.downruptInterrupt >= Core::kInterruptCount) {
      config.downruptInterrupt = kInterruptDownrupt;
    }
    if (config.keyruptInterrupt >= Core::kInterruptCount) {
      config.keyruptInterrupt = kInterruptKeyrupt;
    }

    return config;
  }

  static bool cycleReached(uint32_t cycle, uint32_t target) {
    return static_cast<int32_t>(cycle - target) >= 0;
  }

  static uint16_t downlinkChannelAt(uint8_t index) {
    switch (index % kDownlinkChannelCount) {
      case 0:
        return kChannelDownlink0;
      case 1:
        return kChannelDownlink1;
      case 2:
        return kChannelDownlink2;
      case 3:
        return kChannelDownlink3;
      case 4:
        return kChannelDownlinkWord0;
      default:
        return kChannelDownlinkWord1;
    }
  }

  static bool isDownlinkChannel(uint16_t channel) {
    return channel == kChannelDownlink0 || channel == kChannelDownlink1 ||
           channel == kChannelDownlink2 || channel == kChannelDownlink3 ||
           channel == kChannelDownlinkWord0 ||
           channel == kChannelDownlinkWord1;
  }

  void snapshotChannels(const Core& core) {
    for (uint16_t channel = 0; channel < Core::kIoChannels; ++channel) {
      lastChannels_[channel] = core.readChannel(channel);
    }
  }

  bool captureChangedChannels(const Core& core, uint32_t cycle) {
    bool changed = false;

    for (uint16_t channel = 0; channel < Core::kIoChannels; ++channel) {
      const uint16_t word = core.readChannel(channel);
      if (lastChannels_[channel] == word) {
        continue;
      }

      lastChannels_[channel] = word;
      channelChangeCount_++;
      changed = true;

      if (isDownlinkChannel(channel)) {
        pushDownlink(channel, word, cycle, kDownlinkChannelChange);
      }
    }

    return changed;
  }

  void capturePeriodicDownlink(const Core& core, uint32_t cycle) {
    const uint16_t channel = downlinkChannelAt(nextDownlinkChannelIndex_);
    nextDownlinkChannelIndex_ =
        static_cast<uint8_t>((nextDownlinkChannelIndex_ + 1) %
                             kDownlinkChannelCount);
    pushDownlink(channel, core.readChannel(channel), cycle, kDownlinkPeriodic);
  }

  bool serviceKeyQueue(Core& core, uint32_t cycle) {
    if (keyCount_ == 0 || !cycleReached(cycle, nextKeyCycle_)) {
      return false;
    }

    const uint16_t keyWord = keyQueue_[keyHead_];
    keyHead_ = static_cast<uint8_t>((keyHead_ + 1) % kKeyQueueSize);
    keyCount_--;

    writeChannel(core, kChannelKeyInput,
                 static_cast<uint16_t>((keyWord & 037) | 00040));
    if (config_.requestScheduledInterrupts) {
      core.requestInterrupt(config_.keyruptInterrupt);
    }
    keyruptCount_++;
    lastKeyCycle_ = cycle;
    lastKeyWord_ = keyWord;
    nextKeyCycle_ = cycle + config_.keyPeriodCycles;
    return true;
  }

  bool serviceCounters(Core& core, uint32_t cycle) {
    bool changed = false;

    while (cycleReached(cycle, nextTimerCycle_)) {
      if (core.counterPinc(Core::kRegTIME1)) {
        core.counterPinc(Core::kRegTIME2);
      }
      core.counterPinc(Core::kRegTIME3, Core::kInterruptT3Rupt);
      core.counterPinc(Core::kRegTIME4, Core::kInterruptT4Rupt);
      core.counterPinc(Core::kRegTIME5, Core::kInterruptT5Rupt);
      counterPulseCount_++;
      lastCounterCycle_ = nextTimerCycle_;
      nextTimerCycle_ += config_.timerPeriodCycles;
      changed = true;
    }

    while (cycleReached(cycle, nextTime6Cycle_)) {
      if ((core.readChannel(Core::kChannelCounterEnable) & Core::kSignBit) !=
          0) {
        if (core.counterDinc(Core::kRegTIME6, Core::kInterruptT6Rupt)) {
          core.writeChannel(Core::kChannelCounterEnable,
                            static_cast<uint16_t>(
                                core.readChannel(Core::kChannelCounterEnable) &
                                (Core::kWordMask ^ Core::kSignBit)));
        }
        time6PulseCount_++;
        lastTime6Cycle_ = nextTime6Cycle_;
        changed = true;
      }
      nextTime6Cycle_ += config_.time6PeriodCycles;
    }

    return changed;
  }

  void pushDownlink(uint16_t channel, uint16_t word, uint32_t cycle,
                    DownlinkReason reason) {
    if (downlinkCount_ == kDownlinkQueueSize) {
      downlinkHead_ = static_cast<uint8_t>((downlinkHead_ + 1) %
                                           kDownlinkQueueSize);
      downlinkCount_--;
      downlinkDropped_++;
    }

    const uint8_t index = static_cast<uint8_t>(
        (downlinkHead_ + downlinkCount_) % kDownlinkQueueSize);
    DownlinkWord& entry = downlinkQueue_[index];
    entry.cycle = cycle;
    entry.sequence = downlinkSequence_++;
    entry.channel = channel;
    entry.word = word & Core::kWordMask;
    entry.reason = reason;
    downlinkCount_++;

    lastDownlinkWord_ = entry.word;
    lastDownlinkChannel_ = entry.channel;
    lastDownlinkCycle_ = entry.cycle;
  }

  Config config_;
  uint16_t lastChannels_[kChannelCount];
  DownlinkWord downlinkQueue_[kDownlinkQueueSize];
  uint16_t keyQueue_[kKeyQueueSize];
  uint32_t lastCycle_ = 0;
  uint32_t nextDownlinkCycle_ = 0;
  uint32_t nextDownruptCycle_ = 0;
  uint32_t nextKeyCycle_ = 0;
  uint32_t nextTimerCycle_ = 0;
  uint32_t nextTime6Cycle_ = 0;
  uint8_t downlinkHead_ = 0;
  uint8_t downlinkCount_ = 0;
  uint8_t keyHead_ = 0;
  uint8_t keyCount_ = 0;
  uint16_t downlinkSequence_ = 0;
  uint8_t nextDownlinkChannelIndex_ = 0;
  uint32_t channelChangeCount_ = 0;
  uint32_t downlinkDropped_ = 0;
  uint32_t keyDropped_ = 0;
  uint32_t downruptCount_ = 0;
  uint32_t keyruptCount_ = 0;
  uint32_t counterPulseCount_ = 0;
  uint32_t time6PulseCount_ = 0;
  uint32_t lastDownruptCycle_ = 0;
  uint32_t lastKeyCycle_ = 0;
  uint32_t lastCounterCycle_ = 0;
  uint32_t lastTime6Cycle_ = 0;
  uint16_t lastKeyWord_ = 0;
  uint16_t lastDownlinkWord_ = 0;
  uint16_t lastDownlinkChannel_ = 0;
  uint32_t lastDownlinkCycle_ = 0;
};

}  // namespace agc

#endif
