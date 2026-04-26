#ifndef APOLLO11_EMBEDDED_AGC_MACHINE_TIMING_H
#define APOLLO11_EMBEDDED_AGC_MACHINE_TIMING_H

#include "agc_core.h"

#include <stdint.h>

namespace agc {

class MachineTiming {
 public:
  struct DownlinkFrame {
    uint32_t cycle;
    uint16_t word0;
    uint16_t word1;
    uint16_t sequence;
  };

  MachineTiming() { reset(); }

  void reset() {
    scalerCounter_ = 0;
    pendingExtraDelay_ = 0;
    downlinkLatch_ = 0;
    downlinkWord0_ = 0;
    downlinkWord1_ = 0;
    downlinkFrameHead_ = 0;
    downlinkFrameCount_ = 0;
    downlinkFrameSequence_ = 0;
    downlinkFrameDropped_ = 0;
    downruptTimeValid_ = false;
    downruptTime_ = 0;
    nightWatchman_ = false;
    ruptLock_ = false;
    noRupt_ = false;
    tcTrap_ = false;
    noTc_ = false;
    restartMonitorsEnabled_ = true;
    radarGateCounter_ = 0;
    radarDataWord_ = 0;
    radarQueueHead_ = 0;
    radarQueueCount_ = 0;
    radarDropped_ = 0;
    uplinkShiftRegister_ = 0;
    uplinkBitCount_ = 0;
    uplinkWordCount_ = 0;
    uplinkParityRejectCount_ = 0;
    scalerOverflowCount_ = 0;
    stealCycleCount_ = 0;
    counterPulseCount_ = 0;
    time6PulseCount_ = 0;
    downruptCount_ = 0;
    restartAlarmCount_ = 0;
    radarRuptCount_ = 0;
    handruptCount_ = 0;
  }

  void serviceScheduledEvents(Core& core) {
    if (downruptTimeValid_ &&
        static_cast<int32_t>(core.cycles() - downruptTime_) >= 0) {
      core.requestInterrupt(Core::kInterruptDownrupt);
      downruptTimeValid_ = false;
      ++downruptCount_;
    }
  }

  uint16_t consumeStallCycles(Core& core) {
    serviceScheduledEvents(core);
    if (!nextCycleWouldStall(core)) {
      return 0;
    }

    uint16_t consumed = 0;
    if (pendingExtraDelay_ > 0) {
      while (pendingExtraDelay_ > 0) {
        core.advanceCycles(1);
        scalerCounter_ += kScalerIncrement;
        serviceScheduledEvents(core);
        --pendingExtraDelay_;
        ++consumed;
      }
      stealCycleCount_ += consumed;
      return consumed;
    }

    core.advanceCycles(1);
    scalerCounter_ += kScalerIncrement;
    serviceScheduledEvents(core);
    consumed = 1;

    const uint16_t stalls = processDueScalerOverflows(core);
    if (stalls > 0) {
      for (uint16_t stall = 1; stall < stalls; ++stall) {
        core.advanceCycles(1);
        scalerCounter_ += kScalerIncrement;
        serviceScheduledEvents(core);
        ++consumed;
      }
      stealCycleCount_ += consumed;
      return consumed;
    }

    return 0;
  }

  void observeCpuCycles(Core& core, uint32_t mct) {
    for (uint32_t i = 0; i < mct; ++i) {
      serviceScheduledEvents(core);
      scalerCounter_ += kScalerIncrement;
      const uint16_t stalls = processDueScalerOverflows(core);
      if (stalls == 0) {
        continue;
      }

      core.advanceCycles(stalls);
      serviceScheduledEvents(core);
      stealCycleCount_ += stalls;

      for (uint16_t stall = 0; stall < stalls; ++stall) {
        scalerCounter_ += kScalerIncrement;
        serviceScheduledEvents(core);
      }
    }
  }

  void observeCoreEvents(Core& core, bool executedInstruction = true) {
    const uint8_t flags = core.takeHardwareWriteFlags();
    if ((flags & Core::kHardwareWriteDownlinkWord0) != 0) {
      downlinkWord0_ = core.readChannel(Core::kChannelDownlinkWord0);
      downlinkLatch_ |= 1;
    }
    if ((flags & Core::kHardwareWriteDownlinkWord1) != 0) {
      downlinkWord1_ = core.readChannel(Core::kChannelDownlinkWord1);
      downlinkLatch_ |= 2;
    }
    if (downlinkLatch_ == 3) {
      downruptTime_ = core.cycles() + kDownruptDelayMct;
      downruptTimeValid_ = true;
      pushDownlinkFrame(core.cycles(), downlinkWord0_, downlinkWord1_);
      downlinkLatch_ = 0;
    }

    if (core.takeInterruptEntryEvent()) {
      noRupt_ = false;
    }

    if (!executedInstruction) {
      return;
    }

    if (core.inInterruptService()) {
      noRupt_ = false;
    } else {
      ruptLock_ = false;
    }

    const bool executedTc = core.takeTcEvent();
    if (executedTc) {
      noTc_ = false;
    } else {
      tcTrap_ = false;
    }
  }

  uint32_t scalerOverflowCount() const { return scalerOverflowCount_; }
  uint32_t stealCycleCount() const { return stealCycleCount_; }
  uint32_t counterPulseCount() const { return counterPulseCount_; }
  uint32_t time6PulseCount() const { return time6PulseCount_; }
  uint32_t downruptCount() const { return downruptCount_; }
  uint32_t restartAlarmCount() const { return restartAlarmCount_; }
  uint32_t radarRuptCount() const { return radarRuptCount_; }
  uint32_t handruptCount() const { return handruptCount_; }
  uint32_t radarDroppedCount() const { return radarDropped_; }
  uint32_t uplinkWordCount() const { return uplinkWordCount_; }
  uint32_t uplinkParityRejectCount() const {
    return uplinkParityRejectCount_;
  }
  uint8_t uplinkBitCount() const { return uplinkBitCount_; }
  uint8_t downlinkFrameCount() const { return downlinkFrameCount_; }
  uint32_t downlinkFrameDroppedCount() const {
    return downlinkFrameDropped_;
  }

  void setRadarDataWord(uint16_t word) {
    radarDataWord_ = word & Core::kWordMask;
  }

  bool enqueueRadarWord(uint16_t word) {
    if (radarQueueCount_ >= kRadarQueueSize) {
      ++radarDropped_;
      return false;
    }

    const uint8_t index =
        static_cast<uint8_t>((radarQueueHead_ + radarQueueCount_) %
                             kRadarQueueSize);
    radarQueue_[index] = word & Core::kWordMask;
    ++radarQueueCount_;
    return true;
  }

  bool popDownlinkFrame(DownlinkFrame* frame) {
    if (downlinkFrameCount_ == 0) {
      return false;
    }

    if (frame != nullptr) {
      *frame = downlinkFrames_[downlinkFrameHead_];
    }
    downlinkFrameHead_ = static_cast<uint8_t>((downlinkFrameHead_ + 1) %
                                              kDownlinkFrameQueueSize);
    --downlinkFrameCount_;
    return true;
  }

  void clearDownlinkFrames() {
    downlinkFrameHead_ = 0;
    downlinkFrameCount_ = 0;
  }

  void resetUplinkReceiver() {
    uplinkShiftRegister_ = 0;
    uplinkBitCount_ = 0;
  }

  bool receiveUplinkBit(Core& core, bool oneBit) {
    uplinkShiftRegister_ =
        static_cast<uint16_t>(((uplinkShiftRegister_ << 1) |
                               (oneBit ? 1U : 0U)) &
                              Core::kWordMask);
    ++uplinkBitCount_;
    if (uplinkBitCount_ < 15) {
      return false;
    }

    core.receiveUplinkWord(uplinkShiftRegister_);
    ++uplinkWordCount_;
    resetUplinkReceiver();
    return true;
  }

  bool receiveUplinkWord(Core& core, uint16_t word) {
    bool accepted = false;
    for (int8_t bit = 14; bit >= 0; --bit) {
      accepted = receiveUplinkBit(core, (word & (1U << bit)) != 0);
    }
    return accepted;
  }

  bool receivePhysicalUplinkWord(Core& core, uint16_t physicalWord) {
    uint16_t word = 0;
    if (!Core::physicalWordToData(physicalWord, &word)) {
      ++uplinkParityRejectCount_;
      core.gojam(Core::kCh77ParityFail);
      resetUplinkReceiver();
      return false;
    }

    return receiveUplinkWord(core, word);
  }

  void setRestartMonitorsEnabled(bool enabled) {
    restartMonitorsEnabled_ = enabled;
    if (!enabled) {
      nightWatchman_ = false;
      ruptLock_ = false;
      noRupt_ = false;
      tcTrap_ = false;
      noTc_ = false;
    }
  }

 private:
  static constexpr uint32_t kAgcMctPerSecond = 85333;
  static constexpr uint32_t kDownruptDelayMct = kAgcMctPerSecond / 50;
  static constexpr int16_t kScalerIncrement = 3;
  static constexpr int16_t kScalerOverflow = 80;
  static constexpr uint16_t kChannelScaler2 = 0003;
  static constexpr uint16_t kChannelScaler1 = 0004;
  static constexpr uint8_t kDownlinkFrameQueueSize = 16;
  static constexpr uint8_t kRadarQueueSize = 16;

  bool nextCycleWouldStall(const Core& core) const {
    if (pendingExtraDelay_ > 0) {
      return true;
    }

    int16_t scalerCounter =
        static_cast<int16_t>(scalerCounter_ + kScalerIncrement);
    uint16_t scaler1 = core.readChannel(kChannelScaler1);

    while (scalerCounter >= kScalerOverflow) {
      scalerCounter = static_cast<int16_t>(scalerCounter - kScalerOverflow);
      scaler1 = nextScaler1Value(scaler1);
      if (stallCostForScalerValue(core, scaler1) > 0) {
        return true;
      }
    }

    return false;
  }

  uint16_t processDueScalerOverflows(Core& core) {
    uint16_t stalls = 0;
    while (scalerCounter_ >= kScalerOverflow) {
      scalerCounter_ = static_cast<int16_t>(scalerCounter_ - kScalerOverflow);
      const uint16_t scaler1 = incrementScalerChannels(core);
      ++scalerOverflowCount_;
      stalls = static_cast<uint16_t>(stalls + pulseCountersForScaler(core,
                                                                      scaler1));
      serviceRestartMonitors(core, scaler1);
      serviceRadar(core, scaler1);
      const uint16_t handruptBefore = core.pendingInterruptMask();
      core.serviceHandruptTraps();
      if (core.pendingInterruptMask() != handruptBefore &&
          (core.pendingInterruptMask() &
           (1U << Core::kInterruptHandrupt)) != 0) {
        ++handruptCount_;
      }
    }
    return stalls;
  }

  static uint16_t nextScaler1Value(uint16_t scaler1) {
    scaler1 = static_cast<uint16_t>((scaler1 + 1) & Core::kWordMask);
    return scaler1 == 040000 ? 0 : scaler1;
  }

  static uint16_t incrementScalerChannels(Core& core) {
    uint16_t scaler1 = core.readChannel(kChannelScaler1);
    scaler1 = static_cast<uint16_t>((scaler1 + 1) & Core::kWordMask);
    if (scaler1 == 040000) {
      scaler1 = 0;
      const uint16_t scaler2 =
          static_cast<uint16_t>((core.readChannel(kChannelScaler2) + 1) &
                                Core::kWordMask);
      core.writeChannel(kChannelScaler2, scaler2);
    }
    core.writeChannel(kChannelScaler1, scaler1);
    return scaler1;
  }

  static uint16_t stallCostForScalerValue(const Core& core,
                                          uint16_t scaler1) {
    uint16_t stalls = 0;
    if ((scaler1 & 037) == 020) {
      stalls = static_cast<uint16_t>(stalls + 2);
      if (core.readErasable(Core::kRegTIME1) == Core::kPositiveMask) {
        ++stalls;
      }
    }
    if ((scaler1 & 037) == 000) {
      ++stalls;
    }
    if ((scaler1 & 037) == 010) {
      ++stalls;
    }
    if ((core.readChannel(Core::kChannelCounterEnable) & Core::kSignBit) != 0 &&
        (scaler1 & 1) != 0) {
      ++stalls;
    }
    return stalls;
  }

  uint16_t pulseCountersForScaler(Core& core, uint16_t scaler1) {
    uint16_t stalls = 0;

    if ((scaler1 & 037) == 020) {
      ++stalls;
      ++counterPulseCount_;
      if (counterPinc(core, Core::kRegTIME1, Core::kNoInterrupt)) {
        ++stalls;
        ++counterPulseCount_;
        counterPinc(core, Core::kRegTIME2, Core::kNoInterrupt);
      }

      ++stalls;
      ++counterPulseCount_;
      counterPinc(core, Core::kRegTIME3, Core::kInterruptT3Rupt);
    }

    if ((scaler1 & 037) == 000) {
      ++stalls;
      ++counterPulseCount_;
      counterPinc(core, Core::kRegTIME5, Core::kInterruptT5Rupt);
      if ((core.readChannel(Core::kChannelCounterEnable) &
           Core::kChannel13RadarActivity) != 0) {
        ++radarGateCounter_;
      }
    }

    if ((scaler1 & 037) == 010) {
      ++stalls;
      ++counterPulseCount_;
      counterPinc(core, Core::kRegTIME4, Core::kInterruptT4Rupt);
    }

    const uint16_t channel13 = core.readChannel(Core::kChannelCounterEnable);
    if ((channel13 & Core::kSignBit) != 0 && (scaler1 & 1) != 0) {
      ++stalls;
      ++time6PulseCount_;
      if (core.counterDinc(Core::kRegTIME6, Core::kInterruptT6Rupt)) {
        core.writeChannel(Core::kChannelCounterEnable,
                          static_cast<uint16_t>(channel13 & ~Core::kSignBit));
      }
    }

    return stalls;
  }

  void serviceRadar(Core& core, uint16_t scaler1) {
    if (radarGateCounter_ == 9 && (scaler1 & 037) == 036) {
      radarGateCounter_ = 0;
      core.writeChannel(Core::kChannelCounterEnable,
                        static_cast<uint16_t>(
                            core.readChannel(Core::kChannelCounterEnable) &
                            ~Core::kChannel13RadarActivity));
      core.writeErasable(Core::kRegRNRAD, nextRadarDataWord());
      core.requestInterrupt(Core::kInterruptRadarRupt);
      ++radarRuptCount_;
    }
  }

  void pushDownlinkFrame(uint32_t cycle, uint16_t word0, uint16_t word1) {
    if (downlinkFrameCount_ >= kDownlinkFrameQueueSize) {
      downlinkFrameHead_ = static_cast<uint8_t>((downlinkFrameHead_ + 1) %
                                                kDownlinkFrameQueueSize);
      --downlinkFrameCount_;
      ++downlinkFrameDropped_;
    }

    const uint8_t index =
        static_cast<uint8_t>((downlinkFrameHead_ + downlinkFrameCount_) %
                             kDownlinkFrameQueueSize);
    DownlinkFrame frame = {cycle,
                           static_cast<uint16_t>(word0 & Core::kWordMask),
                           static_cast<uint16_t>(word1 & Core::kWordMask),
                           downlinkFrameSequence_++};
    downlinkFrames_[index] = frame;
    ++downlinkFrameCount_;
  }

  uint16_t nextRadarDataWord() {
    if (radarQueueCount_ == 0) {
      return radarDataWord_;
    }

    radarDataWord_ = radarQueue_[radarQueueHead_] & Core::kWordMask;
    radarQueueHead_ = static_cast<uint8_t>((radarQueueHead_ + 1) %
                                           kRadarQueueSize);
    --radarQueueCount_;
    return radarDataWord_;
  }

  void serviceRestartMonitors(Core& core, uint16_t scaler1) {
    if (!restartMonitorsEnabled_) {
      return;
    }

    if (core.takeNightWatchmanService()) {
      nightWatchman_ = false;
    }

    if ((scaler1 & 07777) == 04000) {
      nightWatchman_ = true;
    } else if ((scaler1 & 07777) == 00000) {
      if (nightWatchman_) {
        core.gojam(Core::kCh77NightWatchman);
        nightWatchman_ = false;
        ++restartAlarmCount_;
      }
    }

    if ((scaler1 & 0777) == 0400) {
      ruptLock_ = true;
      noRupt_ = true;
    } else if ((scaler1 & 0777) == 0300 && (ruptLock_ || noRupt_)) {
      core.gojam(Core::kCh77RuptLock);
      ruptLock_ = false;
      noRupt_ = false;
      ++restartAlarmCount_;
    }

    if ((scaler1 & 037) == 020) {
      tcTrap_ = true;
      noTc_ = true;
    } else if ((scaler1 & 037) == 000 && (tcTrap_ || noTc_)) {
      core.gojam(Core::kCh77TcTrap);
      tcTrap_ = false;
      noTc_ = false;
      ++restartAlarmCount_;
    }
  }

  static bool counterPinc(Core& core,
                          uint16_t address,
                          uint8_t interruptNumber) {
    const uint16_t value = core.readErasable(address) & Core::kWordMask;
    const bool overflow = value == Core::kPositiveMask;
    uint16_t next = 0;

    if (overflow) {
      next = 0;
    } else {
      next = static_cast<uint16_t>((value + 1) & Core::kWordMask);
      if (next == 0) {
        next = 1;
      }
    }

    core.writeErasable(address, next);
    if (overflow && interruptNumber < Core::kInterruptCount) {
      core.requestInterrupt(interruptNumber);
    }
    return overflow;
  }

  int16_t scalerCounter_ = 0;
  uint16_t pendingExtraDelay_ = 0;
  uint8_t downlinkLatch_ = 0;
  uint16_t downlinkWord0_ = 0;
  uint16_t downlinkWord1_ = 0;
  DownlinkFrame downlinkFrames_[kDownlinkFrameQueueSize] = {};
  uint8_t downlinkFrameHead_ = 0;
  uint8_t downlinkFrameCount_ = 0;
  uint16_t downlinkFrameSequence_ = 0;
  uint32_t downlinkFrameDropped_ = 0;
  bool downruptTimeValid_ = false;
  uint32_t downruptTime_ = 0;
  bool nightWatchman_ = false;
  bool ruptLock_ = false;
  bool noRupt_ = false;
  bool tcTrap_ = false;
  bool noTc_ = false;
  bool restartMonitorsEnabled_ = true;
  uint8_t radarGateCounter_ = 0;
  uint16_t radarDataWord_ = 0;
  uint16_t radarQueue_[kRadarQueueSize] = {};
  uint8_t radarQueueHead_ = 0;
  uint8_t radarQueueCount_ = 0;
  uint32_t radarDropped_ = 0;
  uint16_t uplinkShiftRegister_ = 0;
  uint8_t uplinkBitCount_ = 0;
  uint32_t uplinkWordCount_ = 0;
  uint32_t uplinkParityRejectCount_ = 0;
  uint32_t scalerOverflowCount_ = 0;
  uint32_t stealCycleCount_ = 0;
  uint32_t counterPulseCount_ = 0;
  uint32_t time6PulseCount_ = 0;
  uint32_t downruptCount_ = 0;
  uint32_t restartAlarmCount_ = 0;
  uint32_t radarRuptCount_ = 0;
  uint32_t handruptCount_ = 0;
};

}  // namespace agc

#endif  // APOLLO11_EMBEDDED_AGC_MACHINE_TIMING_H
