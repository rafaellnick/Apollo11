#include "../shared/agc_core.h"
#include "../shared/agc_machine_timing.h"
#include "../esp32_agc_core/rope_image.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

namespace {

constexpr uint16_t kScratch = 00120;
constexpr uint16_t kScratchSubtrahend = 00121;
constexpr uint16_t kConstOne = 04100;
constexpr uint16_t kConstTwo = 04101;
constexpr uint16_t kTraceSteps = 18;
constexpr uint32_t kRopeTraceSteps = 64;

enum class TraceMode {
  Synthetic,
  Rope,
};

struct Options {
  TraceMode mode = TraceMode::Synthetic;
  uint32_t steps = 0;
  uint32_t skipRows = 0;
  bool hardwareTiming = false;
  std::string ropeBinPath;
};

struct TraceSnapshot {
  uint16_t pc;
  uint16_t instruction;
  bool extended;
  uint16_t a;
  uint16_t l;
  uint16_t q;
  uint16_t z;
  uint16_t eb;
  uint16_t fb;
  uint16_t bb;
  uint32_t cycles;
  uint16_t mct;
  uint16_t irq;
  uint16_t ch010;
  uint16_t ch015;
};

uint16_t encodeQuarter(uint8_t opcode, uint8_t quarter, uint16_t address) {
  return static_cast<uint16_t>(((opcode & 07) << 12) |
                               ((quarter & 03) << 10) |
                               (address & agc::Core::kLow10Mask));
}

uint16_t encodeIo(uint8_t peripheralCode, uint16_t channel) {
  return static_cast<uint16_t>(((peripheralCode & 07) << 9) |
                               (channel & agc::Core::kIoAddressMask));
}

void printOctal(uint16_t value, uint8_t width) {
  std::cout << std::oct << std::uppercase << std::setfill('0')
            << std::setw(width) << static_cast<unsigned int>(value)
            << std::dec;
}

void printTraceHeader() {
  std::cout
      << "step,pc,instr,extended,a,l,q,z,eb,fb,bb,cyc,mct,irq,ch010,ch015\n";
}

TraceSnapshot makeTraceSnapshot(const agc::Core& core,
                                uint16_t pc,
                                uint16_t instruction,
                                bool extended,
                                uint16_t mct) {
  TraceSnapshot snapshot = {};
  snapshot.pc = pc & agc::Core::kAddressMask;
  snapshot.instruction = instruction & agc::Core::kWordMask;
  snapshot.extended = extended;
  snapshot.a = core.getA();
  snapshot.l = core.readErasable(agc::Core::kRegL);
  snapshot.q = core.readErasable(agc::Core::kRegQ);
  snapshot.z = core.getZ();
  snapshot.eb = core.readErasable(agc::Core::kRegEBANK);
  snapshot.fb = core.readErasable(agc::Core::kRegFBANK);
  snapshot.bb = core.readErasable(agc::Core::kRegBBANK);
  snapshot.cycles = core.cycles();
  snapshot.mct = mct;
  snapshot.irq = core.pendingInterruptMask();
  snapshot.ch010 = core.readOutputChannel10(0);
  snapshot.ch015 = core.readChannel(0015);
  return snapshot;
}

TraceSnapshot makeInstructionSnapshot(const agc::Core& core, uint16_t mct) {
  return makeTraceSnapshot(core, core.lastAddress(), core.lastInstruction(),
                           core.lastInstructionExtended(), mct);
}

void printTraceRow(const TraceSnapshot& snapshot, uint32_t step) {
  std::cout << std::dec << step << ",";
  printOctal(snapshot.pc, 4);
  std::cout << ",";
  printOctal(snapshot.instruction, 5);
  std::cout << "," << (snapshot.extended ? 1 : 0) << ",";
  printOctal(snapshot.a, 5);
  std::cout << ",";
  printOctal(snapshot.l, 5);
  std::cout << ",";
  printOctal(snapshot.q, 5);
  std::cout << ",";
  printOctal(snapshot.z, 4);
  std::cout << ",";
  printOctal(snapshot.eb, 5);
  std::cout << ",";
  printOctal(snapshot.fb, 5);
  std::cout << ",";
  printOctal(snapshot.bb, 5);
  std::cout << "," << snapshot.cycles << ","
            << static_cast<unsigned int>(snapshot.mct) << ",";
  printOctal(snapshot.irq, 3);
  std::cout << ",";
  printOctal(snapshot.ch010, 5);
  std::cout << ",";
  printOctal(snapshot.ch015, 5);
  std::cout << "\n";
}

void installTraceProgram(agc::Core& core) {
  core.writeFixed(kConstOne, agc::Core::fromInt(1));
  core.writeFixed(kConstTwo, agc::Core::fromInt(2));
  core.writeErasable(kScratchSubtrahend, agc::Core::fromInt(4));

  uint16_t address = agc::Core::kBootAddress;
  core.writeFixed(address++, agc::Core::encodeBasic(3, kConstOne));
  core.writeFixed(address++, agc::Core::encodeBasic(6, kConstTwo));
  core.writeFixed(address++, encodeQuarter(5, 2, kScratch));
  core.writeFixed(address++, agc::Core::kInstructionExtend);
  core.writeFixed(address++, encodeIo(1, agc::Core::kChannelOut0));
  core.writeFixed(address++, agc::Core::kInstructionExtend);
  core.writeFixed(address++, encodeIo(0, agc::Core::kChannelOut0));
  core.writeFixed(address++, encodeQuarter(2, 1, kScratch));
  core.writeFixed(address++, encodeQuarter(2, 2, kScratch));
  core.writeFixed(address++, encodeQuarter(2, 3, kScratch));
  core.writeFixed(address++, agc::Core::kInstructionExtend);
  core.writeFixed(address++, encodeQuarter(2, 1, kScratch));
  core.writeFixed(address++, agc::Core::kInstructionExtend);
  core.writeFixed(address++,
                  agc::Core::encodeBasic(6, kScratchSubtrahend));
  core.writeFixed(address++, agc::Core::kInstructionExtend);
  core.writeFixed(address++, agc::Core::encodeBasic(
                                 1, agc::Core::kBootAddress + 020));
  core.writeFixed(address++, agc::Core::encodeBasic(
                                 1, agc::Core::kBootAddress + 020));
  core.writeFixed(agc::Core::kBootAddress + 020,
                  agc::Core::encodeBasic(0, agc::Core::kBootAddress + 020));
}

bool loadYayulRopeBinary(agc::Core& core, const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << "Could not open rope binary: " << path << "\n";
    return false;
  }

  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  if (bytes.empty() || (bytes.size() % 2) != 0) {
    std::cerr << "Invalid yaYUL rope binary byte count: " << bytes.size()
              << "\n";
    return false;
  }

  const size_t wordCount = bytes.size() / 2;
  if ((wordCount % agc::Core::kFixedBankSize) != 0) {
    std::cerr << "yaYUL rope binary does not contain whole fixed banks\n";
    return false;
  }

  const size_t bankCount = wordCount / agc::Core::kFixedBankSize;
  if (bankCount == 0 || bankCount > agc::Core::kFixedBanks) {
    std::cerr << "Unsupported yaYUL rope bank count: " << bankCount << "\n";
    return false;
  }

  std::vector<uint16_t> rawWords(wordCount, 0);
  for (size_t i = 0; i < wordCount; ++i) {
    rawWords[i] = static_cast<uint16_t>(
        (((static_cast<uint16_t>(bytes[i * 2]) << 8) |
          static_cast<uint16_t>(bytes[i * 2 + 1])) >>
         1) &
        agc::Core::kWordMask);
  }

  std::vector<uint16_t> words(wordCount, 0);
  if (bankCount == agc::Core::kFixedBanks) {
    constexpr uint8_t kYayulBankOrder[agc::Core::kFixedBanks] = {
        2, 3, 0, 1, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
    for (size_t sourceBank = 0; sourceBank < bankCount; ++sourceBank) {
      const size_t actualBank = kYayulBankOrder[sourceBank];
      const size_t sourceOffset = sourceBank * agc::Core::kFixedBankSize;
      const size_t actualOffset = actualBank * agc::Core::kFixedBankSize;
      for (size_t i = 0; i < agc::Core::kFixedBankSize; ++i) {
        words[actualOffset + i] = rawWords[sourceOffset + i];
      }
    }
  } else {
    words = rawWords;
  }

  const agc::RopeImage image = {
      words.data(),
      static_cast<uint8_t>(bankCount),
      path.c_str(),
  };
  return core.loadRopeImage(image);
}

bool parseUnsigned(const char* text,
                   uint32_t* value,
                   bool allowZero = false) {
  if (text == nullptr || *text == '\0') {
    return false;
  }

  char* end = nullptr;
  const unsigned long parsed = std::strtoul(text, &end, 10);
  if (end == text || *end != '\0' || (!allowZero && parsed == 0) ||
      parsed > 10000000UL) {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

void printUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " [--mode synthetic|rope] [--steps N] [--skip-rows N]"
            << " [--hardware-timing]"
            << " [--rope-bin MAIN.agc.bin]\n";
}

bool parseOptions(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
      const char* mode = argv[++i];
      if (std::strcmp(mode, "synthetic") == 0 ||
          std::strcmp(mode, "Synthetic") == 0) {
        options->mode = TraceMode::Synthetic;
      } else if (std::strcmp(mode, "rope") == 0 ||
                 std::strcmp(mode, "Rope") == 0) {
        options->mode = TraceMode::Rope;
      } else {
        return false;
      }
    } else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
      if (!parseUnsigned(argv[++i], &options->steps)) {
        return false;
      }
    } else if (std::strcmp(argv[i], "--skip-rows") == 0 && i + 1 < argc) {
      if (!parseUnsigned(argv[++i], &options->skipRows, true)) {
        return false;
      }
    } else if (std::strcmp(argv[i], "--hardware-timing") == 0) {
      options->hardwareTiming = true;
    } else if (std::strcmp(argv[i], "--rope-bin") == 0 && i + 1 < argc) {
      options->ropeBinPath = argv[++i];
    } else if (std::strcmp(argv[i], "--help") == 0) {
      return false;
    } else {
      return false;
    }
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parseOptions(argc, argv, &options)) {
    printUsage(argv[0]);
    return 2;
  }

  agc::Core core;

  if (options.mode == TraceMode::Synthetic) {
    installTraceProgram(core);
  } else if (!options.ropeBinPath.empty()) {
    if (!loadYayulRopeBinary(core, options.ropeBinPath)) {
      std::cerr << "Failed to load yaYUL rope binary\n";
      return 2;
    }
  } else if (!core.loadRopeImage(embedded_rope::kImage)) {
    std::cerr << "Failed to load embedded rope image\n";
    return 2;
  }

  core.start();
  agc::MachineTiming machineTiming;
  const bool useHardwareTiming =
      options.hardwareTiming && options.mode == TraceMode::Rope;

  const uint32_t steps =
      options.steps != 0
          ? options.steps
          : (options.mode == TraceMode::Rope ? kRopeTraceSteps : kTraceSteps);
  const uint32_t totalRows = options.skipRows + steps;

  printTraceHeader();
  for (uint32_t step = 1; step <= totalRows; ++step) {
    const bool emitRow = step > options.skipRows;
    if (useHardwareTiming) {
      machineTiming.serviceScheduledEvents(core);
      const uint16_t pc = core.previewAddress();
      const uint16_t instruction = core.previewInstruction();
      const bool extended = core.previewInstructionExtended();
      const uint16_t stallMct = machineTiming.consumeStallCycles(core);
      if (stallMct > 0) {
        if (emitRow) {
          printTraceRow(makeTraceSnapshot(core, pc, instruction, extended,
                                          stallMct),
                        step);
        }
        continue;
      }

      uint16_t interruptedPc = 0;
      uint16_t interruptedInstruction = 0;
      if (core.serviceInterruptEntry(&interruptedPc,
                                     &interruptedInstruction)) {
        const uint32_t cyclesBefore = core.cycles();
        core.advanceCycles(agc::Core::kRuptEntryMct);
        machineTiming.observeCpuCycles(core, agc::Core::kRuptEntryMct);
        const uint16_t rowMct =
            static_cast<uint16_t>(core.cycles() - cyclesBefore);
        machineTiming.observeCoreEvents(core, false);
        if (emitRow) {
          printTraceRow(makeTraceSnapshot(core, interruptedPc,
                                          interruptedInstruction, extended,
                                          rowMct),
                        step);
        }
        continue;
      }
    }

    const uint32_t cyclesBefore = core.cycles();
    const bool ok = core.step();
    uint32_t rowMct = core.lastInstructionMct();
    if (useHardwareTiming) {
      machineTiming.observeCpuCycles(core, core.cycles() - cyclesBefore);
      machineTiming.observeCoreEvents(core);
      rowMct = core.cycles() - cyclesBefore;
    }
    if (emitRow) {
      printTraceRow(makeInstructionSnapshot(core,
                                            static_cast<uint16_t>(rowMct)),
                    step);
    }
    if (!ok) {
      break;
    }
  }

  return core.fault() == agc::Core::Fault::None ? 0 : 1;
}
