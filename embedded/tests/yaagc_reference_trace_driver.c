#include "agc_engine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int DebuggerInterruptMasks[11];

void BacktraceAdd(agc_t *state, int cause) {
  (void)state;
  (void)cause;
}

int BacktraceRestore(agc_t *state, int n) {
  (void)state;
  (void)n;
  return 1;
}

void BacktraceDisplay(agc_t *state, int num) {
  (void)state;
  (void)num;
}

enum {
  kWordMask = 077777,
  kAddressMask = 07777,
  kLow10Mask = 01777,
  kIoAddressMask = 00777,
  kBootAddress = 04000,
  kScratch = 00120,
  kScratchSubtrahend = 00121,
  kConstOne = 04100,
  kConstTwo = 04101,
  kDefaultSteps = 18
};

typedef struct {
  unsigned steps;
  const char *rom_image;
  int cpu_only;
} TraceOptions;

static uint16_t agc_negate(uint16_t word) {
  return (uint16_t)((~word) & kWordMask);
}

static uint16_t agc_from_int(int value) {
  if (value < 0) {
    return agc_negate((uint16_t)(-value));
  }

  return (uint16_t)(value & 037777);
}

static uint16_t encode_basic(uint8_t opcode, uint16_t operand) {
  return (uint16_t)(((opcode & 07) << 12) | (operand & kAddressMask));
}

static uint16_t encode_quarter(uint8_t opcode, uint8_t quarter,
                               uint16_t address) {
  return (uint16_t)(((opcode & 07) << 12) | ((quarter & 03) << 10) |
                    (address & kLow10Mask));
}

static uint16_t encode_io(uint8_t peripheral_code, uint16_t channel) {
  return (uint16_t)(((peripheral_code & 07) << 9) |
                    (channel & kIoAddressMask));
}

static void write_fixed(agc_t *state, uint16_t address, uint16_t value) {
  const uint16_t sreg = (uint16_t)(address & kAddressMask);
  int bank = 0;

  if (sreg >= 04000 && sreg < 06000) {
    bank = 2;
  } else if (sreg >= 06000) {
    bank = 3;
  } else {
    bank = (state->Erasable[0][RegFB] >> 10) & 037;
  }

  state->Fixed[bank][sreg & 01777] = (int16_t)(value & kWordMask);
}

static uint16_t read_word(const agc_t *state, uint16_t address) {
  const uint16_t sreg = (uint16_t)(address & kAddressMask);

  if (sreg < 00400) {
    return (uint16_t)state->Erasable[0][sreg & 00377] & kWordMask;
  }

  if (sreg < 01000) {
    return (uint16_t)state->Erasable[1][sreg & 00377] & kWordMask;
  }

  if (sreg < 01400) {
    return (uint16_t)state->Erasable[2][sreg & 00377] & kWordMask;
  }

  if (sreg < 02000) {
    const int bank = (state->Erasable[0][RegEB] >> 8) & 07;
    return (uint16_t)state->Erasable[bank][sreg & 00377] & kWordMask;
  }

  if (sreg < 04000) {
    int bank = (state->Erasable[0][RegFB] >> 10) & 037;
    if ((bank & 030) == 030 && (state->OutputChannel7 & 0100) != 0) {
      bank += 010;
    }
    return (uint16_t)state->Fixed[bank][sreg & 01777] & kWordMask;
  }

  if (sreg < 06000) {
    return (uint16_t)state->Fixed[2][sreg & 01777] & kWordMask;
  }

  return (uint16_t)state->Fixed[3][sreg & 01777] & kWordMask;
}

static uint16_t executed_instruction(const agc_t *state, uint16_t pc) {
  if (state->SubstituteInstruction) {
    return (uint16_t)state->Erasable[0][RegBRUPT] & kWordMask;
  }

  return (uint16_t)OverflowCorrected(
             AddSP16(SignExtend((int16_t)state->IndexValue),
                     SignExtend((int16_t)read_word(state, pc)))) &
         kWordMask;
}

static uint16_t pending_interrupt_mask(const agc_t *state) {
  uint16_t mask = 0;
  for (uint8_t i = 1; i <= NUM_INTERRUPT_TYPES; ++i) {
    if (state->InterruptRequests[i]) {
      mask |= (uint16_t)(1U << (i - 1U));
    }
  }

  return mask;
}

static uint16_t trace_channel(const agc_t *state, uint16_t channel) {
  channel &= kIoAddressMask;

  if (channel == 010) {
    return (uint16_t)state->OutputChannel10[0] & kWordMask;
  }

  if (channel == 07) {
    return (uint16_t)state->OutputChannel7 & kWordMask;
  }

  return (uint16_t)state->InputChannel[channel] & kWordMask;
}

static void print_octal(uint16_t value, int width) {
  printf("%0*o", width, value & kWordMask);
}

static void print_header(void) {
  puts("step,pc,instr,extended,a,l,q,z,eb,fb,bb,cyc,mct,irq,ch010,ch015");
}

static void print_row(const agc_t *state, unsigned step, uint16_t pc,
                      uint16_t instr, int extended, uint64_t mct) {
  printf("%u,", step);
  print_octal(pc, 4);
  printf(",");
  print_octal(instr, 5);
  printf(",%d,", extended ? 1 : 0);
  print_octal((uint16_t)state->Erasable[0][RegA], 5);
  printf(",");
  print_octal((uint16_t)state->Erasable[0][RegL], 5);
  printf(",");
  print_octal((uint16_t)state->Erasable[0][RegQ], 5);
  printf(",");
  print_octal((uint16_t)state->Erasable[0][RegZ], 4);
  printf(",");
  print_octal((uint16_t)state->Erasable[0][RegEB], 5);
  printf(",");
  print_octal((uint16_t)state->Erasable[0][RegFB], 5);
  printf(",");
  print_octal((uint16_t)state->Erasable[0][RegBB], 5);
  printf(",%llu,%llu,", (unsigned long long)state->CycleCounter,
         (unsigned long long)mct);
  print_octal(pending_interrupt_mask(state), 3);
  printf(",");
  print_octal(trace_channel(state, 010), 5);
  printf(",");
  print_octal(trace_channel(state, 015), 5);
  printf("\n");
}

static void install_trace_program(agc_t *state) {
  write_fixed(state, kConstOne, agc_from_int(1));
  write_fixed(state, kConstTwo, agc_from_int(2));
  state->Erasable[0][kScratchSubtrahend] = (int16_t)agc_from_int(4);

  uint16_t address = kBootAddress;
  write_fixed(state, address++, encode_basic(3, kConstOne));
  write_fixed(state, address++, encode_basic(6, kConstTwo));
  write_fixed(state, address++, encode_quarter(5, 2, kScratch));
  write_fixed(state, address++, 00006);
  write_fixed(state, address++, encode_io(1, 010));
  write_fixed(state, address++, 00006);
  write_fixed(state, address++, encode_io(0, 010));
  write_fixed(state, address++, encode_quarter(2, 1, kScratch));
  write_fixed(state, address++, encode_quarter(2, 2, kScratch));
  write_fixed(state, address++, encode_quarter(2, 3, kScratch));
  write_fixed(state, address++, 00006);
  write_fixed(state, address++, encode_quarter(2, 1, kScratch));
  write_fixed(state, address++, 00006);
  write_fixed(state, address++, encode_basic(6, kScratchSubtrahend));
  write_fixed(state, address++, 00006);
  write_fixed(state, address++, encode_basic(1, kBootAddress + 020));
  write_fixed(state, address++, encode_basic(1, kBootAddress + 020));
  write_fixed(state, kBootAddress + 020, encode_basic(0, kBootAddress + 020));
}

static int run_one_instruction(agc_t *state, uint64_t *mct) {
  const uint64_t before = state->CycleCounter;
  int guard = 32;

  do {
    if (agc_engine(state) != 0) {
      return 1;
    }
    guard--;
  } while ((state->PendFlag || state->ExtraDelay) && guard > 0);

  *mct = state->CycleCounter - before;
  return guard > 0 ? 0 : 2;
}

static int parse_options(int argc, char **argv, TraceOptions *options) {
  options->steps = kDefaultSteps;
  options->rom_image = NULL;
  options->cpu_only = 0;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
      options->steps = (unsigned)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
      options->rom_image = argv[++i];
    } else if (strcmp(argv[i], "--cpu-only") == 0) {
      options->cpu_only = 1;
    } else if (strcmp(argv[i], "--help") == 0) {
      return 1;
    } else {
      return 1;
    }
  }

  return options->steps > 0 ? 0 : 1;
}

static void print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--steps N] [--rom MAIN.agc.bin] [--cpu-only]\n",
          argv0);
}

int main(int argc, char **argv) {
  TraceOptions options;
  if (parse_options(argc, argv, &options) != 0) {
    print_usage(argv[0]);
    return 2;
  }

  agc_t state;
  memset(&state, 0, sizeof(state));

  const int init_result = agc_engine_init(&state, options.rom_image, NULL, 0);
  if (init_result != 0) {
    fprintf(stderr, "agc_engine_init failed: %d\n", init_result);
    return init_result;
  }

  for (uint8_t i = 0; i <= NUM_INTERRUPT_TYPES; ++i) {
    state.InterruptRequests[i] = 0;
    DebuggerInterruptMasks[i] = 1;
  }
  state.DownruptTimeValid = 0;
  state.ExtraDelay = 0;
  state.PendFlag = 0;
  state.PendDelay = 0;
  state.Erasable[0][RegZ] = kBootAddress;
  if (options.cpu_only) {
    state.ScalerCounter = -1000000000;
    state.ChannelRoutineCount = 1;
    state.DownruptTimeValid = 0;
  }

  if (options.rom_image == NULL) {
    install_trace_program(&state);
  }

  print_header();
  for (unsigned step = 1; step <= options.steps; ++step) {
    const uint16_t pc = (uint16_t)state.Erasable[0][RegZ] & kAddressMask;
    const uint16_t instr = executed_instruction(&state, pc);
    const int extended = state.ExtraCode != 0;
    uint64_t mct = 0;

    const int run_result = run_one_instruction(&state, &mct);
    if (run_result != 0) {
      fprintf(stderr, "agc_engine failed at step %u: %d\n", step, run_result);
      return run_result;
    }

    print_row(&state, step, pc, instr, extended, mct);
  }

  return 0;
}
