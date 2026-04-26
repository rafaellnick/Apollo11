#ifndef APOLLO11_EMBEDDED_AGC_MACHINE_TIMING_H
#define APOLLO11_EMBEDDED_AGC_MACHINE_TIMING_H

#include "agc_core.h"

#include <stdint.h>

namespace agc {

class MachineTiming {
 public:
  MachineTiming() { reset(); }

  void reset() {
    scalerCounter_ = 0;
    pendingExtraDelay_ = 0;
    scalerOverflowCount_ = 0;
    stealCycleCount_ = 0;
    counterPulseCount_ = 0;
    time6PulseCount_ = 0;
  }

  uint16_t consumeStallCycles(Core& core) {
    if (!nextCycleWouldStall(core)) {
      return 0;
    }

    uint16_t consumed = 0;
    if (pendingExtraDelay_ > 0) {
      while (pendingExtraDelay_ > 0) {
        core.advanceCycles(1);
        scalerCounter_ += kScalerIncrement;
        --pendingExtraDelay_;
        ++consumed;
      }
      stealCycleCount_ += consumed;
      return consumed;
    }

    core.advanceCycles(1);
    scalerCounter_ += kScalerIncrement;
    consumed = 1;

    const uint16_t stalls = processDueScalerOverflows(core);
    if (stalls > 0) {
      for (uint16_t stall = 1; stall < stalls; ++stall) {
        core.advanceCycles(1);
        scalerCounter_ += kScalerIncrement;
        ++consumed;
      }
      stealCycleCount_ += consumed;
      return consumed;
    }

    return 0;
  }

  void observeCpuCycles(Core& core, uint32_t mct) {
    for (uint32_t i = 0; i < mct; ++i) {
      scalerCounter_ += kScalerIncrement;
      const uint16_t stalls = processDueScalerOverflows(core);
      if (stalls == 0) {
        continue;
      }

      core.advanceCycles(static_cast<uint8_t>(stalls));
      stealCycleCount_ += stalls;

      for (uint16_t stall = 0; stall < stalls; ++stall) {
        scalerCounter_ += kScalerIncrement;
      }
    }
  }

  uint32_t scalerOverflowCount() const { return scalerOverflowCount_; }
  uint32_t stealCycleCount() const { return stealCycleCount_; }
  uint32_t counterPulseCount() const { return counterPulseCount_; }
  uint32_t time6PulseCount() const { return time6PulseCount_; }

 private:
  static constexpr int16_t kScalerIncrement = 3;
  static constexpr int16_t kScalerOverflow = 80;
  static constexpr uint16_t kChannelScaler2 = 0003;
  static constexpr uint16_t kChannelScaler1 = 0004;

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
  uint32_t scalerOverflowCount_ = 0;
  uint32_t stealCycleCount_ = 0;
  uint32_t counterPulseCount_ = 0;
  uint32_t time6PulseCount_ = 0;
};

}  // namespace agc

#endif  // APOLLO11_EMBEDDED_AGC_MACHINE_TIMING_H
