#pragma once

#include "filters/resample_functions.h"
#ifndef AVS_UNUSED
// resample.h uses this macro after avisynth.h undefines it at the header end.
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_RESAMPLE_UNDEF_AVS_UNUSED
#endif
#include "filters/resample.h"
#ifdef AVSUT_RESAMPLE_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_RESAMPLE_UNDEF_AVS_UNUSED
#endif
#include "filters/intel/resample_avx2.h"
#include "filters/intel/resample_sse.h"

#include "support/comparators.h"
#include "support/guarded_video_buffer.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace avsut::test {

using ResizeFunction = void (*)(BYTE*, const BYTE*, int, int,
                                ResamplingProgram*, int, int, int);

class ScriptEnvironmentOwner {
 public:
  ScriptEnvironmentOwner() : environment_(CreateScriptEnvironment2()) {
    if (environment_ == nullptr) {
      throw std::runtime_error("CreateScriptEnvironment2 failed");
    }
  }

  ~ScriptEnvironmentOwner() {
    if (environment_ != nullptr) {
      environment_->DeleteScriptEnvironment();
    }
  }

  ScriptEnvironmentOwner(const ScriptEnvironmentOwner&) = delete;
  ScriptEnvironmentOwner& operator=(const ScriptEnvironmentOwner&) = delete;

  IScriptEnvironment* get() const noexcept { return environment_; }

 private:
  IScriptEnvironment2* environment_{};
};

class ResamplingProgramOwner {
 public:
  ResamplingProgramOwner(int source_size, int target_size, int bits_per_pixel,
                         int coefficient_alignment = 16)
      : environment_(), filter_(), program_(filter_.GetResamplingProgram(
            source_size, 0.0, static_cast<double>(source_size), target_size,
            bits_per_pixel, 0.5, 0.5, environment_.get())) {
    if (!program_) {
      throw std::runtime_error("GetResamplingProgram returned null");
    }
    resize_prepare_coeffs(program_.get(), environment_.get(),
                          coefficient_alignment);
  }

  ~ResamplingProgramOwner() = default;

  ResamplingProgramOwner(const ResamplingProgramOwner&) = delete;
  ResamplingProgramOwner& operator=(const ResamplingProgramOwner&) = delete;

  ResamplingProgram* get() const noexcept { return program_.get(); }

 private:
  ScriptEnvironmentOwner environment_;
  TriangleFilter filter_;
  std::unique_ptr<ResamplingProgram> program_;
};

struct ResizeVertical8Case {
  std::size_t width_pixels{};
  std::size_t source_height{};
  std::size_t target_height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<ResizeFunction> variant;
  std::string expected_hash;
  std::string name;
};

struct ResizeVertical16Case {
  int bits_per_pixel{};
  std::size_t width_pixels{};
  std::size_t source_height{};
  std::size_t target_height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<ResizeFunction> variant;
  std::string expected_hash;
  std::string name;
};

struct ResizeHorizontal8Case {
  std::size_t source_width{};
  std::size_t target_width{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<ResizeFunction> variant;
  std::string expected_hash;
  std::string name;
};

struct ResizeHorizontal16Case {
  int bits_per_pixel{};
  std::size_t source_width{};
  std::size_t target_width{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<ResizeFunction> variant;
  std::string expected_hash;
  std::string name;
};

template <typename Function>
std::string resize_variant_name(const Variant<Function>& variant) {
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

inline std::string resize_vertical8_case_name(
    std::size_t width_pixels, std::size_t source_height,
    std::size_t target_height, std::size_t source_pitch,
    std::size_t destination_pitch, const Variant<ResizeFunction>& variant) {
  std::ostringstream stream;
  stream << "Plane8Vertical_Width" << width_pixels
         << "_SourceHeight" << source_height
         << "_TargetHeight" << target_height
         << "_SrcPitch" << source_pitch
         << "_DstPitch" << destination_pitch
         << "_FilterTriangle_PatternBoundaryRamp_"
         << resize_variant_name(variant);
  return stream.str();
}

inline std::string resize_vertical16_case_name(
    int bits_per_pixel, std::size_t width_pixels, std::size_t source_height,
    std::size_t target_height, std::size_t source_pitch,
    std::size_t destination_pitch, const Variant<ResizeFunction>& variant) {
  std::ostringstream stream;
  stream << "Plane16Vertical_Bits" << bits_per_pixel
         << "_Width" << width_pixels
         << "_SourceHeight" << source_height
         << "_TargetHeight" << target_height
         << "_SrcPitch" << source_pitch
         << "_DstPitch" << destination_pitch
         << "_FilterTriangle_PatternBoundaryRamp_"
         << resize_variant_name(variant);
  return stream.str();
}

inline std::string resize_horizontal8_case_name(
    std::size_t source_width, std::size_t target_width, std::size_t height,
    std::size_t source_pitch, std::size_t destination_pitch,
    const Variant<ResizeFunction>& variant) {
  std::ostringstream stream;
  stream << "Plane8Horizontal_SourceWidth" << source_width
         << "_TargetWidth" << target_width
         << "_Height" << height
         << "_SrcPitch" << source_pitch
         << "_DstPitch" << destination_pitch
         << "_FilterTriangle_PatternBoundaryRamp_"
         << resize_variant_name(variant);
  return stream.str();
}

inline std::string resize_horizontal16_case_name(
    int bits_per_pixel, std::size_t source_width, std::size_t target_width,
    std::size_t height, std::size_t source_pitch,
    std::size_t destination_pitch, const Variant<ResizeFunction>& variant) {
  std::ostringstream stream;
  stream << "Plane16Horizontal_Bits" << bits_per_pixel
         << "_SourceWidth" << source_width
         << "_TargetWidth" << target_width
         << "_Height" << height
         << "_SrcPitch" << source_pitch
         << "_DstPitch" << destination_pitch
         << "_FilterTriangle_PatternBoundaryRamp_"
         << resize_variant_name(variant);
  return stream.str();
}

inline ResizeVertical8Case make_resize_vertical8_case(
    std::size_t width_pixels, std::size_t source_height,
    std::size_t target_height, std::size_t source_pitch,
    std::size_t destination_pitch, Variant<ResizeFunction> variant,
    std::string expected_hash = {}) {
  ResizeVertical8Case result{width_pixels, source_height, target_height,
                             source_pitch, destination_pitch,
                             std::move(variant), std::move(expected_hash), {}};
  result.name = resize_vertical8_case_name(
      result.width_pixels, result.source_height, result.target_height,
      result.source_pitch, result.destination_pitch, result.variant);
  return result;
}

inline ResizeVertical16Case make_resize_vertical16_case(
    int bits_per_pixel, std::size_t width_pixels, std::size_t source_height,
    std::size_t target_height, std::size_t source_pitch,
    std::size_t destination_pitch, Variant<ResizeFunction> variant,
    std::string expected_hash = {}) {
  ResizeVertical16Case result{bits_per_pixel, width_pixels, source_height,
                              target_height, source_pitch, destination_pitch,
                              std::move(variant), std::move(expected_hash), {}};
  result.name = resize_vertical16_case_name(
      result.bits_per_pixel, result.width_pixels, result.source_height,
      result.target_height, result.source_pitch, result.destination_pitch,
      result.variant);
  return result;
}

inline ResizeHorizontal8Case make_resize_horizontal8_case(
    std::size_t source_width, std::size_t target_width, std::size_t height,
    std::size_t source_pitch, std::size_t destination_pitch,
    Variant<ResizeFunction> variant, std::string expected_hash = {}) {
  ResizeHorizontal8Case result{
      source_width, target_width, height, source_pitch, destination_pitch,
      std::move(variant), std::move(expected_hash), {}};
  result.name = resize_horizontal8_case_name(
      result.source_width, result.target_width, result.height,
      result.source_pitch, result.destination_pitch, result.variant);
  return result;
}

inline ResizeHorizontal16Case make_resize_horizontal16_case(
    int bits_per_pixel, std::size_t source_width, std::size_t target_width,
    std::size_t height, std::size_t source_pitch,
    std::size_t destination_pitch, Variant<ResizeFunction> variant,
    std::string expected_hash = {}) {
  ResizeHorizontal16Case result{
      bits_per_pixel, source_width, target_width, height, source_pitch,
      destination_pitch, std::move(variant), std::move(expected_hash), {}};
  result.name = resize_horizontal16_case_name(
      result.bits_per_pixel, result.source_width, result.target_width,
      result.height, result.source_pitch, result.destination_pitch,
      result.variant);
  return result;
}

inline void PrintTo(const ResizeVertical8Case& test_case,
                    std::ostream* stream) {
  *stream << test_case.name;
}

inline void PrintTo(const ResizeVertical16Case& test_case,
                    std::ostream* stream) {
  *stream << test_case.name;
}

inline void PrintTo(const ResizeHorizontal8Case& test_case,
                    std::ostream* stream) {
  *stream << test_case.name;
}

inline void PrintTo(const ResizeHorizontal16Case& test_case,
                    std::ostream* stream) {
  *stream << test_case.name;
}

template <typename T>
void fill_resize_input(PlaneView<T> view, int bits_per_pixel) {
  static_assert(!std::is_const_v<T>);
  using Value = std::remove_const_t<T>;
  const std::uint64_t max_value =
      (std::uint64_t{1} << bits_per_pixel) - 1U;
  const std::array<std::uint64_t, 10> anchors{
      0U, 1U, max_value / 7U, max_value / 3U, max_value / 2U,
      max_value - 2U, max_value - 1U, max_value, 17U, 193U};

  for (std::size_t y = 0; y < view.height(); ++y) {
    for (std::size_t x = 0; x < view.width(); ++x) {
      const std::size_t index = y * view.width() + x;
      const std::uint64_t value =
          (index % anchors.size() == 0U)
              ? anchors[(index / anchors.size()) % anchors.size()]
              : (x * 37U + y * 101U + index * 13U) % (max_value + 1U);
      view.row(y)[x] = static_cast<Value>(value);
    }
  }
}

template <typename T>
void apply_resize_vertical_reference(PlaneView<const T> source,
                                     PlaneView<T> destination,
                                     const ResamplingProgram& program,
                                     int bits_per_pixel) {
  static_assert(std::is_integral_v<T>);
  ASSERT_EQ(source.width(), destination.width());
  ASSERT_EQ(destination.height(),
            static_cast<std::size_t>(program.target_size));

  constexpr bool is_8_bit = sizeof(T) == sizeof(std::uint8_t);
  const int scale_bits = is_8_bit ? FPScale8bits : FPScale16bits;
  const int rounder = 1 << (scale_bits - 1);
  const int limit = (1 << bits_per_pixel) - 1;

  for (std::size_t y = 0; y < destination.height(); ++y) {
    const int offset = program.pixel_offset[y];
    const short* coefficients =
        program.pixel_coefficient + y * program.filter_size;
    for (std::size_t x = 0; x < destination.width(); ++x) {
      int result = rounder;
      for (int i = 0; i < program.filter_size_real; ++i) {
        int value = source.row(static_cast<std::size_t>(offset + i))[x];
        if constexpr (sizeof(T) == sizeof(std::uint16_t)) {
          if (bits_per_pixel == 16) {
            value -= 32768;
          }
        }
        result += value * coefficients[i];
      }
      if constexpr (sizeof(T) == sizeof(std::uint16_t)) {
        if (bits_per_pixel == 16) {
          result += 32768 << FPScale16bits;
        }
      }
      result >>= scale_bits;
      destination.row(y)[x] = static_cast<T>(std::clamp(result, 0, limit));
    }
  }
}

template <typename T>
void invoke_resize_vertical(const ResizeFunction function,
                            PlaneView<T> destination,
                            PlaneView<const T> source,
                            ResamplingProgram* program, int bits_per_pixel) {
  function(reinterpret_cast<BYTE*>(destination.data()),
           reinterpret_cast<const BYTE*>(source.data()),
           static_cast<int>(destination.pitch_bytes()),
           static_cast<int>(source.pitch_bytes()), program,
           static_cast<int>(destination.width()),
           static_cast<int>(destination.height()), bits_per_pixel);
}

template <typename T>
void apply_resize_horizontal_reference(PlaneView<const T> source,
                                       PlaneView<T> destination,
                                       const ResamplingProgram& program,
                                       int bits_per_pixel) {
  static_assert(std::is_integral_v<T>);
  const int scale_bits = sizeof(T) == sizeof(std::uint8_t)
                             ? FPScale8bits
                             : FPScale16bits;
  const int rounder = 1 << (scale_bits - 1);
  const int limit = (1 << bits_per_pixel) - 1;

  for (std::size_t y = 0; y < destination.height(); ++y) {
    for (std::size_t x = 0; x < destination.width(); ++x) {
      const int offset = program.pixel_offset[x];
      const short* coefficients =
          program.pixel_coefficient + x * program.filter_size;
      int result = rounder;
      for (int i = 0; i < program.filter_size_real; ++i) {
        int value = source.row(y)[static_cast<std::size_t>(offset + i)];
        if constexpr (sizeof(T) == sizeof(std::uint16_t)) {
          if (bits_per_pixel == 16) {
            value -= 32768;
          }
        }
        result += value * coefficients[i];
      }
      if constexpr (sizeof(T) == sizeof(std::uint16_t)) {
        if (bits_per_pixel == 16) {
          result += 32768 << FPScale16bits;
        }
      }
      result >>= scale_bits;
      destination.row(y)[x] = static_cast<T>(std::clamp(result, 0, limit));
    }
  }
}

template <typename T>
void invoke_resize_horizontal(const ResizeFunction function,
                              PlaneView<T> destination,
                              PlaneView<const T> source,
                              ResamplingProgram* program, int bits_per_pixel) {
  function(reinterpret_cast<BYTE*>(destination.data()),
           reinterpret_cast<const BYTE*>(source.data()),
           static_cast<int>(destination.pitch_bytes()),
           static_cast<int>(source.pitch_bytes()), program,
           static_cast<int>(destination.width()),
           static_cast<int>(destination.height()), bits_per_pixel);
}

template <typename T>
void run_resize_vertical_case(std::size_t width_pixels,
                              std::size_t source_height,
                              std::size_t target_height,
                              std::size_t source_pitch,
                              std::size_t destination_pitch, int bits_per_pixel,
                              const Variant<ResizeFunction>& variant,
                              const std::string& expected_hash) {
  GuardedVideoBuffer<T> source(width_pixels, source_height, source_pitch, 64);
  GuardedVideoBuffer<T> expected(width_pixels, target_height, destination_pitch,
                                 64);
  GuardedVideoBuffer<T> actual(width_pixels, target_height, destination_pitch,
                               64);
  fill_resize_input(source.view(), bits_per_pixel);
  const auto source_snapshot = source.snapshot_active();

  ResamplingProgramOwner program(static_cast<int>(source_height),
                                 static_cast<int>(target_height),
                                 bits_per_pixel);
  apply_resize_vertical_reference(source.view().as_const(), expected.view(),
                                  *program.get(), bits_per_pixel);
  invoke_resize_vertical(variant.function, actual.view(), source.view().as_const(),
                         program.get(), bits_per_pixel);

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << "variant=" << variant.name << " bits=" << bits_per_pixel
      << " width=" << width_pixels << " source_height=" << source_height
      << " target_height=" << target_height << " source_pitch=" << source_pitch
      << " destination_pitch=" << destination_pitch;
  EXPECT_EQ(format_hash(hash_active(actual.view().as_const())), expected_hash)
      << "variant=" << variant.name << " stable output hash mismatch";
  EXPECT_TRUE(source.active_matches(source_snapshot))
      << "variant=" << variant.name << " modified source pixels";
  EXPECT_TRUE(source.memory_intact())
      << "variant=" << variant.name << " corrupted source padding or guards";
  EXPECT_TRUE(expected.memory_intact())
      << "variant=" << variant.name << " corrupted reference padding or guards";
  EXPECT_TRUE(actual.memory_intact())
      << "variant=" << variant.name << " corrupted output padding or guards";
}

inline void run_resize_vertical8_case(const ResizeVertical8Case& test_case) {
  run_resize_vertical_case<std::uint8_t>(
      test_case.width_pixels, test_case.source_height, test_case.target_height,
      test_case.source_pitch, test_case.destination_pitch, 8, test_case.variant,
      test_case.expected_hash);
}

inline void run_resize_vertical16_case(const ResizeVertical16Case& test_case) {
  run_resize_vertical_case<std::uint16_t>(
      test_case.width_pixels, test_case.source_height, test_case.target_height,
      test_case.source_pitch, test_case.destination_pitch,
      test_case.bits_per_pixel, test_case.variant, test_case.expected_hash);
}

template <typename T>
void run_resize_horizontal_case(
    std::size_t source_width, std::size_t target_width, std::size_t height,
    std::size_t source_pitch, std::size_t destination_pitch, int bits_per_pixel,
    const Variant<ResizeFunction>& variant, const std::string& expected_hash) {
  GuardedVideoBuffer<T> source(source_width, height, source_pitch, 64);
  GuardedVideoBuffer<T> expected(target_width, height, destination_pitch, 64);
  GuardedVideoBuffer<T> actual(target_width, height, destination_pitch, 64);
  fill_resize_input(source.view(), bits_per_pixel);
  const auto source_snapshot = source.snapshot_active();

  ResamplingProgramOwner program(static_cast<int>(source_width),
                                 static_cast<int>(target_width),
                                 bits_per_pixel);
  apply_resize_horizontal_reference(source.view().as_const(), expected.view(),
                                    *program.get(), bits_per_pixel);
  invoke_resize_horizontal(variant.function, actual.view(), source.view().as_const(),
                           program.get(), bits_per_pixel);

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << "variant=" << variant.name << " bits=" << bits_per_pixel
      << " source_width=" << source_width << " target_width=" << target_width
      << " height=" << height << " source_pitch=" << source_pitch
      << " destination_pitch=" << destination_pitch;
  EXPECT_EQ(format_hash(hash_active(actual.view().as_const())), expected_hash)
      << "variant=" << variant.name << " stable output hash mismatch";
  EXPECT_TRUE(source.active_matches(source_snapshot))
      << "variant=" << variant.name << " modified source pixels";
  EXPECT_TRUE(source.memory_intact())
      << "variant=" << variant.name << " corrupted source padding or guards";
  EXPECT_TRUE(expected.memory_intact())
      << "variant=" << variant.name << " corrupted reference padding or guards";
  EXPECT_TRUE(actual.memory_intact())
      << "variant=" << variant.name << " corrupted output padding or guards";
}

inline void run_resize_horizontal8_case(
    const ResizeHorizontal8Case& test_case) {
  run_resize_horizontal_case<std::uint8_t>(
      test_case.source_width, test_case.target_width, test_case.height,
      test_case.source_pitch, test_case.destination_pitch, 8, test_case.variant,
      test_case.expected_hash);
}

inline void run_resize_horizontal16_case(
    const ResizeHorizontal16Case& test_case) {
  run_resize_horizontal_case<std::uint16_t>(
      test_case.source_width, test_case.target_width, test_case.height,
      test_case.source_pitch, test_case.destination_pitch,
      test_case.bits_per_pixel, test_case.variant, test_case.expected_hash);
}

}  // namespace avsut::test
