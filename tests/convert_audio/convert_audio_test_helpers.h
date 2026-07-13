#pragma once

#include <avisynth.h>

#include "convert/convert_audio.h"

#include "support/comparators.h"
#include "support/cpu_features.h"
#include "support/guarded_audio_buffer.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace avsut::test {

using AudioConvertFunction = void (*)(void*, void*, int);

enum class AudioFormat { U8, S16, S24, S32, F32 };

inline const char* audio_format_name(AudioFormat format) {
  switch (format) {
    case AudioFormat::U8:
      return "U8";
    case AudioFormat::S16:
      return "S16";
    case AudioFormat::S24:
      return "S24";
    case AudioFormat::S32:
      return "S32";
    case AudioFormat::F32:
      return "F32";
  }
  return "Unknown";
}

inline std::size_t audio_format_bytes(AudioFormat format) {
  switch (format) {
    case AudioFormat::U8:
      return 1;
    case AudioFormat::S16:
      return 2;
    case AudioFormat::S24:
      return 3;
    case AudioFormat::S32:
    case AudioFormat::F32:
      return 4;
  }
  throw std::invalid_argument("unknown audio format");
}

inline std::size_t audio_alignment_offset(AudioFormat format) {
  switch (format) {
    case AudioFormat::U8:
      return 1;
    case AudioFormat::S16:
      return 2;
    case AudioFormat::S24:
      return 3;
    case AudioFormat::S32:
    case AudioFormat::F32:
      return 4;
  }
  return 0;
}

inline std::string audio_variant_name(const Variant<AudioConvertFunction>& variant) {
  std::string result = "Variant";
  bool capitalize = true;
  for (const char character : variant.name) {
    if (character == '_' || character == '-' || character == '.') {
      capitalize = true;
      continue;
    }
    result.push_back(capitalize && character >= 'a' && character <= 'z'
                         ? static_cast<char>(character - ('a' - 'A'))
                         : character);
    capitalize = false;
  }
  return result;
}

struct AudioIntegerCase {
  AudioFormat source_format{};
  AudioFormat destination_format{};
  std::size_t count{};
  std::size_t source_alignment_offset{};
  std::size_t destination_alignment_offset{};
  Variant<AudioConvertFunction> variant;
  std::string expected_hash;
  std::string name;
};

inline std::string audio_integer_case_name(AudioFormat source_format, AudioFormat destination_format,
                                           std::size_t count, std::size_t source_offset,
                                           std::size_t destination_offset,
                                           const Variant<AudioConvertFunction>& variant) {
  std::ostringstream stream;
  stream << audio_format_name(source_format) << "To" << audio_format_name(destination_format)
         << "_Count" << count << "_SrcOffset" << source_offset << "_DstOffset"
         << destination_offset << "_PatternBoundaryValues_" << audio_variant_name(variant);
  return stream.str();
}

inline AudioIntegerCase make_audio_integer_case(
    AudioFormat source_format, AudioFormat destination_format, std::size_t count,
    Variant<AudioConvertFunction> variant, std::string expected_hash = {}) {
  AudioIntegerCase result{source_format,
                          destination_format,
                          count,
                          audio_alignment_offset(source_format),
                          audio_alignment_offset(destination_format),
                          std::move(variant),
                          std::move(expected_hash),
                          {}};
  result.name = audio_integer_case_name(
      result.source_format, result.destination_format, result.count,
      result.source_alignment_offset, result.destination_alignment_offset, result.variant);
  return result;
}

inline void PrintTo(const AudioIntegerCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

inline std::int64_t integer_anchor(AudioFormat format, std::size_t index) {
  switch (format) {
    case AudioFormat::U8: {
      constexpr std::array<std::int64_t, 10> values{0, 1, 2, 127, 128,
                                                     129, 254, 255, 42, 213};
      return values[index % values.size()];
    }
    case AudioFormat::S16: {
      constexpr std::array<std::int64_t, 12> values{
          std::numeric_limits<std::int16_t>::min(), -32767, -32768, -257, -256, -1,
          0, 1, 255, 256, 32766, std::numeric_limits<std::int16_t>::max()};
      return values[index % values.size()];
    }
    case AudioFormat::S32: {
      constexpr std::array<std::int64_t, 14> values{
          std::numeric_limits<std::int32_t>::min(),
          static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) + 1,
          -2147483647LL,
          -16777217LL,
          -16777216LL,
          -65536LL,
          -1LL,
          0LL,
          1LL,
          65535LL,
          65536LL,
          16777215LL,
          16777216LL,
          std::numeric_limits<std::int32_t>::max()};
      return values[index % values.size()];
    }
    case AudioFormat::S24:
    case AudioFormat::F32:
      break;
  }
  throw std::invalid_argument("integer anchor requested for non-integer format");
}

inline void write_u16_le(std::uint8_t* destination, std::uint16_t value) {
  destination[0] = static_cast<std::uint8_t>(value);
  destination[1] = static_cast<std::uint8_t>(value >> 8);
}

inline void write_u32_le(std::uint8_t* destination, std::uint32_t value) {
  destination[0] = static_cast<std::uint8_t>(value);
  destination[1] = static_cast<std::uint8_t>(value >> 8);
  destination[2] = static_cast<std::uint8_t>(value >> 16);
  destination[3] = static_cast<std::uint8_t>(value >> 24);
}

inline void fill_integer_audio_source(GuardedAudioBuffer& buffer, AudioFormat format,
                                      std::size_t count) {
  const auto bytes = audio_format_bytes(format);
  if (buffer.active_bytes() != count * bytes) {
    throw std::invalid_argument("audio source size does not match count");
  }
  for (std::size_t index = 0; index < count; ++index) {
    auto* destination = buffer.data() + index * bytes;
    const auto value = integer_anchor(format, index);
    switch (format) {
      case AudioFormat::U8:
        destination[0] = static_cast<std::uint8_t>(value);
        break;
      case AudioFormat::S16:
        write_u16_le(destination, static_cast<std::uint16_t>(static_cast<std::int16_t>(value)));
        break;
      case AudioFormat::S32:
        write_u32_le(destination, static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
        break;
      case AudioFormat::S24:
      case AudioFormat::F32:
        throw std::invalid_argument("unsupported integer source format");
    }
  }
}

inline void copy_bytes(std::uint8_t* destination, const std::uint8_t* source, std::size_t count) {
  std::memcpy(destination, source, count);
}

inline void convert_integer_reference(AudioFormat source_format, AudioFormat destination_format,
                                      const std::uint8_t* source, std::uint8_t* destination,
                                      std::size_t count) {
  if (source_format == AudioFormat::S32 && destination_format == AudioFormat::S16) {
    for (std::size_t i = 0; i < count; ++i)
      copy_bytes(destination + i * 2, source + i * 4 + 2, 2);
    return;
  }
  if (source_format == AudioFormat::S16 && destination_format == AudioFormat::S32) {
    for (std::size_t i = 0; i < count; ++i) {
      destination[i * 4] = 0;
      destination[i * 4 + 1] = 0;
      copy_bytes(destination + i * 4 + 2, source + i * 2, 2);
    }
    return;
  }
  if (source_format == AudioFormat::S32 && destination_format == AudioFormat::U8) {
    for (std::size_t i = 0; i < count; ++i)
      destination[i] = static_cast<std::uint8_t>(static_cast<std::int8_t>(source[i * 4 + 3]) + 128);
    return;
  }
  if (source_format == AudioFormat::U8 && destination_format == AudioFormat::S32) {
    for (std::size_t i = 0; i < count; ++i) {
      destination[i * 4] = 0;
      destination[i * 4 + 1] = 0;
      destination[i * 4 + 2] = 0;
      destination[i * 4 + 3] = static_cast<std::uint8_t>(
          static_cast<std::int16_t>(source[i]) - static_cast<std::int16_t>(128));
    }
    return;
  }
  if (source_format == AudioFormat::S16 && destination_format == AudioFormat::U8) {
    for (std::size_t i = 0; i < count; ++i)
      destination[i] = static_cast<std::uint8_t>(static_cast<std::int8_t>(source[i * 2 + 1]) + 128);
    return;
  }
  if (source_format == AudioFormat::U8 && destination_format == AudioFormat::S16) {
    for (std::size_t i = 0; i < count; ++i) {
      destination[i * 2] = 0;
      destination[i * 2 + 1] = static_cast<std::uint8_t>(
          static_cast<std::int16_t>(source[i]) - static_cast<std::int16_t>(128));
    }
    return;
  }
  throw std::invalid_argument("unsupported basic integer conversion");
}

inline std::uint64_t hash_audio_active(const GuardedAudioBuffer& buffer) {
  const PlaneView<const std::uint8_t> view(buffer.data(), buffer.active_bytes(), 1,
                                           buffer.active_bytes());
  return hash_active(view);
}

inline ::testing::AssertionResult compare_audio_exact(const GuardedAudioBuffer& expected,
                                                      const GuardedAudioBuffer& actual) {
  if (expected.active_bytes() != actual.active_bytes()) {
    return ::testing::AssertionFailure() << "active byte count mismatch";
  }
  for (std::size_t index = 0; index < expected.active_bytes(); ++index) {
    if (expected.data()[index] != actual.data()[index]) {
      return ::testing::AssertionFailure()
             << "byte=" << index << " expected=" << static_cast<unsigned>(expected.data()[index])
             << " actual=" << static_cast<unsigned>(actual.data()[index]);
    }
  }
  return ::testing::AssertionSuccess();
}

inline void run_audio_integer_case(const AudioIntegerCase& test_case) {
  const auto source_bytes = test_case.count * audio_format_bytes(test_case.source_format);
  const auto destination_bytes = test_case.count * audio_format_bytes(test_case.destination_format);
  GuardedAudioBuffer source(source_bytes, 64, 64, test_case.source_alignment_offset);
  GuardedAudioBuffer expected(destination_bytes, 64, 64, test_case.destination_alignment_offset);
  GuardedAudioBuffer actual(destination_bytes, 64, 64, test_case.destination_alignment_offset);

  fill_integer_audio_source(source, test_case.source_format, test_case.count);
  const auto source_snapshot = source.snapshot_active();
  expected.fill_active(0xCD);
  actual.fill_active(0xCD);
  convert_integer_reference(test_case.source_format, test_case.destination_format, source.data(),
                            expected.data(), test_case.count);

  test_case.variant.function(source.data(), actual.data(), static_cast<int>(test_case.count));

  EXPECT_TRUE(compare_audio_exact(expected, actual))
      << test_case.name << " reference mismatch for variant " << test_case.variant.name;
  if (!test_case.expected_hash.empty()) {
    EXPECT_EQ(format_hash(hash_audio_active(actual)), test_case.expected_hash) << test_case.name;
  }
  EXPECT_TRUE(source.active_matches(source_snapshot)) << test_case.name << " modified source";
  EXPECT_TRUE(source.memory_intact()) << test_case.name << " source guard or padding corruption";
  EXPECT_TRUE(expected.memory_intact()) << test_case.name << " reference guard or padding corruption";
  EXPECT_TRUE(actual.memory_intact()) << test_case.name << " output guard or padding corruption";
}

}  // namespace avsut::test
