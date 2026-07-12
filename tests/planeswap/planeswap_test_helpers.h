#pragma once

#include "filters/intel/planeswap_avx2.h"
#include "filters/intel/planeswap_sse.h"

#include "support/comparators.h"
#include "support/guarded_video_buffer.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace avsut::test {

using PlaneSwapFuncPtr = void (*)(const BYTE*, BYTE*, int, int, int, int);

struct Yuy2SwapCase {
  std::size_t width_bytes{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<PlaneSwapFuncPtr> variant;
  std::string expected_hash;
  std::string name;
};

struct RgbExtractCase {
  std::string format;
  int channel_index{};
  std::size_t width_pixels{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  std::size_t bytes_per_channel{};
  PlaneSwapFuncPtr function{};
  Variant<PlaneSwapFuncPtr> variant;
  std::string expected_hash;
  std::string name;
};

template <typename Function>
inline std::string planeswap_variant_name(const Variant<Function>& variant) {
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

inline const char* planeswap_channel_name(int channel_index) {
  switch (channel_index) {
    case 0:
      return "B";
    case 1:
      return "G";
    case 2:
      return "R";
    case 3:
      return "A";
    default:
      return "Unknown";
  }
}

inline std::string yuy2_case_name(std::size_t width_bytes, std::size_t height,
                                  std::size_t source_pitch, std::size_t destination_pitch,
                                  const Variant<PlaneSwapFuncPtr>& variant) {
  std::ostringstream stream;
  stream << "Yuy2Swap_Width" << width_bytes << "_Height" << height << "_SrcPitch" << source_pitch
         << "_DstPitch" << destination_pitch << "_PatternChannelRamp_"
         << planeswap_variant_name(variant);
  return stream.str();
}

inline std::string rgb_case_name(const RgbExtractCase& test_case) {
  std::ostringstream stream;
  stream << test_case.format << "Channel" << planeswap_channel_name(test_case.channel_index)
         << "_Width" << test_case.width_pixels << "_Height" << test_case.height << "_SrcPitch"
         << test_case.source_pitch << "_DstPitch" << test_case.destination_pitch
         << "_PatternChannelRamp_" << planeswap_variant_name(test_case.variant);
  return stream.str();
}

inline Yuy2SwapCase make_yuy2_case(std::size_t width_bytes, std::size_t height,
                                   std::size_t source_pitch, std::size_t destination_pitch,
                                   Variant<PlaneSwapFuncPtr> variant, std::string expected_hash) {
  Yuy2SwapCase result{width_bytes,
                      height,
                      source_pitch,
                      destination_pitch,
                      std::move(variant),
                      std::move(expected_hash),
                      {}};
  result.name = yuy2_case_name(result.width_bytes, result.height, result.source_pitch,
                               result.destination_pitch, result.variant);
  return result;
}

inline RgbExtractCase make_rgb_case(std::string format, int channel_index, std::size_t width_pixels,
                                    std::size_t height, std::size_t source_pitch,
                                    std::size_t destination_pitch, std::size_t bytes_per_channel,
                                    PlaneSwapFuncPtr function, Variant<PlaneSwapFuncPtr> variant,
                                    std::string expected_hash) {
  RgbExtractCase result{std::move(format),
                        channel_index,
                        width_pixels,
                        height,
                        source_pitch,
                        destination_pitch,
                        bytes_per_channel,
                        function,
                        std::move(variant),
                        std::move(expected_hash),
                        {}};
  result.name = rgb_case_name(result);
  return result;
}

inline void PrintTo(const Yuy2SwapCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

inline void PrintTo(const RgbExtractCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

inline void fill_yuy2_input(PlaneView<std::uint8_t> view) {
  for (std::size_t y = 0; y < view.height(); ++y) {
    for (std::size_t x = 0; x < view.width(); x += 4) {
      view.row(y)[x + 0] = static_cast<std::uint8_t>(17 + y * 23 + x * 5);
      view.row(y)[x + 1] = static_cast<std::uint8_t>(31 + y * 29 + x * 7);
      view.row(y)[x + 2] = static_cast<std::uint8_t>(43 + y * 31 + x * 11);
      view.row(y)[x + 3] = static_cast<std::uint8_t>(59 + y * 37 + x * 13);
    }
  }
}

inline void apply_yuy2_reference(PlaneView<const std::uint8_t> source,
                                 PlaneView<std::uint8_t> destination) {
  for (std::size_t y = 0; y < source.height(); ++y) {
    for (std::size_t x = 0; x < source.width(); x += 4) {
      destination.row(y)[x + 0] = source.row(y)[x + 0];
      destination.row(y)[x + 1] = source.row(y)[x + 3];
      destination.row(y)[x + 2] = source.row(y)[x + 2];
      destination.row(y)[x + 3] = source.row(y)[x + 1];
    }
  }
}

inline void run_yuy2_case(const Yuy2SwapCase& test_case) {
  GuardedVideoBuffer<std::uint8_t> source(test_case.width_bytes, test_case.height,
                                          test_case.source_pitch, 32);
  GuardedVideoBuffer<std::uint8_t> expected(test_case.width_bytes, test_case.height,
                                            test_case.destination_pitch, 32);
  GuardedVideoBuffer<std::uint8_t> actual(test_case.width_bytes, test_case.height,
                                          test_case.destination_pitch, 32);

  fill_yuy2_input(source.view());
  const auto source_snapshot = source.snapshot_active();
  apply_yuy2_reference(source.view().as_const(), expected.view());

  test_case.variant.function(
      reinterpret_cast<const BYTE*>(source.view().data()),
      reinterpret_cast<BYTE*>(actual.view().data()), static_cast<int>(source.view().pitch_bytes()),
      static_cast<int>(actual.view().pitch_bytes()), static_cast<int>(test_case.width_bytes),
      static_cast<int>(test_case.height));

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << test_case.name << " reference mismatch for variant " << test_case.variant.name;
  if (!test_case.expected_hash.empty()) {
    EXPECT_EQ(format_hash(hash_active(expected.view().as_const())), test_case.expected_hash)
        << test_case.name << " stable output hash mismatch";
  }
  EXPECT_TRUE(source.active_matches(source_snapshot))
      << test_case.name << " modified the source input";
  EXPECT_TRUE(source.memory_intact())
      << test_case.name << " source padding or guards were corrupted";
  EXPECT_TRUE(expected.memory_intact())
      << test_case.name << " reference padding or guards were corrupted";
  EXPECT_TRUE(actual.memory_intact())
      << test_case.name << " output padding or guards were corrupted";
}

template <typename T>
void fill_rgb_input(PlaneView<T> view, std::size_t width_pixels) {
  static_assert(std::is_integral_v<T>);
  constexpr std::size_t kComponents = 4;
  const auto max_value = static_cast<std::uint32_t>(std::numeric_limits<T>::max());
  for (std::size_t y = 0; y < view.height(); ++y) {
    for (std::size_t x = 0; x < width_pixels; ++x) {
      for (std::size_t channel = 0; channel < kComponents; ++channel) {
        const auto value = 7U + static_cast<unsigned int>(x * 37) +
                           static_cast<unsigned int>(y * 101) +
                           static_cast<unsigned int>(channel * 53);
        view.row(y)[x * kComponents + channel] = static_cast<T>(value & max_value);
      }
    }
  }
}

template <typename T>
void apply_rgb_reference(const RgbExtractCase& test_case, PlaneView<const T> source,
                         PlaneView<T> destination) {
  for (std::size_t output_y = 0; output_y < test_case.height; ++output_y) {
    const auto source_y = test_case.height - 1 - output_y;
    for (std::size_t x = 0; x < test_case.width_pixels; ++x) {
      destination.row(output_y)[x] = source.row(source_y)[x * 4 + test_case.channel_index];
    }
  }
}

template <typename T>
void run_rgb_case_typed(const RgbExtractCase& test_case) {
  const auto source_width = test_case.width_pixels * 4;
  GuardedVideoBuffer<T> source(source_width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<T> expected(test_case.width_pixels, test_case.height,
                                 test_case.destination_pitch, 64);
  GuardedVideoBuffer<T> actual(test_case.width_pixels, test_case.height,
                               test_case.destination_pitch, 64);

  fill_rgb_input(source.view(), test_case.width_pixels);
  const auto source_snapshot = source.snapshot_active();
  apply_rgb_reference(test_case, source.view().as_const(), expected.view());

  auto* source_bottom = reinterpret_cast<const BYTE*>(source.view().data()) +
                        (test_case.height - 1) * test_case.source_pitch;
  test_case.variant.function(
      source_bottom, reinterpret_cast<BYTE*>(actual.view().data()),
      static_cast<int>(test_case.source_pitch), static_cast<int>(test_case.destination_pitch),
      static_cast<int>(test_case.width_pixels), static_cast<int>(test_case.height));

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << test_case.name << " reference mismatch for variant " << test_case.variant.name;
  if (!test_case.expected_hash.empty()) {
    EXPECT_EQ(format_hash(hash_active(expected.view().as_const())), test_case.expected_hash)
        << test_case.name << " stable output hash mismatch";
  }
  EXPECT_TRUE(source.active_matches(source_snapshot))
      << test_case.name << " modified the source input";
  EXPECT_TRUE(source.memory_intact())
      << test_case.name << " source padding or guards were corrupted";
  EXPECT_TRUE(expected.memory_intact())
      << test_case.name << " reference padding or guards were corrupted";
  EXPECT_TRUE(actual.memory_intact())
      << test_case.name << " output padding or guards were corrupted";
}

}  // namespace avsut::test
