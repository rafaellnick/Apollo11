#include "../shared/agc_core.h"
#include "../esp32_agc_core/rope_image.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdint.h>

namespace {

constexpr uint16_t kScratch = 00120;
constexpr uint16_t kScratchSubtrahend = 00121;
constexpr uint16_t kConstOne = 04100;
constexpr uint16_t kConstTwo = 04101;
constexpr uint16_t kTraceSteps = 18;
constexpr uint16_t kRopeTraceSteps = 64;

enum class TraceMode {
  Synthetic,
  Rope,
};

struct Options {
  TraceMode mode = TraceMode::Synthetic;
  uint16_t steps = 0;
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

void printTraceRow(const agc::Core& core, uint16_t step) {
  std::cout << std::dec << step << ",";
  printOctal(core.lastAddress(), 4);
  std::cout << ",";
  printOctal(core.lastInstruction(), 5);
  std::cout << "," << (core.lastInstructionExtended() ? 1 : 0) << ",";
  printOctal(core.getA(), 5);
  std::cout << ",";
  printOctal(core.readErasable(agc::Core::kRegL), 5);
  std::cout << ",";
  printOctal(core.readErasable(agc::Core::kRegQ), 5);
  std::cout << ",";
  printOctal(core.getZ(), 4);
  std::cout << ",";
  printOctal(core.readErasable(agc::Core::kRegEBANK), 5);
  std::cout << ",";
  printOctal(core.readErasable(agc::Core::kRegFBANK), 5);
  std::cout << ",";
  printOctal(core.readErasable(agc::Core::kRegBBANK), 5);
  std::cout << "," << core.cycles() << ","
            << static_cast<unsigned int>(core.lastInstructionMct()) << ",";
  printOctal(core.pendingInterruptMask(), 3);
  std::cout << ",";
  printOctal(core.readChannel(0010), 5);
  std::cout << ",";
  printOctal(core.readChannel(0015), 5);
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

bool parseUnsigned(const char* text, uint16_t* value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }

  char* end = nullptr;
  const unsigned long parsed = std::strtoul(text, &end, 10);
  if (end == text || *end != '\0' || parsed == 0 || parsed > 65535UL) {
    return false;
  }

  *value = static_cast<uint16_t>(parsed);
  return true;
}

void printUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " [--mode synthetic|rope] [--steps N]\n";
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
  } else if (!core.loadRopeImage(embedded_rope::kImage)) {
    std::cerr << "Failed to load embedded rope image\n";
    return 2;
  }

  core.start();

  const uint16_t steps =
      options.steps != 0
          ? options.steps
          : (options.mode == TraceMode::Rope ? kRopeTraceSteps : kTraceSteps);

  printTraceHeader();
  for (uint16_t step = 1; step <= steps; ++step) {
    const bool ok = core.step();
    printTraceRow(core, step);
    if (!ok) {
      break;
    }
  }

  return core.fault() == agc::Core::Fault::None ? 0 : 1;
}
