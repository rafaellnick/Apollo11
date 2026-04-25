#ifndef APOLLO11_EMBEDDED_AGC_CORE_H
#define APOLLO11_EMBEDDED_AGC_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace agc {

class Core {
 public:
  static constexpr uint16_t kWordMask = 077777;
  static constexpr uint16_t kSignBit = 040000;
  static constexpr uint16_t kPositiveMask = 037777;
  static constexpr uint16_t kAddressMask = 07777;
  static constexpr uint16_t kErasableWords = 04000;
  static constexpr uint16_t kFixedBankSize = 02000;
  static constexpr uint8_t kFixedBanks = 36;
  static constexpr size_t kFixedWords =
      static_cast<size_t>(kFixedBanks) * kFixedBankSize;

  static constexpr uint16_t kRegA = 00000;
  static constexpr uint16_t kRegL = 00001;
  static constexpr uint16_t kRegQ = 00002;
  static constexpr uint16_t kRegEBANK = 00003;
  static constexpr uint16_t kRegFBANK = 00004;
  static constexpr uint16_t kRegZ = 00005;
  static constexpr uint16_t kRegBBANK = 00006;

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
  };

  Core() { reset(); }

  void reset() {
    memset(erasable_, 0, sizeof(erasable_));
    memset(fixed_, 0, sizeof(fixed_));
    runState_ = RunState::Halted;
    fault_ = Fault::None;
    cycles_ = 0;
    indexPending_ = false;
    indexValue_ = 0;
    lastInstruction_ = 0;
    lastAddress_ = 0;
    lastOpcode_ = 0;
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
    writeFixed(kBootAddress + 0002, encodeBasic(5, kPanelCounter));
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

    const uint16_t pc = getZ();
    uint16_t instruction = read(pc);
    setZ(incrementAddress(pc, 1));

    if (indexPending_) {
      instruction = applyIndex(instruction, indexValue_);
      indexPending_ = false;
      indexValue_ = 0;
    }

    lastInstruction_ = instruction;
    lastAddress_ = pc;
    lastOpcode_ = static_cast<uint8_t>((instruction >> 12) & 07);

    const uint16_t operand = instruction & kAddressMask;
    executeBasic(lastOpcode_, operand);
    cycles_++;
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

    if (address < kErasableWords) {
      return erasable_[address] & kWordMask;
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

    if (address < kErasableWords) {
      erasable_[address] = value;
      return true;
    }

    setFault(Fault::FixedWrite);
    return false;
  }

  uint16_t readErasable(uint16_t address) const {
    return erasable_[address & (kErasableWords - 1)] & kWordMask;
  }

  void writeErasable(uint16_t address, uint16_t value) {
    erasable_[address & (kErasableWords - 1)] = value & kWordMask;
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

  uint16_t getA() const { return readErasable(kRegA); }
  void setA(uint16_t value) { writeErasable(kRegA, value); }

  uint16_t getZ() const { return readErasable(kRegZ) & kAddressMask; }
  void setZ(uint16_t address) { writeErasable(kRegZ, address & kAddressMask); }

  uint32_t cycles() const { return cycles_; }
  RunState runState() const { return runState_; }
  Fault fault() const { return fault_; }
  uint16_t lastInstruction() const { return lastInstruction_; }
  uint16_t lastAddress() const { return lastAddress_; }
  uint8_t lastOpcode() const { return lastOpcode_; }

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
    const uint16_t opcode = instruction & 070000;
    const uint16_t operand =
        static_cast<uint16_t>((instruction + indexValue) & kAddressMask);
    return static_cast<uint16_t>(opcode | operand);
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

  static bool fixedIndex(uint16_t address, size_t* index) {
    address &= kAddressMask;

    if (address < 04000) {
      return false;
    }

    const uint8_t bank = address < 06000 ? 2 : 3;
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

  void executeBasic(uint8_t opcode, uint16_t operand) {
    switch (opcode & 07) {
      case 0:
        executeTc(operand);
        break;
      case 1:
        executeCcs(operand);
        break;
      case 2:
        indexPending_ = true;
        indexValue_ = read(operand) & kAddressMask;
        break;
      case 3:
        executeXch(operand);
        break;
      case 4:
        setA(negate(read(operand)));
        break;
      case 5:
        write(operand, getA());
        break;
      case 6:
        setA(addOnesComplement(getA(), read(operand)));
        break;
      case 7:
        setA(static_cast<uint16_t>(getA() & read(operand)));
        break;
    }
  }

  void executeTc(uint16_t operand) {
    writeErasable(kRegQ, getZ());
    setZ(operand);
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
        write(operand, counted);
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
        write(operand, negate(countedMagnitude));
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

  uint16_t erasable_[kErasableWords];
  uint16_t fixed_[kFixedWords];
  RunState runState_ = RunState::Halted;
  Fault fault_ = Fault::None;
  uint32_t cycles_ = 0;
  bool indexPending_ = false;
  uint16_t indexValue_ = 0;
  uint16_t lastInstruction_ = 0;
  uint16_t lastAddress_ = 0;
  uint8_t lastOpcode_ = 0;
};

}  // namespace agc

#endif
