#ifndef APOLLO11_EMBEDDED_AGC_CORE_H
#define APOLLO11_EMBEDDED_AGC_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace agc {

struct RopeImage {
  const uint16_t* words;
  uint8_t bankCount;
  const char* name;
};

class Core {
 public:
  static constexpr uint16_t kWordMask = 077777;
  static constexpr uint16_t kSignBit = 040000;
  static constexpr uint16_t kPositiveMask = 037777;
  static constexpr uint16_t kAddressMask = 07777;
  static constexpr uint16_t kErasableWords = 04000;
  static constexpr uint16_t kErasableBankSize = 00400;
  static constexpr uint16_t kSwitchedErasableBase = 01400;
  static constexpr uint16_t kCommonFixedBase = 02000;
  static constexpr uint16_t kFixedFixedBank2Base = 04000;
  static constexpr uint16_t kFixedFixedBank3Base = 06000;
  static constexpr uint16_t kFixedBankSize = 02000;
  static constexpr uint8_t kFixedBanks = 36;
  static constexpr size_t kFixedWords =
      static_cast<size_t>(kFixedBanks) * kFixedBankSize;
  static constexpr uint16_t kLow10Mask = 01777;
  static constexpr uint16_t kIoAddressMask = 00777;
  static constexpr uint16_t kQuarterMask = 06000;
  static constexpr uint16_t kIoChannels = 01000;
  static constexpr uint8_t kInterruptCount = 10;
  static constexpr uint8_t kNoInterrupt = 0xff;
  static constexpr uint8_t kRuptEntryMct = 2;

  static constexpr uint16_t kRegA = 00000;
  static constexpr uint16_t kRegL = 00001;
  static constexpr uint16_t kRegQ = 00002;
  static constexpr uint16_t kRegEBANK = 00003;
  static constexpr uint16_t kRegFBANK = 00004;
  static constexpr uint16_t kRegZ = 00005;
  static constexpr uint16_t kRegBBANK = 00006;
  static constexpr uint16_t kRegZERO = 00007;
  static constexpr uint16_t kRegARUPT = 00010;
  static constexpr uint16_t kRegLRUPT = 00011;
  static constexpr uint16_t kRegQRUPT = 00012;
  static constexpr uint16_t kRegSAMPTIME1 = 00013;
  static constexpr uint16_t kRegSAMPTIME2 = 00014;
  static constexpr uint16_t kRegZRUPT = 00015;
  static constexpr uint16_t kRegBBRUPT = 00016;
  static constexpr uint16_t kRegBRUPT = 00017;
  static constexpr uint16_t kRegCYR = 00020;
  static constexpr uint16_t kRegSR = 00021;
  static constexpr uint16_t kRegCYL = 00022;
  static constexpr uint16_t kRegEDOP = 00023;
  static constexpr uint16_t kRegTIME2 = 00024;
  static constexpr uint16_t kRegTIME1 = 00025;
  static constexpr uint16_t kRegTIME3 = 00026;
  static constexpr uint16_t kRegTIME4 = 00027;
  static constexpr uint16_t kRegTIME5 = 00030;
  static constexpr uint16_t kRegTIME6 = 00031;
  static constexpr uint16_t kRegINLINK = 00045;
  static constexpr uint16_t kRegRNRAD = 00046;
  static constexpr uint16_t kRegOUTLINK = 00057;

  static constexpr uint8_t kInterruptT6Rupt = 0;
  static constexpr uint8_t kInterruptT5Rupt = 1;
  static constexpr uint8_t kInterruptT3Rupt = 2;
  static constexpr uint8_t kInterruptT4Rupt = 3;
  static constexpr uint8_t kInterruptKeyrupt1 = 4;
  static constexpr uint8_t kInterruptKeyrupt2 = 5;
  static constexpr uint8_t kInterruptUprupt = 6;
  static constexpr uint8_t kInterruptDownrupt = 7;
  static constexpr uint8_t kInterruptRadarRupt = 8;
  static constexpr uint8_t kInterruptHandrupt = 9;

  static constexpr uint16_t kInstructionRelint = 00003;
  static constexpr uint16_t kInstructionInhint = 00004;
  static constexpr uint16_t kInstructionExtend = 00006;
  static constexpr uint16_t kInstructionResume = 050017;

  static constexpr uint16_t kBootAddress = 04000;
  static constexpr uint16_t kPanelCounter = 00100;
  static constexpr uint16_t kPanelProgram = 00101;
  static constexpr uint16_t kPanelVerb = 00102;
  static constexpr uint16_t kPanelNoun = 00103;
  static constexpr uint16_t kPanelLastKey = 00104;
  static constexpr uint16_t kPanelAlarm = 00105;
  static constexpr uint16_t kInputRhcX = 00110;
  static constexpr uint16_t kInputRhcY = 00111;
  static constexpr uint16_t kInputRhcSwitch = 00112;

  static constexpr uint16_t kChannelDSKY = 0010;
  static constexpr uint16_t kChannelKey = 0015;
  static constexpr uint16_t kChannelOut0 = 0010;
  static constexpr uint16_t kChannelOut1 = 0011;
  static constexpr uint16_t kChannelOut2 = 0012;
  static constexpr uint16_t kChannelOut3 = 0013;
  static constexpr uint16_t kChannelCounterEnable = 0013;
  static constexpr uint16_t kChannelDownlinkWord0 = 0034;
  static constexpr uint16_t kChannelDownlinkWord1 = 0035;
  static constexpr uint16_t kChannelRestartMonitor = 0077;
  static constexpr uint16_t kChannelUplink = 0173;

  static constexpr uint16_t kChannel13Trap31A = 004000;
  static constexpr uint16_t kChannel13Trap31B = 010000;
  static constexpr uint16_t kChannel13Trap32 = 020000;
  static constexpr uint16_t kChannel13RadarActivity = 000010;
  static constexpr uint16_t kChannel33UplinkTooFast = 002000;

  static constexpr uint16_t kCh77ParityFail = 000001;
  static constexpr uint16_t kCh77TcTrap = 000004;
  static constexpr uint16_t kCh77RuptLock = 000010;
  static constexpr uint16_t kCh77NightWatchman = 000020;

  static constexpr uint8_t kHardwareWriteDownlinkWord0 = 1U << 0;
  static constexpr uint8_t kHardwareWriteDownlinkWord1 = 1U << 1;

  enum class RunState : uint8_t {
    Halted = 0,
    Running,
    Faulted,
  };

  enum class Fault : uint8_t {
    None = 0,
    FixedWrite,
    BadFixedAddress,
    ExtendedNotImplemented,
    DivideByZero,
    BadRopeImage,
  };

  Core() { reset(); }

  void reset() {
    memset(erasable_, 0, sizeof(erasable_));
    memset(fixed_, 0, sizeof(fixed_));
    initializeChannels();
    runState_ = RunState::Halted;
    fault_ = Fault::None;
    cycles_ = 0;
    indexPending_ = false;
    indexValue_ = 0;
    extendPending_ = false;
    forcedInstructionPending_ = false;
    forcedInstruction_ = 0;
    interruptsEnabled_ = true;
    interruptsInhibited_ = false;
    inInterruptService_ = false;
    accumulatorOverflow_ = 0;
    pendingInterruptMask_ = 0;
    activeInterrupt_ = 0;
    hardwareWriteFlags_ = 0;
    tcEvent_ = false;
    interruptEntryEvent_ = false;
    nightWatchmanServiced_ = false;
    trap31A_ = false;
    trap31B_ = false;
    trap32_ = false;
    restartLight_ = false;
    lastInstruction_ = 0;
    lastAddress_ = 0;
    lastOpcode_ = 0;
    lastExtended_ = false;
    lastInstructionMct_ = 0;
    loadBringupProgram();
  }

  void loadBringupProgram() {
    memset(fixed_, 0, sizeof(fixed_));

    setZ(kBootAddress);
    writeErasable(kPanelProgram, fromInt(0));
    writeErasable(kPanelVerb, fromInt(16));
    writeErasable(kPanelNoun, fromInt(36));
    writeErasable(kPanelCounter, fromInt(0));
    writeErasable(kPanelAlarm, fromInt(0));
    writeErasable(kInputRhcX, fromInt(0));
    writeErasable(kInputRhcY, fromInt(0));
    writeErasable(kInputRhcSwitch, fromInt(0));

    const uint16_t constComplementOne = kBootAddress + 0010;

    writeFixed(kBootAddress + 0000, encodeBasic(4, constComplementOne));
    writeFixed(kBootAddress + 0001, encodeBasic(6, kPanelCounter));
    writeFixed(kBootAddress + 0002,
               static_cast<uint16_t>(054000 | kPanelCounter));
    writeFixed(kBootAddress + 0003, encodeBasic(0, kBootAddress));
    writeFixed(constComplementOne, negate(fromInt(1)));
  }

  void start() {
    if (fault_ == Fault::None) {
      runState_ = RunState::Running;
    }
  }

  void halt() {
    if (runState_ != RunState::Faulted) {
      runState_ = RunState::Halted;
    }
  }

  bool step() {
    if (runState_ == RunState::Faulted) {
      return false;
    }

    if (serviceInterrupt()) {
      return true;
    }

    const uint16_t pc = getZ();
    uint16_t instruction = 0;
    if (forcedInstructionPending_) {
      instruction = forcedInstruction_;
      forcedInstructionPending_ = false;
      forcedInstruction_ = 0;
    } else {
      instruction = read(pc);
    }
    setZ(incrementAddress(pc, 1));

    bool indexedInstruction = false;
    if (indexPending_) {
      instruction = applyIndex(instruction, indexValue_);
      indexPending_ = false;
      indexValue_ = 0;
      indexedInstruction = true;
    }

    lastInstruction_ = instruction;
    lastAddress_ = pc;
    lastOpcode_ = static_cast<uint8_t>((instruction >> 12) & 07);
    lastExtended_ = extendPending_;

    if (extendPending_) {
      extendPending_ = false;
      lastInstructionMct_ = defaultInstructionMct(instruction, true);
      executeExtended(instruction);
    } else {
      lastInstructionMct_ = defaultInstructionMct(instruction, false);
      executeBasic(instruction, indexedInstruction);
    }

    cycles_ += lastInstructionMct_;
    return runState_ != RunState::Faulted;
  }

  void runFor(uint16_t instructionCount) {
    for (uint16_t i = 0; i < instructionCount; ++i) {
      if (runState_ != RunState::Running || !step()) {
        return;
      }
    }
  }

  uint16_t read(uint16_t address) const {
    address &= kAddressMask;

    if (isErasableAddress(address)) {
      return readErasable(resolveErasableIndex(address));
    }

    size_t index = 0;
    if (!fixedIndex(address, &index)) {
      return 0;
    }

    return fixed_[index] & kWordMask;
  }

  bool write(uint16_t address, uint16_t value) {
    address &= kAddressMask;
    value &= kWordMask;

    if (isErasableAddress(address)) {
      writeErasable(resolveErasableIndex(address), value);
      return true;
    }

    setFault(Fault::FixedWrite);
    return false;
  }

  uint16_t readErasable(uint16_t address) const {
    address &= (kErasableWords - 1);
    noteErasableAccess(address);
    if (address == kRegZERO) {
      return 0;
    }

    const uint16_t value = erasable_[address] & kWordMask;
    if (isEditingRegister(address)) {
      erasable_[address] = editValueForAddress(address, value);
    }
    return value;
  }

  void writeErasable(uint16_t address, uint16_t value) {
    address &= (kErasableWords - 1);
    value &= kWordMask;
    noteErasableAccess(address);

    if (address == kRegZERO) {
      return;
    }

    if (address == kRegA) {
      accumulatorOverflow_ = 0;
    }

    if (isBankRegister(address)) {
      writeBankRegister(address, value);
      return;
    }

    erasable_[address] = editValueForAddress(address, value);
  }

  bool writeFixed(uint16_t address, uint16_t value) {
    size_t index = 0;
    if (!fixedIndex(address, &index)) {
      setFault(Fault::BadFixedAddress);
      return false;
    }

    fixed_[index] = value & kWordMask;
    return true;
  }

  bool writeFixedBank(uint8_t bank, uint16_t offset, uint16_t value) {
    if (bank >= kFixedBanks || offset >= kFixedBankSize) {
      setFault(Fault::BadFixedAddress);
      return false;
    }

    fixed_[static_cast<size_t>(bank) * kFixedBankSize + offset] =
        value & kWordMask;
    return true;
  }

  bool loadFixedBank(uint8_t bank, const uint16_t* words, size_t count) {
    if (bank >= kFixedBanks || words == nullptr || count > kFixedBankSize) {
      setFault(Fault::BadRopeImage);
      return false;
    }

    for (size_t i = 0; i < kFixedBankSize; ++i) {
      fixed_[static_cast<size_t>(bank) * kFixedBankSize + i] =
          i < count ? words[i] & kWordMask : 0;
    }

    return true;
  }

  bool loadRopeImage(const RopeImage& image) {
    if (image.words == nullptr || image.bankCount == 0 ||
        image.bankCount > kFixedBanks) {
      setFault(Fault::BadRopeImage);
      return false;
    }

    memset(erasable_, 0, sizeof(erasable_));
    initializeChannels();
    runState_ = RunState::Halted;
    fault_ = Fault::None;
    cycles_ = 0;
    indexPending_ = false;
    indexValue_ = 0;
    extendPending_ = false;
    forcedInstructionPending_ = false;
    forcedInstruction_ = 0;
    interruptsEnabled_ = true;
    interruptsInhibited_ = false;
    inInterruptService_ = false;
    accumulatorOverflow_ = 0;
    pendingInterruptMask_ = 0;
    activeInterrupt_ = 0;
    hardwareWriteFlags_ = 0;
    tcEvent_ = false;
    interruptEntryEvent_ = false;
    nightWatchmanServiced_ = false;
    trap31A_ = false;
    trap31B_ = false;
    trap32_ = false;
    restartLight_ = false;
    lastInstruction_ = 0;
    lastAddress_ = 0;
    lastOpcode_ = 0;
    lastExtended_ = false;
    lastInstructionMct_ = 0;

    memset(fixed_, 0, sizeof(fixed_));
    for (uint8_t bank = 0; bank < image.bankCount; ++bank) {
      if (!loadFixedBank(bank, image.words + bank * kFixedBankSize,
                         kFixedBankSize)) {
        return false;
      }
    }

    setZ(kBootAddress);
    return true;
  }

  static bool hasOddParity16(uint16_t physicalWord) {
    uint8_t ones = 0;
    for (uint8_t bit = 0; bit < 16; ++bit) {
      if ((physicalWord & (1U << bit)) != 0) {
        ++ones;
      }
    }
    return (ones & 1U) != 0;
  }

  static uint16_t stripParityBit(uint16_t physicalWord) {
    return static_cast<uint16_t>((physicalWord >> 1) & kWordMask);
  }

  static uint16_t appendOddParityBit(uint16_t dataWord) {
    uint16_t physicalWord =
        static_cast<uint16_t>((dataWord & kWordMask) << 1);
    if (!hasOddParity16(physicalWord)) {
      physicalWord |= 1U;
    }
    return physicalWord;
  }

  static bool physicalWordToData(uint16_t physicalWord, uint16_t* dataOut) {
    if (!hasOddParity16(physicalWord)) {
      return false;
    }
    if (dataOut != nullptr) {
      *dataOut = stripParityBit(physicalWord);
    }
    return true;
  }

  uint16_t readChannel(uint16_t channel) const {
    channel &= kIoAddressMask;
    if (channel == kRegL || channel == kRegQ) {
      return readErasable(channel);
    }
    return channels_[channel & (kIoChannels - 1)] & kWordMask;
  }

  uint16_t readOutputChannel10(uint8_t row) const {
    return outputChannel10_[row & 017] & kWordMask;
  }

  void writeChannel(uint16_t channel, uint16_t value) {
    channel &= kIoAddressMask;
    value &= kWordMask;
    if (channel == kRegL || channel == kRegQ) {
      writeErasable(channel, value);
      return;
    }
    channels_[channel & (kIoChannels - 1)] = value;
  }

  uint8_t takeHardwareWriteFlags() {
    const uint8_t flags = hardwareWriteFlags_;
    hardwareWriteFlags_ = 0;
    return flags;
  }

  bool takeTcEvent() {
    const bool event = tcEvent_;
    tcEvent_ = false;
    return event;
  }

  bool takeInterruptEntryEvent() {
    const bool event = interruptEntryEvent_;
    interruptEntryEvent_ = false;
    return event;
  }

  bool takeNightWatchmanService() const {
    const bool serviced = nightWatchmanServiced_;
    nightWatchmanServiced_ = false;
    return serviced;
  }

  bool inInterruptService() const { return inInterruptService_; }
  bool restartLight() const { return restartLight_; }

  void receiveUplinkWord(uint16_t word) {
    if ((pendingInterruptMask_ & (1U << kInterruptUprupt)) != 0) {
      channels_[0033] &= static_cast<uint16_t>(~kChannel33UplinkTooFast);
    }
    writeErasable(kRegINLINK, word & kWordMask);
    requestInterrupt(kInterruptUprupt);
  }

  void serviceHandruptTraps() {
    if (trap31A_ && (readChannel(0031) & 000077) != 000077) {
      trap31A_ = false;
      requestInterrupt(kInterruptHandrupt);
    }
    if (trap31B_ && (readChannel(0031) & 007700) != 007700) {
      trap31B_ = false;
      requestInterrupt(kInterruptHandrupt);
    }
    if (trap32_ && (readChannel(0032) & 001777) != 001777) {
      trap32_ = false;
      requestInterrupt(kInterruptHandrupt);
    }
  }

  void setRestartMonitorAlarm(uint16_t alarmBits) {
    channels_[kChannelRestartMonitor] =
        static_cast<uint16_t>((channels_[kChannelRestartMonitor] |
                               (alarmBits & kWordMask)) &
                              kWordMask);
  }

  void gojam(uint16_t alarmBits) {
    setRestartMonitorAlarm(alarmBits);
    writeErasable(kRegQ, getZ());
    setZ(kBootAddress);
    interruptsEnabled_ = true;
    interruptsInhibited_ = false;
    inInterruptService_ = false;
    accumulatorOverflow_ = 0;
    pendingInterruptMask_ = 0;
    activeInterrupt_ = 0;
    indexPending_ = false;
    indexValue_ = 0;
    extendPending_ = false;
    forcedInstructionPending_ = false;
    forcedInstruction_ = 0;
    trap31A_ = false;
    trap31B_ = false;
    trap32_ = false;
    restartLight_ = true;

    writeChannel(0005, 0);
    writeChannel(0006, 0);
    writeChannel(0010, 0);
    writeChannel(0011, 0);
    writeChannel(0012, 0);
    writeChannel(0013, 0);
    writeChannel(0014, 0);
    channels_[0033] |= kChannel33UplinkTooFast;
    writeChannel(kChannelDownlinkWord0, 0);
    writeChannel(kChannelDownlinkWord1, 0);
    hardwareWriteFlags_ = 0;
  }

  void setInterruptsEnabled(bool enabled) {
    interruptsEnabled_ = enabled;
    if (enabled) {
      interruptsInhibited_ = false;
    }
  }

  void requestInterrupt(uint8_t interruptNumber) {
    if (interruptNumber < kInterruptCount) {
      pendingInterruptMask_ |= static_cast<uint16_t>(1U << interruptNumber);
    }
  }

  void clearInterrupt(uint8_t interruptNumber) {
    if (interruptNumber < kInterruptCount) {
      pendingInterruptMask_ &=
          static_cast<uint16_t>(~static_cast<uint16_t>(1U << interruptNumber));
    }
  }

  bool serviceInterruptEntry(uint16_t* interruptedPcOut = nullptr,
                             uint16_t* interruptedInstructionOut = nullptr) {
    if (!canEnterInterrupt()) {
      return false;
    }

    const uint16_t interruptedPc = getZ();
    uint16_t interruptedInstruction =
        forcedInstructionPending_ ? forcedInstruction_ : read(interruptedPc);
    if (!indexPending_ &&
        isInterruptBoundaryInstruction(interruptedInstruction)) {
      return false;
    }

    for (uint8_t i = 0; i < kInterruptCount; ++i) {
      if ((pendingInterruptMask_ & (1U << i)) == 0) {
        continue;
      }

      pendingInterruptMask_ &=
          static_cast<uint16_t>(~static_cast<uint16_t>(1U << i));
      activeInterrupt_ = i;
      if (indexPending_) {
        interruptedInstruction =
            applyIndex(interruptedInstruction, indexValue_);
        indexPending_ = false;
        indexValue_ = 0;
      }
      return enterInterrupt(i, interruptedPc, interruptedInstruction,
                            interruptedPcOut, interruptedInstructionOut);
    }

    return false;
  }

  uint16_t pendingInterruptMask() const { return pendingInterruptMask_; }
  bool interruptsEnabled() const { return interruptsEnabled_; }
  bool lastInstructionExtended() const { return lastExtended_; }

  bool counterPinc(uint16_t address,
                   uint8_t interruptNumber = kNoInterrupt) {
    const uint16_t value = readErasable(address);
    const bool overflow =
        (value & kSignBit) == 0 && (value & kPositiveMask) == kPositiveMask;
    writeErasable(address, overflow ? 0 : addOnesComplement(value, fromInt(1)));
    if (overflow && interruptNumber < kInterruptCount) {
      requestInterrupt(interruptNumber);
    }
    return overflow;
  }

  bool counterMinc(uint16_t address,
                   uint8_t interruptNumber = kNoInterrupt) {
    const uint16_t value = readErasable(address);
    const bool underflow = value == kWordMask;
    writeErasable(address, underflow ? kWordMask
                                     : addOnesComplement(value,
                                                         negate(fromInt(1))));
    if (underflow && interruptNumber < kInterruptCount) {
      requestInterrupt(interruptNumber);
    }
    return underflow;
  }

  bool counterDinc(uint16_t address,
                   uint8_t interruptNumber = kNoInterrupt) {
    const uint16_t value = readErasable(address);
    bool reachedZero = false;

    if (value == 0 || value == kWordMask) {
      reachedZero = true;
    } else if ((value & kSignBit) != 0) {
      writeErasable(address, addOnesComplement(value, fromInt(1)));
      const uint16_t next = readErasable(address);
      reachedZero = next == 0 || next == kWordMask;
    } else {
      writeErasable(address, addOnesComplement(value, negate(fromInt(1))));
      const uint16_t next = readErasable(address);
      reachedZero = next == 0 || next == kWordMask;
    }

    if (reachedZero && interruptNumber < kInterruptCount) {
      requestInterrupt(interruptNumber);
    }
    return reachedZero;
  }

  bool counterShift(uint16_t address,
                    bool oneBit,
                    uint8_t interruptNumber = kNoInterrupt) {
    const uint16_t value = readErasable(address);
    const bool overflow = (value & kSignBit) != 0;
    uint16_t next = static_cast<uint16_t>((value << 1) & kWordMask);
    if (oneBit) {
      next |= 1;
    }
    writeErasable(address, next);
    if (overflow && interruptNumber < kInterruptCount) {
      requestInterrupt(interruptNumber);
    }
    return overflow;
  }

  uint16_t getA() const { return readErasable(kRegA); }
  void setA(uint16_t value) { setAWithOverflow(value, 0); }

  uint16_t getZ() const { return readErasable(kRegZ) & kAddressMask; }
  void setZ(uint16_t address) { writeErasable(kRegZ, address & kAddressMask); }

  uint32_t cycles() const { return cycles_; }
  void advanceCycles(uint32_t mct) { cycles_ += mct; }
  RunState runState() const { return runState_; }
  Fault fault() const { return fault_; }
  uint16_t previewAddress() const { return getZ(); }
  uint16_t previewInstruction() const {
    uint16_t instruction =
        forcedInstructionPending_ ? forcedInstruction_ : read(getZ());
    if (indexPending_) {
      instruction = applyIndex(instruction, indexValue_);
    }
    return instruction & kWordMask;
  }
  bool previewInstructionExtended() const { return extendPending_; }
  uint16_t lastInstruction() const { return lastInstruction_; }
  uint16_t lastAddress() const { return lastAddress_; }
  uint8_t lastOpcode() const { return lastOpcode_; }
  uint8_t lastInstructionMct() const { return lastInstructionMct_; }

  static uint16_t encodeBasic(uint8_t opcode, uint16_t operand) {
    return static_cast<uint16_t>(((opcode & 07) << 12) |
                                (operand & kAddressMask));
  }

  static uint16_t addOnesComplement(uint16_t left, uint16_t right) {
    uint32_t sum = static_cast<uint32_t>(left & kWordMask) +
                   static_cast<uint32_t>(right & kWordMask);
    sum = (sum & kWordMask) + (sum >> 15);
    sum = (sum & kWordMask) + (sum >> 15);
    return static_cast<uint16_t>(sum & kWordMask);
  }

  static uint32_t addSp16(uint32_t left, uint32_t right) {
    uint32_t sum = (left & 0177777U) + (right & 0177777U);
    if ((sum & 0200000U) != 0) {
      sum = (sum + 1U) & 0177777U;
    }
    return sum & 0177777U;
  }

  static int8_t valueOverflowed16(uint32_t value) {
    switch (value & 0140000U) {
      case 0040000U:
        return 1;
      case 0100000U:
        return -1;
      default:
        return 0;
    }
  }

  static uint16_t overflowCorrected16(uint32_t value) {
    return static_cast<uint16_t>(((value & 037777U) |
                                  ((value >> 1) & kSignBit)) &
                                 kWordMask);
  }

  static uint32_t signExtendSp16(uint16_t word) {
    return ((word & kWordMask) | ((word << 1) & 0100000U)) & 0177777U;
  }

  static uint16_t absSp(uint16_t value) {
    value &= kWordMask;
    return (value & kSignBit) != 0
               ? static_cast<uint16_t>((~value) & kWordMask)
               : value;
  }

  static int32_t agcSpToCpu(uint16_t value) {
    value &= kWordMask;
    return (value & kSignBit) != 0
               ? -static_cast<int32_t>((~value) & kPositiveMask)
               : static_cast<int32_t>(value & kPositiveMask);
  }

  static uint16_t cpuToAgcSp(int32_t value) {
    if (value < 0) {
      return static_cast<uint16_t>((~static_cast<uint32_t>(-value)) &
                                   kWordMask);
    }
    return static_cast<uint16_t>(value & kWordMask);
  }

  static int32_t agcDpToCpu(uint32_t value) {
    value &= 03777777777U;
    if ((value & 02000000000U) != 0) {
      return -static_cast<int32_t>((~value) & 01777777777U);
    }
    return static_cast<int32_t>(value & 01777777777U);
  }

  static uint32_t cpuToAgcDp(int32_t value) {
    if (value < 0) {
      return (~static_cast<uint32_t>(-value)) & 03777777777U;
    }
    return static_cast<uint32_t>(value) & 01777777777U;
  }

  static uint32_t spToDecent(uint16_t msbIn, uint16_t lsbIn) {
    uint16_t msb = msbIn & kWordMask;
    uint16_t lsb = lsbIn & kWordMask;

    if (msb == 0 || msb == kWordMask) {
      uint32_t value = signExtendSp16(lsb);
      if ((value & 0100000U) != 0) {
        value |= ~0177777U;
      }
      return value & 07777777777U;
    }

    if ((lsb & kSignBit) != (msb & kSignBit)) {
      if (lsb == 0 || lsb == kWordMask) {
        lsb = (msb & kSignBit) == 0 ? 0 : kWordMask;
      } else {
        const bool complement = (msb & kSignBit) != 0;
        if (complement) {
          msb = static_cast<uint16_t>((~msb) & kWordMask);
          lsb = static_cast<uint16_t>((~lsb) & kWordMask);
        }
        msb = static_cast<uint16_t>((msb - 1U) & kWordMask);
        lsb = static_cast<uint16_t>((lsb + 040000U + 1U) & kWordMask);
        if (complement) {
          msb = static_cast<uint16_t>((~msb) & kWordMask);
          lsb = static_cast<uint16_t>((~lsb) & kWordMask);
        }
      }
    }

    uint32_t value =
        (03777740000U & (static_cast<uint32_t>(msb) << 14)) |
        (037777U & lsb);
    if ((value & 02000000000U) != 0) {
      value |= 04000000000U;
    }
    return value & 07777777777U;
  }

  static void decentToSp(uint32_t decent, uint16_t& msb, uint16_t& lsb) {
    const bool negative = (decent & 04000000000U) != 0;
    lsb = static_cast<uint16_t>(decent & 037777U);
    if (negative) {
      lsb |= kSignBit;
    }
    msb = overflowCorrected16((decent >> 14) & 0177777U);
  }

  static uint16_t negate(uint16_t word) { return (~word) & kWordMask; }

  static int16_t toInt(uint16_t word) {
    word &= kWordMask;

    if (word == kWordMask) {
      return 0;
    }

    if ((word & kSignBit) == 0) {
      return static_cast<int16_t>(word & kPositiveMask);
    }

    return static_cast<int16_t>(-static_cast<int16_t>((~word) & kWordMask));
  }

  static uint16_t fromInt(int16_t value) {
    if (value < 0) {
      return negate(static_cast<uint16_t>(abs(value)) & kPositiveMask);
    }

    return static_cast<uint16_t>(value) & kPositiveMask;
  }

  static uint16_t interruptVectorAddress(uint8_t interruptNumber) {
    return interruptNumber < kInterruptCount
               ? static_cast<uint16_t>(04004 + interruptNumber * 4U)
               : kBootAddress;
  }

 private:
  enum class SignClass : uint8_t {
    Positive,
    PositiveZero,
    Negative,
    NegativeZero,
  };

  static uint16_t incrementAddress(uint16_t address, uint16_t amount) {
    return static_cast<uint16_t>((address + amount) & kAddressMask);
  }

  static uint16_t applyIndex(uint16_t instruction, uint16_t indexValue) {
    return addOnesComplement(instruction, indexValue);
  }

  static SignClass classify(uint16_t word) {
    word &= kWordMask;

    if (word == 0) {
      return SignClass::PositiveZero;
    }

    if (word == kWordMask) {
      return SignClass::NegativeZero;
    }

    if ((word & kSignBit) != 0) {
      return SignClass::Negative;
    }

    return SignClass::Positive;
  }

  static uint16_t decrementMagnitude(uint16_t positiveMagnitude) {
    positiveMagnitude &= kPositiveMask;

    if (positiveMagnitude == 0) {
      return 0;
    }

    return static_cast<uint16_t>((positiveMagnitude - 1) & kPositiveMask);
  }

  static uint16_t rotateRight(uint16_t word) {
    word &= kWordMask;
    return static_cast<uint16_t>((word >> 1) | ((word & 1U) << 14));
  }

  static uint16_t rotateLeft(uint16_t word) {
    word &= kWordMask;
    return static_cast<uint16_t>(((word << 1) & kWordMask) |
                                 ((word >> 14) & 1U));
  }

  static uint16_t shiftRight(uint16_t word) {
    word &= kWordMask;
    return static_cast<uint16_t>((word & kSignBit) |
                                 ((word >> 1) & kPositiveMask));
  }

  static bool isEditingRegister(uint16_t address) {
    address &= (kErasableWords - 1);
    return address == kRegCYR || address == kRegSR ||
           address == kRegCYL || address == kRegEDOP;
  }

  static bool isBankRegister(uint16_t address) {
    address &= (kErasableWords - 1);
    return address == kRegEBANK || address == kRegFBANK ||
           address == kRegBBANK;
  }

  void initializeChannels() {
    memset(channels_, 0, sizeof(channels_));
    memset(outputChannel10_, 0, sizeof(outputChannel10_));
    channels_[0030] = 037777;
    channels_[0031] = kWordMask;
    channels_[0032] = kWordMask;
    channels_[0033] = kWordMask;
  }

  void writeBankRegister(uint16_t address, uint16_t value) {
    if (address == kRegEBANK) {
      erasable_[kRegEBANK] = value & 03400;
    } else if (address == kRegFBANK) {
      erasable_[kRegFBANK] = value & 076000;
    } else {
      erasable_[kRegBBANK] = value & 076007;
      erasable_[kRegFBANK] = erasable_[kRegBBANK] & 076000;
      erasable_[kRegEBANK] =
          static_cast<uint16_t>((erasable_[kRegBBANK] & 07) << 8);
      return;
    }

    erasable_[kRegBBANK] =
        static_cast<uint16_t>((erasable_[kRegFBANK] & 076000) |
                              ((erasable_[kRegEBANK] & 03400) >> 8));
  }

  static uint16_t editValueForAddress(uint16_t address, uint16_t value) {
    value &= kWordMask;
    switch (address & (kErasableWords - 1)) {
      case kRegCYR:
        return rotateRight(value);
      case kRegSR:
        return shiftRight(value);
      case kRegCYL:
        return rotateLeft(value);
      case kRegEDOP:
        return editPolishOpcode(value);
      default:
        return value;
    }
  }

  static uint16_t editPolishOpcode(uint16_t word) {
    return static_cast<uint16_t>((word >> 7) & 0177);
  }

  static uint8_t instructionCode(uint16_t instruction) {
    return static_cast<uint8_t>((instruction >> 12) & 07);
  }

  static uint8_t quarterCode(uint16_t instruction) {
    return static_cast<uint8_t>((instruction >> 10) & 03);
  }

  static uint8_t peripheralCode(uint16_t instruction) {
    return static_cast<uint8_t>((instruction >> 9) & 07);
  }

  static uint16_t address12(uint16_t instruction) {
    return instruction & kAddressMask;
  }

  static uint16_t address10(uint16_t instruction) {
    return instruction & kLow10Mask;
  }

  static uint16_t previousAddress10(uint16_t instruction) {
    return static_cast<uint16_t>((instruction - 1U) & kLow10Mask);
  }

  static uint16_t previousAddress12(uint16_t instruction) {
    return static_cast<uint16_t>((instruction - 1U) & kAddressMask);
  }

  static uint16_t nextAddress12(uint16_t address) {
    return static_cast<uint16_t>((address + 1U) & kAddressMask);
  }

  static uint16_t nextAddress10(uint16_t address) {
    return static_cast<uint16_t>((address + 1U) & kLow10Mask);
  }

  static uint16_t channelAddress(uint16_t instruction) {
    return instruction & kIoAddressMask;
  }

  static bool isLow10Operand(uint16_t operand) {
    return (operand & static_cast<uint16_t>(~kLow10Mask)) == 0;
  }

  static uint8_t defaultInstructionMct(uint16_t instruction, bool extended) {
    const uint8_t opcode = instructionCode(instruction);
    const uint8_t quarter = quarterCode(instruction);
    const uint16_t operand = address12(instruction);

    if (!extended) {
      if (opcode == 0) {
        return 1;
      }
      if (opcode == 1 && !isLow10Operand(operand)) {
        return 1;
      }
      if (opcode == 2 && quarter == 0) {
        return 3;
      }
      if (opcode == 5 && quarter == 1) {
        return 3;
      }
      return 2;
    }

    if (opcode == 1 && isLow10Operand(operand)) {
      return 6;
    }
    if (opcode == 3 || opcode == 4 || opcode == 7) {
      return 3;
    }
    return 2;
  }

  static bool isInterruptBoundaryInstruction(uint16_t instruction) {
    instruction &= kWordMask;
    return instruction == kInstructionRelint ||
           instruction == kInstructionInhint ||
           instruction == kInstructionExtend;
  }

  static bool isErasableAddress(uint16_t address) {
    return (address & kAddressMask) < kCommonFixedBase;
  }

  uint16_t resolveErasableIndex(uint16_t address) const {
    address &= kAddressMask;
    if (address < kSwitchedErasableBase) {
      return address;
    }

    const uint16_t offset =
        static_cast<uint16_t>(address - kSwitchedErasableBase);
    return static_cast<uint16_t>(
        (selectedErasableBank() * kErasableBankSize + offset) &
        (kErasableWords - 1));
  }

  uint8_t selectedErasableBank() const {
    return static_cast<uint8_t>((erasable_[kRegEBANK] >> 8) & 07);
  }

  uint8_t selectedFixedBank() const {
    uint8_t bank =
        static_cast<uint8_t>((erasable_[kRegFBANK] >> 10) & 037);
    if ((channels_[0007] & 0100) != 0 && bank >= 030 && bank <= 033) {
      bank = static_cast<uint8_t>(bank + 010);
    }
    return bank < kFixedBanks ? bank : static_cast<uint8_t>(bank % kFixedBanks);
  }

  bool fixedIndex(uint16_t address, size_t* index) const {
    address &= kAddressMask;

    if (address < kCommonFixedBase) {
      return false;
    }

    uint8_t bank = 0;
    if (address < kFixedFixedBank2Base) {
      bank = selectedFixedBank();
    } else if (address < kFixedFixedBank3Base) {
      bank = 2;
    } else {
      bank = 3;
    }
    const uint16_t offset = address & (kFixedBankSize - 1);
    const size_t resolved =
        static_cast<size_t>(bank) * kFixedBankSize + offset;

    if (resolved >= kFixedWords) {
      return false;
    }

    if (index != nullptr) {
      *index = resolved;
    }

    return true;
  }

  void executeBasic(uint16_t instruction, bool indexedInstruction) {
    const uint8_t opcode = instructionCode(instruction);
    const uint8_t quarter = quarterCode(instruction);
    const uint16_t operand = address12(instruction);
    const uint16_t erasable = address10(instruction);

    switch (opcode) {
      case 0:
        if (!indexedInstruction && operand == kInstructionRelint) {
          interruptsEnabled_ = true;
        } else if (!indexedInstruction && operand == kInstructionInhint) {
          interruptsEnabled_ = false;
        } else if (!indexedInstruction && operand == kInstructionExtend) {
          extendPending_ = true;
        } else {
          executeTc(operand);
        }
        break;
      case 1:
        if (!isLow10Operand(operand)) {
          setZ(operand);
        } else {
          executeCcs(erasable);
        }
        break;
      case 2:
        executeBasicQuarter2(instruction, quarter);
        break;
      case 3:
        setA(read(operand));
        break;
      case 4:
        setA(negate(read(operand)));
        break;
      case 5:
        executeBasicQuarter5(instruction, quarter, indexedInstruction);
        break;
      case 6:
        executeAd(operand);
        break;
      case 7:
        setA(static_cast<uint16_t>(getA() & read(operand)));
        break;
    }
  }

  void executeTc(uint16_t operand) {
    tcEvent_ = true;
    if (operand != kRegQ) {
      writeErasable(kRegQ, getZ());
    }
    setZ(operand);
  }

  void executeBasicQuarter2(uint16_t instruction, uint8_t quarter) {
    switch (quarter & 03) {
      case 0:
        executeDas(previousAddress10(instruction));
        break;
      case 1:
        executeLxch(address10(instruction));
        break;
      case 2:
        executeIncr(address10(instruction));
        break;
      case 3:
        executeAds(address10(instruction));
        break;
    }
  }

  void executeBasicQuarter5(uint16_t instruction,
                            uint8_t quarter,
                            bool indexedInstruction) {
    switch (quarter & 03) {
      case 0:
        if (!indexedInstruction && instruction == kInstructionResume) {
          resumeFromInterrupt();
        } else {
          indexPending_ = true;
          indexValue_ = read(address10(instruction)) & kWordMask;
        }
        break;
      case 1:
        executeDxch(previousAddress10(instruction));
        break;
      case 2:
        executeTs(address10(instruction));
        break;
      case 3:
        executeXch(address10(instruction));
        break;
    }
  }

  void executeExtendedQuarter2(uint16_t instruction, uint8_t quarter) {
    switch (quarter & 03) {
      case 0:
        executeMsu(address10(instruction));
        break;
      case 1:
        executeQxch(address10(instruction));
        break;
      case 2:
        executeAug(address10(instruction));
        break;
      case 3:
        executeDim(address10(instruction));
        break;
    }
  }

  void executeIoInstruction(uint16_t instruction) {
    const uint8_t operation = peripheralCode(instruction);
    const uint16_t channel = channelAddress(instruction);
    const uint16_t channelValue = readChannel(channel);
    uint16_t result = 0;

    switch (operation) {
      case 0:
        setA(channelValue);
        break;
      case 1:
        cpuWriteChannel(channel, getA());
        break;
      case 2:
        setA(static_cast<uint16_t>(getA() & channelValue));
        break;
      case 3:
        result = static_cast<uint16_t>(getA() & channelValue);
        setA(result);
        cpuWriteChannel(channel, result);
        break;
      case 4:
        setA(static_cast<uint16_t>((getA() | channelValue) & kWordMask));
        break;
      case 5:
        result = static_cast<uint16_t>((getA() | channelValue) & kWordMask);
        setA(result);
        cpuWriteChannel(channel, result);
        break;
      case 6:
        setA(static_cast<uint16_t>((getA() ^ channelValue) & kWordMask));
        break;
      case 7:
        writeErasable(kRegZRUPT, getZ());
        forcedInstruction_ = read(0);
        forcedInstructionPending_ = true;
        interruptsInhibited_ = true;
        break;
    }
  }

  void cpuWriteChannel(uint16_t channel, uint16_t value) {
    channel &= kIoAddressMask;
    value &= kWordMask;
    if (channel == kRegL || channel == kRegQ) {
      writeErasable(channel, value);
      return;
    }
    if (channel == kChannelCounterEnable) {
      if ((value & kChannel13Trap31A) != 0) {
        trap31A_ = true;
      }
      if ((value & kChannel13Trap31B) != 0) {
        trap31B_ = true;
      }
      if ((value & kChannel13Trap32) != 0) {
        trap32_ = true;
      }
      value &= 043777;
    }
    if (channel == kChannelDSKY) {
      outputChannel10_[(value >> 11) & 017] = value;
    }
    if (channel == 0033) {
      channels_[channel] = static_cast<uint16_t>(
          (channels_[channel] | 076000) & kWordMask);
      return;
    }
    if (channel == kChannelRestartMonitor) {
      writeChannel(channel, 0);
      return;
    }
    if (channel == 0011 && (value & 01000) != 0) {
      restartLight_ = false;
    }
    if (channel == kChannelDownlinkWord0) {
      hardwareWriteFlags_ |= kHardwareWriteDownlinkWord0;
    } else if (channel == kChannelDownlinkWord1) {
      hardwareWriteFlags_ |= kHardwareWriteDownlinkWord1;
    }
    writeChannel(channel, value);
  }

  void executeAd(uint16_t operand) {
    const uint16_t left = getA();
    const uint16_t right = read(operand);
    const uint16_t result = addOnesComplement(left, right);
    const bool leftNegative = (left & kSignBit) != 0;
    const bool rightNegative = (right & kSignBit) != 0;
    const bool resultNegative = (result & kSignBit) != 0;
    int8_t overflow = 0;

    if (!leftNegative && !rightNegative && resultNegative) {
      overflow = 1;
    } else if (leftNegative && rightNegative && !resultNegative) {
      overflow = -1;
    }

    setAWithOverflow(result, overflow);
  }

  void executeTs(uint16_t address) {
    address &= kLow10Mask;
    const uint16_t accumulator = getA();
    const int8_t overflow = accumulatorOverflow_;

    if (address == kRegA) {
      if (overflow != 0) {
        setZ(incrementAddress(getZ(), 1));
      }
      return;
    }

    if (address == kRegZ) {
      setZ(accumulator);
      if (overflow != 0) {
        setA(overflowResidual(overflow));
      }
      return;
    }

    write(address, overflow == 0 ? accumulator
                                 : overflowCorrectedAccumulator(overflow));
    if (overflow != 0) {
      setA(overflowResidual(overflow));
      setZ(incrementAddress(getZ(), 1));
    }
  }

  void executeExtended(uint16_t instruction) {
    const uint8_t opcode = instructionCode(instruction);
    const uint8_t quarter = quarterCode(instruction);
    const uint16_t operand = address12(instruction);
    const uint16_t erasable = address10(instruction);

    if (opcode == 0) {
      executeIoInstruction(instruction);
      return;
    }

    switch (opcode) {
      case 0:
        break;
      case 1:
        if (!isLow10Operand(operand)) {
          executeBzf(operand);
        } else {
          executeDivide(erasable);
        }
        break;
      case 2:
        executeExtendedQuarter2(instruction, quarter);
        break;
      case 3:
        executeDca(previousAddress12(instruction));
        break;
      case 4:
        executeDcs(previousAddress12(instruction));
        break;
      case 5:
        indexPending_ = true;
        indexValue_ = read(operand) & kWordMask;
        extendPending_ = true;
        break;
      case 6:
        if (!isLow10Operand(operand)) {
          executeBzmf(operand);
        } else {
          executeSu(erasable);
        }
        break;
      case 7:
        executeMultiply(operand);
        break;
    }
  }

  void executeDivide(uint16_t operand) {
    operand &= kLow10Mask;

    uint16_t accMsw = overflowCorrected16(accumulator16());
    uint16_t accLsw = readErasable(kRegL);
    const uint32_t dividend = spToDecent(accMsw, accLsw);
    decentToSp(dividend, accMsw, accLsw);

    const uint16_t absA = absSp(accMsw);
    const uint16_t absL = absSp(accLsw);
    const uint32_t divisor16 = dvDivisor(operand, absA);
    const uint16_t absK = absSp(overflowCorrected16(divisor16));

    if (absA > absK || (absA == absK && absL != 0) ||
        valueOverflowed16(divisor16) != 0) {
      simulateHardwareDivide(divisor16);
      return;
    }

    if (absA == 0 && absL == 0) {
      uint16_t quotient = 0;
      if ((readErasable(kRegL) & kSignBit) ==
          (overflowCorrected16(divisor16) & kSignBit)) {
        quotient = absK == 0 ? 037777 : 0;
      } else {
        quotient = absK == 0 ? kSignBit : kWordMask;
      }
      setA(quotient);
      return;
    }

    if (absA == absK && absL == 0) {
      writeErasable(kRegL, accMsw);
      setA(accMsw == overflowCorrected16(divisor16) ? 037777 : kSignBit);
      return;
    }

    const int32_t cpuDividend = agcDpToCpu(dividend);
    const int32_t cpuDivisor = agcSpToCpu(overflowCorrected16(divisor16));
    const int32_t quotient = cpuDividend / cpuDivisor;
    const int32_t remainder = cpuDividend % cpuDivisor;
    setA(cpuToAgcSp(quotient));
    writeErasable(kRegL, remainder == 0
                              ? (cpuDividend >= 0 ? 0 : kWordMask)
                              : cpuToAgcSp(remainder));
  }

  void executeBzf(uint16_t operand) {
    const bool branch = getA() == 0 || getA() == kWordMask;
    lastInstructionMct_ = 1;
    if (branch) {
      setZ(operand);
    }
  }

  void executeBzmf(uint16_t operand) {
    const uint16_t accumulator = getA();
    const bool branch = accumulator == 0 || accumulator == kWordMask ||
                        (accumulator & kSignBit) != 0;
    lastInstructionMct_ = 1;
    if (branch) {
      setZ(operand);
    }
  }

  void executeSu(uint16_t operand) {
    setA(addOnesComplement(getA(), negate(read(operand))));
  }

  void executeMsu(uint16_t operand) {
    operand &= kLow10Mask;
    uint32_t left = getA();
    uint32_t rightComplement = 0;
    if (operand < 020) {
      rightComplement = (~readErasable(operand)) & 0177777;
    } else {
      left &= kWordMask;
      rightComplement = (~read(operand)) & kWordMask;
    }

    int32_t difference = static_cast<int32_t>(left + rightComplement + 1U);
    if ((difference & kSignBit) != 0) {
      difference |= 0100000;
      --difference;
    }
    setA(static_cast<uint16_t>(difference) & kWordMask);
  }

  void executeMultiply(uint16_t operand) {
    operand &= kAddressMask;
    const uint16_t multiplicand = overflowCorrected16(accumulator16());
    const uint16_t multiplier =
        operand < 020 ? overflowCorrected16(signExtendSp16(readErasable(operand)))
                      : read(operand);

    if (multiplier == 0 || multiplier == kWordMask) {
      setA(0);
      writeErasable(kRegL, 0);
      return;
    }

    if (multiplicand == 0 || multiplicand == kWordMask) {
      const bool negativeZero =
          (multiplicand == 0 && (multiplier & kSignBit) != 0) ||
          (multiplicand == kWordMask && (multiplier & kSignBit) == 0);
      const uint16_t zero = negativeZero ? kWordMask : 0;
      setA(zero);
      writeErasable(kRegL, zero);
      return;
    }

    uint32_t product =
        cpuToAgcDp(agcSpToCpu(multiplicand) * agcSpToCpu(multiplier));
    if ((product & 02000000000U) != 0) {
      product |= 04000000000U;
    }

    uint16_t high = 0;
    uint16_t low = 0;
    decentToSp(product, high, low);
    setA(high);
    writeErasable(kRegL, low);
  }

  void executeDas(uint16_t operand) {
    const uint16_t highAddress = operand & kLow10Mask;
    const uint16_t lowAddress = nextAddress10(highAddress);

    if (highAddress == kRegA) {
      writeErasable(kRegL, addOnesComplement(readErasable(kRegL),
                                             readErasable(kRegL)));
      setA(addOnesComplement(getA(), getA()));
      return;
    }

    const uint16_t lowSum =
        addOnesComplement(read(lowAddress), readErasable(kRegL));
    const uint16_t highSum = addOnesComplement(read(highAddress), getA());
    write(lowAddress, lowSum);
    write(highAddress, highSum);
    writeErasable(kRegL, 0);
    setA(0);
  }

  void executeLxch(uint16_t operand) {
    const uint16_t address = operand & kLow10Mask;
    const uint16_t previousL = readErasable(kRegL);
    writeErasable(kRegL, read(address));
    write(address, previousL);
  }

  void executeIncr(uint16_t operand) {
    const uint16_t address = operand & kLow10Mask;
    write(address, addOnesComplement(read(address), fromInt(1)));
  }

  void executeAds(uint16_t operand) {
    const uint16_t address = operand & kLow10Mask;
    const uint16_t result = addOnesComplement(read(address), getA());
    write(address, result);
    setA(result);
  }

  void executeDxch(uint16_t operand) {
    const uint16_t highAddress = operand & kLow10Mask;
    const uint16_t lowAddress = nextAddress10(highAddress);
    const uint16_t previousA = getA();
    const uint16_t previousL = readErasable(kRegL);
    writeErasable(kRegL, read(lowAddress));
    setA(read(highAddress));
    write(lowAddress, previousL);
    write(highAddress, previousA);
  }

  void executeQxch(uint16_t operand) {
    const uint16_t address = operand & kLow10Mask;
    const uint16_t previousQ = readErasable(kRegQ);
    writeErasable(kRegQ, read(address));
    write(address, previousQ);
  }

  void executeAug(uint16_t operand) {
    const uint16_t value = read(operand);
    const SignClass signClass = classify(value);
    if (signClass == SignClass::Negative ||
        signClass == SignClass::NegativeZero) {
      write(operand, addOnesComplement(value, negate(fromInt(1))));
    } else {
      write(operand, addOnesComplement(value, fromInt(1)));
    }
  }

  void executeDim(uint16_t operand) {
    const uint16_t value = read(operand);
    const SignClass signClass = classify(value);
    if (signClass == SignClass::Positive) {
      write(operand, addOnesComplement(value, negate(fromInt(1))));
    } else if (signClass == SignClass::Negative) {
      write(operand, addOnesComplement(value, fromInt(1)));
    }
  }

  void executeDca(uint16_t operand) {
    const uint16_t highAddress = operand & kAddressMask;
    const uint16_t lowAddress = nextAddress12(highAddress);
    const uint16_t lowValue = read(lowAddress);
    writeErasable(kRegL, lowValue);
    const uint16_t highValue = read(highAddress);
    setA(highValue);
    rewriteIfErasable(lowAddress, lowValue);
    rewriteIfErasable(highAddress, highValue);
  }

  void executeDcs(uint16_t operand) {
    const uint16_t highAddress = operand & kAddressMask;
    const uint16_t lowAddress = nextAddress12(highAddress);
    if (highAddress == kRegA) {
      writeErasable(kRegL, negate(readErasable(kRegL)));
      setA(negate(getA()));
      return;
    }

    const uint16_t lowValue = read(lowAddress);
    writeErasable(kRegL, negate(lowValue));
    const uint16_t highValue = read(highAddress);
    setA(negate(highValue));
    rewriteIfErasable(lowAddress, lowValue);
    rewriteIfErasable(highAddress, highValue);
  }

  void rewriteIfErasable(uint16_t address, uint16_t value) {
    address &= kAddressMask;
    if (isErasableAddress(address)) {
      writeErasable(resolveErasableIndex(address), value);
    }
  }

  static int16_t clampAgcInt(int32_t value) {
    if (value > static_cast<int32_t>(kPositiveMask)) {
      return static_cast<int16_t>(kPositiveMask);
    }
    if (value < -static_cast<int32_t>(kPositiveMask)) {
      return -static_cast<int16_t>(kPositiveMask);
    }
    return static_cast<int16_t>(value);
  }

  bool serviceInterrupt() {
    uint16_t interruptedPc = 0;
    uint16_t interruptedInstruction = 0;
    if (!serviceInterruptEntry(&interruptedPc, &interruptedInstruction)) {
      return false;
    }

    lastAddress_ = interruptedPc;
    lastInstruction_ = interruptedInstruction;
    lastOpcode_ = instructionCode(interruptedInstruction);
    lastExtended_ = false;
    lastInstructionMct_ = kRuptEntryMct;
    cycles_ += kRuptEntryMct;
    return true;
  }

  bool canEnterInterrupt() const {
    return interruptsEnabled_ && !interruptsInhibited_ &&
           pendingInterruptMask_ != 0 && !extendPending_;
  }

  bool serviceInterruptEntryAt(uint16_t interruptedPc,
                               uint16_t interruptedInstruction) {
    if (!canEnterInterrupt() ||
        isInterruptBoundaryInstruction(interruptedInstruction)) {
      return false;
    }

    for (uint8_t i = 0; i < kInterruptCount; ++i) {
      if ((pendingInterruptMask_ & (1U << i)) == 0) {
        continue;
      }

      return enterInterrupt(i, interruptedPc, interruptedInstruction, nullptr,
                            nullptr);
    }

    return false;
  }

  bool enterInterrupt(uint8_t interruptNumber,
                      uint16_t interruptedPc,
                      uint16_t interruptedInstruction,
                      uint16_t* interruptedPcOut,
                      uint16_t* interruptedInstructionOut) {
    pendingInterruptMask_ &=
        static_cast<uint16_t>(~static_cast<uint16_t>(1U << interruptNumber));
    activeInterrupt_ = interruptNumber;
    writeErasable(kRegZRUPT, interruptedPc);
    writeErasable(kRegBRUPT, interruptedInstruction);
    indexPending_ = false;
    indexValue_ = 0;
    extendPending_ = false;
    forcedInstructionPending_ = false;
    forcedInstruction_ = 0;
    interruptsInhibited_ = true;
    inInterruptService_ = true;
    interruptEntryEvent_ = true;
    setZ(interruptVectorAddress(interruptNumber));
    if (interruptedPcOut != nullptr) {
      *interruptedPcOut = interruptedPc;
    }
    if (interruptedInstructionOut != nullptr) {
      *interruptedInstructionOut = interruptedInstruction;
    }
    return true;
  }

  void resumeFromInterrupt() {
    setZ(readErasable(kRegZRUPT));
    forcedInstruction_ = readErasable(kRegBRUPT);
    forcedInstructionPending_ = true;
    interruptsInhibited_ = false;
    inInterruptService_ = false;
  }

  void executeXch(uint16_t operand) {
    const uint16_t previousA = getA();
    setA(read(operand));
    write(operand, previousA);
  }

  void executeCcs(uint16_t operand) {
    const uint16_t value = read(operand);

    switch (classify(value)) {
      case SignClass::Positive: {
        const uint16_t counted = decrementMagnitude(value);
        setA(counted);
        break;
      }
      case SignClass::PositiveZero:
        setA(0);
        setZ(incrementAddress(getZ(), 1));
        break;
      case SignClass::Negative: {
        const uint16_t magnitude = negate(value) & kPositiveMask;
        const uint16_t countedMagnitude = decrementMagnitude(magnitude);
        setA(countedMagnitude);
        setZ(incrementAddress(getZ(), 2));
        break;
      }
      case SignClass::NegativeZero:
        setA(0);
        setZ(incrementAddress(getZ(), 3));
        break;
    }
  }

  void setFault(Fault fault) {
    fault_ = fault;
    runState_ = RunState::Faulted;
  }

  uint32_t accumulator16() const {
    const uint16_t accumulator = getA();
    if (accumulatorOverflow_ > 0) {
      return accumulator & kWordMask;
    }
    if (accumulatorOverflow_ < 0) {
      return (0100000U | (accumulator & kPositiveMask)) & 0177777U;
    }
    return signExtendSp16(accumulator);
  }

  uint32_t dvDivisor(uint16_t operand, uint16_t absA) const {
    operand &= kLow10Mask;
    const bool dividendNegative =
        (absA == 0 && (readErasable(kRegL) & kSignBit) != 0) ||
        (absA != 0 && (getA() & kSignBit) != 0);

    if (operand == kRegA) {
      uint32_t divisor = accumulator16();
      if ((divisor & 0100000U) == 0) {
        divisor = (~divisor) & 0177777U;
      }
      return divisor;
    }

    if (operand == kRegL) {
      uint32_t divisor = signExtendSp16(readErasable(kRegL));
      if (dividendNegative) {
        divisor = (~divisor) & 0177777U;
      }
      return signExtendSp16(
          overflowCorrected16(addSp16(divisor, 040000U)));
    }

    if (operand == kRegZ) {
      uint32_t divisor = signExtendSp16(readErasable(kRegZ));
      if (dividendNegative) {
        divisor |= 0100000U;
      }
      return divisor & 0177777U;
    }

    if (operand < 020) {
      return signExtendSp16(readErasable(operand));
    }

    return signExtendSp16(read(operand));
  }

  void simulateHardwareDivide(uint32_t divisor) {
    divisor &= 0177777U;
    uint32_t a = accumulator16();
    uint32_t l = signExtendSp16(readErasable(kRegL));

    uint32_t dividendSign = a & 0100000U;
    if (dividendSign == 0) {
      a = (~a) & 0177777U;
    }
    if (a == 0177777U) {
      dividendSign = l & 0100000U;
    }
    if (dividendSign != 0) {
      l = (~l) & 0177777U;
    }

    l = addSp16(l, 040000U);
    if (valueOverflowed16(l) != 1) {
      a = addSp16(a, 1U);
    }

    uint32_t remainder = a;
    const uint32_t divisorSign = divisor & 0100000U;
    if (divisorSign != 0) {
      divisor = (~divisor) & 0177777U;
    }

    const uint32_t quotientSign = l & 0100000U;
    uint32_t quotient =
        quotientSign | ((l & 037777U) << 1) | (quotientSign >> 15);

    for (uint8_t i = 0; i < 14; ++i) {
      quotient <<= 1;
      const uint32_t remainderSign = remainder & 0100000U;
      remainder = remainderSign | ((remainder & 037777U) << 1);
      if ((quotient & 0100000U) == 0) {
        remainder |= (remainderSign >> 15);
      }

      const uint32_t sum = addSp16(remainder, divisor);
      if ((sum & 0100000U) != 0) {
        quotient |= 1U;
        remainder = sum;
      }
    }

    a = quotientSign | (quotient & kWordMask);
    setA(((dividendSign != divisorSign) ? ~a : a) & kWordMask);
    writeErasable(kRegL,
                  ((dividendSign != 0) ? remainder : ~remainder) & kWordMask);
  }

  void setAWithOverflow(uint16_t value, int8_t overflow) {
    writeErasable(kRegA, value);
    accumulatorOverflow_ = overflow;
  }

  static uint16_t overflowResidual(int8_t overflow) {
    return overflow > 0 ? fromInt(1) : negate(fromInt(1));
  }

  static uint16_t overflowCorrectedAccumulator(int8_t overflow) {
    return overflow > 0 ? 0 : kSignBit;
  }

  void noteErasableAccess(uint16_t address) const {
    if ((address & (kErasableWords - 1)) == 00067) {
      nightWatchmanServiced_ = true;
    }
  }

  mutable uint16_t erasable_[kErasableWords];
  uint16_t fixed_[kFixedWords];
  uint16_t channels_[kIoChannels];
  uint16_t outputChannel10_[16];
  RunState runState_ = RunState::Halted;
  Fault fault_ = Fault::None;
  uint32_t cycles_ = 0;
  bool indexPending_ = false;
  uint16_t indexValue_ = 0;
  bool extendPending_ = false;
  bool forcedInstructionPending_ = false;
  uint16_t forcedInstruction_ = 0;
  bool interruptsEnabled_ = true;
  bool interruptsInhibited_ = false;
  bool inInterruptService_ = false;
  int8_t accumulatorOverflow_ = 0;
  uint16_t pendingInterruptMask_ = 0;
  uint8_t activeInterrupt_ = 0;
  uint8_t hardwareWriteFlags_ = 0;
  bool tcEvent_ = false;
  bool interruptEntryEvent_ = false;
  mutable bool nightWatchmanServiced_ = false;
  bool trap31A_ = false;
  bool trap31B_ = false;
  bool trap32_ = false;
  bool restartLight_ = false;
  uint16_t lastInstruction_ = 0;
  uint16_t lastAddress_ = 0;
  uint8_t lastOpcode_ = 0;
  bool lastExtended_ = false;
  uint8_t lastInstructionMct_ = 0;
};

}  // namespace agc

#endif
