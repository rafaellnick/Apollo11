#ifndef APOLLO11_EMBEDDED_DSKY_PROTOCOL_H
#define APOLLO11_EMBEDDED_DSKY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dsky {

constexpr size_t kRegisterFieldSize = 8;
constexpr size_t kLineBufferSize = 128;

enum Lamp : uint32_t {
  kLampCompActy = 1UL << 0,
  kLampUplinkActy = 1UL << 1,
  kLampTemp = 1UL << 2,
  kLampGimbalLock = 1UL << 3,
  kLampProg = 1UL << 4,
  kLampKeyRel = 1UL << 5,
  kLampOprErr = 1UL << 6,
  kLampStby = 1UL << 7,
  kLampNoAtt = 1UL << 8,
  kLampTracker = 1UL << 9,
};

enum class Key : uint8_t {
  None = 0,
  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,
  Verb,
  Noun,
  Plus,
  Minus,
  Clear,
  Proceed,
  KeyRelease,
  Enter,
  Reset,
};

struct State {
  uint8_t program;
  uint8_t verb;
  uint8_t noun;
  char r1[kRegisterFieldSize];
  char r2[kRegisterFieldSize];
  char r3[kRegisterFieldSize];
  uint16_t alarm;
  bool flashVerbNoun;
  uint32_t lampMask;
  uint32_t missionSeconds;
};

inline void copyField(char* destination, size_t destinationSize,
                      const char* source) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

inline void initState(State* state) {
  if (state == nullptr) {
    return;
  }

  state->program = 0;
  state->verb = 16;
  state->noun = 36;
  copyField(state->r1, sizeof(state->r1), "+00000");
  copyField(state->r2, sizeof(state->r2), "+00000");
  copyField(state->r3, sizeof(state->r3), "+00000");
  state->alarm = 0;
  state->flashVerbNoun = false;
  state->lampMask = 0;
  state->missionSeconds = 0;
}

inline void setLamp(State* state, Lamp lamp, bool enabled) {
  if (state == nullptr) {
    return;
  }

  if (enabled) {
    state->lampMask |= static_cast<uint32_t>(lamp);
  } else {
    state->lampMask &= ~static_cast<uint32_t>(lamp);
  }
}

inline bool lampEnabled(const State& state, Lamp lamp) {
  return (state.lampMask & static_cast<uint32_t>(lamp)) != 0;
}

inline const char* keyName(Key key) {
  switch (key) {
    case Key::Digit0:
      return "0";
    case Key::Digit1:
      return "1";
    case Key::Digit2:
      return "2";
    case Key::Digit3:
      return "3";
    case Key::Digit4:
      return "4";
    case Key::Digit5:
      return "5";
    case Key::Digit6:
      return "6";
    case Key::Digit7:
      return "7";
    case Key::Digit8:
      return "8";
    case Key::Digit9:
      return "9";
    case Key::Verb:
      return "VERB";
    case Key::Noun:
      return "NOUN";
    case Key::Plus:
      return "PLUS";
    case Key::Minus:
      return "MINUS";
    case Key::Clear:
      return "CLR";
    case Key::Proceed:
      return "PRO";
    case Key::KeyRelease:
      return "KEYREL";
    case Key::Enter:
      return "ENTR";
    case Key::Reset:
      return "RSET";
    default:
      return "NONE";
  }
}

inline Key parseKeyName(const char* name) {
  if (name == nullptr) {
    return Key::None;
  }

  if (strcmp(name, "0") == 0) {
    return Key::Digit0;
  }
  if (strcmp(name, "1") == 0) {
    return Key::Digit1;
  }
  if (strcmp(name, "2") == 0) {
    return Key::Digit2;
  }
  if (strcmp(name, "3") == 0) {
    return Key::Digit3;
  }
  if (strcmp(name, "4") == 0) {
    return Key::Digit4;
  }
  if (strcmp(name, "5") == 0) {
    return Key::Digit5;
  }
  if (strcmp(name, "6") == 0) {
    return Key::Digit6;
  }
  if (strcmp(name, "7") == 0) {
    return Key::Digit7;
  }
  if (strcmp(name, "8") == 0) {
    return Key::Digit8;
  }
  if (strcmp(name, "9") == 0) {
    return Key::Digit9;
  }
  if (strcmp(name, "VERB") == 0) {
    return Key::Verb;
  }
  if (strcmp(name, "NOUN") == 0) {
    return Key::Noun;
  }
  if (strcmp(name, "PLUS") == 0) {
    return Key::Plus;
  }
  if (strcmp(name, "MINUS") == 0) {
    return Key::Minus;
  }
  if (strcmp(name, "CLR") == 0) {
    return Key::Clear;
  }
  if (strcmp(name, "PRO") == 0) {
    return Key::Proceed;
  }
  if (strcmp(name, "KEYREL") == 0) {
    return Key::KeyRelease;
  }
  if (strcmp(name, "ENTR") == 0) {
    return Key::Enter;
  }
  if (strcmp(name, "RSET") == 0) {
    return Key::Reset;
  }

  return Key::None;
}

inline bool formatKeyLine(Key key, char* buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0 || key == Key::None) {
    return false;
  }

  const int written =
      snprintf(buffer, bufferSize, "KEY,%s", keyName(key));
  return written > 0 && static_cast<size_t>(written) < bufferSize;
}

inline bool parseKeyLine(char* line, Key* key) {
  if (line == nullptr || key == nullptr) {
    return false;
  }

  char* savePtr = nullptr;
  char* token = strtok_r(line, ",", &savePtr);
  if (token == nullptr || strcmp(token, "KEY") != 0) {
    return false;
  }

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }

  *key = parseKeyName(token);
  return *key != Key::None;
}

inline bool formatStateLine(const State& state, char* buffer,
                            size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return false;
  }

  const int written = snprintf(
      buffer, bufferSize, "STATE,%u,%u,%u,%s,%s,%s,%u,%u,%lu,%lu",
      static_cast<unsigned int>(state.program),
      static_cast<unsigned int>(state.verb),
      static_cast<unsigned int>(state.noun), state.r1, state.r2, state.r3,
      static_cast<unsigned int>(state.alarm), state.flashVerbNoun ? 1U : 0U,
      static_cast<unsigned long>(state.lampMask),
      static_cast<unsigned long>(state.missionSeconds));

  return written > 0 && static_cast<size_t>(written) < bufferSize;
}

inline bool parseStateLine(char* line, State* state) {
  if (line == nullptr || state == nullptr) {
    return false;
  }

  char* savePtr = nullptr;
  char* token = strtok_r(line, ",", &savePtr);
  if (token == nullptr || strcmp(token, "STATE") != 0) {
    return false;
  }

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->program = static_cast<uint8_t>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->verb = static_cast<uint8_t>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->noun = static_cast<uint8_t>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  copyField(state->r1, sizeof(state->r1), token);

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  copyField(state->r2, sizeof(state->r2), token);

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  copyField(state->r3, sizeof(state->r3), token);

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->alarm = static_cast<uint16_t>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->flashVerbNoun = strtoul(token, nullptr, 10) != 0;

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->lampMask = static_cast<uint32_t>(strtoul(token, nullptr, 10));

  token = strtok_r(nullptr, ",", &savePtr);
  if (token == nullptr) {
    return false;
  }
  state->missionSeconds = static_cast<uint32_t>(strtoul(token, nullptr, 10));

  return true;
}

}  // namespace dsky

#endif
