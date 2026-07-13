#include "core/AviHelper.h"

#include "support/comparators.h"
#include "support/cpu_features.h"
#include "support/guarded_video_buffer.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace avsut::test {
namespace {

using ToY416Function = void (*)(uint8_t*, int, const uint8_t*, int, const uint8_t*, const uint8_t*,
                                int, const uint8_t*, int, int, int);
using BgraToArgbBeFunction = void (*)(uint8_t*, int, const uint8_t*, int, int, int);

struct ToY416Case {
  bool has_alpha{};
  std::size_t width_pixels{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<ToY416Function> variant;
  std::string expected_hash;
  std::string name;
};

struct BgraToArgbBeCase {
  std::size_t width_pixels{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<BgraToArgbBeFunction> variant;
  std::string expected_hash;
  std::string name;
};

template <typename Function>
std::string variant_name(const Variant<Function>& variant) {
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

inline std::string to_y416_case_name(const ToY416Case& test_case) {
  std::ostringstream stream;
  stream << "ToY416_" << (test_case.has_alpha ? "SourceAlpha" : "OpaqueAlpha") << "_Width"
         << test_case.width_pixels << "_Height" << test_case.height << "_SrcPitch"
         << test_case.source_pitch << "_DstPitch" << test_case.destination_pitch
         << "_PatternChannelAnchors_" << variant_name(test_case.variant);
  return stream.str();
}

inline std::string bgra_to_argb_be_case_name(const BgraToArgbBeCase& test_case) {
  std::ostringstream stream;
  stream << "BgraToArgbBE_Width" << test_case.width_pixels << "_Height" << test_case.height
         << "_SrcPitch" << test_case.source_pitch << "_DstPitch" << test_case.destination_pitch
         << "_PatternByteAnchors_" << variant_name(test_case.variant);
  return stream.str();
}

inline ToY416Case make_to_y416_case(bool has_alpha, Variant<ToY416Function> variant,
                                    std::string expected_hash) {
  ToY416Case result{has_alpha, 7, 3, 64, 80, std::move(variant), std::move(expected_hash), {}};
  result.name = to_y416_case_name(result);
  return result;
}

inline BgraToArgbBeCase make_bgra_to_argb_be_case(Variant<BgraToArgbBeFunction> variant,
                                                  std::string expected_hash) {
  BgraToArgbBeCase result{5, 3, 64, 64, std::move(variant), std::move(expected_hash), {}};
  result.name = bgra_to_argb_be_case_name(result);
  return result;
}

inline void PrintTo(const ToY416Case& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

inline void PrintTo(const BgraToArgbBeCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

template <typename T>
void fill_yuv444p16(PlaneView<T> y_plane, PlaneView<T> u_plane, PlaneView<T> v_plane,
                    PlaneView<T> alpha_plane) {
  static_assert(std::is_same_v<T, std::uint16_t>);
  constexpr std::array<std::uint16_t, 12> anchors{0,   1,    2,    3,     255,   256,
                                                  512, 1023, 4095, 32767, 65534, 65535};
  for (std::size_t y = 0; y < y_plane.height(); ++y) {
    for (std::size_t x = 0; x < y_plane.width(); ++x) {
      const auto sample = [&](std::size_t channel) {
        const auto anchor = anchors[(x * 3U + y * 5U + channel * 7U) % anchors.size()];
        return static_cast<std::uint16_t>(anchor + x * 37U + y * 101U + channel * 257U);
      };
      y_plane.row(y)[x] = sample(0);
      u_plane.row(y)[x] = sample(1);
      v_plane.row(y)[x] = sample(2);
      alpha_plane.row(y)[x] = sample(3);
    }
  }
}

inline void run_to_y416_case(const ToY416Case& test_case) {
  GuardedVideoBuffer<std::uint16_t> source_y(test_case.width_pixels, test_case.height,
                                             test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint16_t> source_u(test_case.width_pixels, test_case.height,
                                             test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint16_t> source_v(test_case.width_pixels, test_case.height,
                                             test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint16_t> source_a(test_case.width_pixels, test_case.height,
                                             test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint16_t> expected(test_case.width_pixels * 4, test_case.height,
                                             test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint16_t> actual(test_case.width_pixels * 4, test_case.height,
                                           test_case.destination_pitch, 64);

  fill_yuv444p16(source_y.view(), source_u.view(), source_v.view(), source_a.view());
  const auto y_snapshot = source_y.snapshot_active();
  const auto u_snapshot = source_u.snapshot_active();
  const auto v_snapshot = source_v.snapshot_active();
  const auto a_snapshot = source_a.snapshot_active();

  const ToY416Function reference = test_case.has_alpha ? ToY416_c<true> : ToY416_c<false>;
  reference(reinterpret_cast<uint8_t*>(expected.view().data()),
            static_cast<int>(expected.view().pitch_bytes()),
            reinterpret_cast<const uint8_t*>(source_y.view().data()),
            static_cast<int>(source_y.view().pitch_bytes()),
            reinterpret_cast<const uint8_t*>(source_u.view().data()),
            reinterpret_cast<const uint8_t*>(source_v.view().data()),
            static_cast<int>(source_u.view().pitch_bytes()),
            reinterpret_cast<const uint8_t*>(source_a.view().data()),
            static_cast<int>(source_a.view().pitch_bytes()),
            static_cast<int>(test_case.width_pixels), static_cast<int>(test_case.height));

  test_case.variant.function(reinterpret_cast<uint8_t*>(actual.view().data()),
                             static_cast<int>(actual.view().pitch_bytes()),
                             reinterpret_cast<const uint8_t*>(source_y.view().data()),
                             static_cast<int>(source_y.view().pitch_bytes()),
                             reinterpret_cast<const uint8_t*>(source_u.view().data()),
                             reinterpret_cast<const uint8_t*>(source_v.view().data()),
                             static_cast<int>(source_u.view().pitch_bytes()),
                             reinterpret_cast<const uint8_t*>(source_a.view().data()),
                             static_cast<int>(source_a.view().pitch_bytes()),
                             static_cast<int>(test_case.width_pixels),
                             static_cast<int>(test_case.height));

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << test_case.name << " reference mismatch for variant " << test_case.variant.name;
  EXPECT_EQ(format_hash(hash_active(expected.view().as_const())), test_case.expected_hash)
      << test_case.name << " stable output hash mismatch";
  EXPECT_TRUE(source_y.active_matches(y_snapshot)) << test_case.name;
  EXPECT_TRUE(source_u.active_matches(u_snapshot)) << test_case.name;
  EXPECT_TRUE(source_v.active_matches(v_snapshot)) << test_case.name;
  EXPECT_TRUE(source_a.active_matches(a_snapshot)) << test_case.name;
  EXPECT_TRUE(source_y.memory_intact()) << test_case.name;
  EXPECT_TRUE(source_u.memory_intact()) << test_case.name;
  EXPECT_TRUE(source_v.memory_intact()) << test_case.name;
  EXPECT_TRUE(source_a.memory_intact()) << test_case.name;
  EXPECT_TRUE(expected.memory_intact()) << test_case.name;
  EXPECT_TRUE(actual.memory_intact()) << test_case.name;
}

inline void fill_bgra_bytes(PlaneView<std::uint8_t> view, std::size_t width_pixels) {
  for (std::size_t y = 0; y < view.height(); ++y) {
    for (std::size_t x = 0; x < width_pixels; ++x) {
      for (std::size_t component = 0; component < 8; ++component) {
        view.row(y)[x * 8 + component] = static_cast<std::uint8_t>(
            3U + x * 29U + y * 47U + component * 61U + ((x + y + component) % 5U) * 17U);
      }
    }
  }
}

inline void run_bgra_to_argb_be_case(const BgraToArgbBeCase& test_case) {
  GuardedVideoBuffer<std::uint8_t> source(test_case.width_pixels * 8, test_case.height,
                                          test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> expected(test_case.width_pixels * 8, test_case.height,
                                            test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> actual(test_case.width_pixels * 8, test_case.height,
                                          test_case.destination_pitch, 64);
  fill_bgra_bytes(source.view(), test_case.width_pixels);
  const auto source_snapshot = source.snapshot_active();

  bgra_to_argbBE_c(reinterpret_cast<uint8_t*>(expected.view().data()),
                   static_cast<int>(expected.view().pitch_bytes()),
                   reinterpret_cast<const uint8_t*>(source.view().data()),
                   static_cast<int>(source.view().pitch_bytes()),
                   static_cast<int>(test_case.width_pixels), static_cast<int>(test_case.height));
  test_case.variant.function(reinterpret_cast<uint8_t*>(actual.view().data()),
                             static_cast<int>(actual.view().pitch_bytes()),
                             reinterpret_cast<const uint8_t*>(source.view().data()),
                             static_cast<int>(source.view().pitch_bytes()),
                             static_cast<int>(test_case.width_pixels),
                             static_cast<int>(test_case.height));

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << test_case.name << " reference mismatch for variant " << test_case.variant.name;
  EXPECT_EQ(format_hash(hash_active(expected.view().as_const())), test_case.expected_hash)
      << test_case.name << " stable output hash mismatch";
  EXPECT_TRUE(source.active_matches(source_snapshot)) << test_case.name;
  EXPECT_TRUE(source.memory_intact()) << test_case.name;
  EXPECT_TRUE(expected.memory_intact()) << test_case.name;
  EXPECT_TRUE(actual.memory_intact()) << test_case.name;
}

inline std::uint16_t swap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

TEST(BgrToRgbBe, ReordersAndByteSwapsPacked16BitPixels) {
  constexpr std::size_t width_pixels = 5;
  constexpr std::size_t height = 3;
  GuardedVideoBuffer<std::uint16_t> source(width_pixels * 3, height, 64, 64);
  GuardedVideoBuffer<std::uint16_t> expected(width_pixels * 3, height, 64, 64);
  GuardedVideoBuffer<std::uint16_t> actual(width_pixels * 3, height, 64, 64);

  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width_pixels * 3; ++x) {
      source.view().row(y)[x] = static_cast<std::uint16_t>(0x1000U + x * 0x31U + y * 0x207U);
    }
    for (std::size_t x = 0; x < width_pixels; ++x) {
      expected.view().row(y)[x * 3 + 0] = swap16(source.view().row(y)[x * 3 + 2]);
      expected.view().row(y)[x * 3 + 1] = swap16(source.view().row(y)[x * 3 + 1]);
      expected.view().row(y)[x * 3 + 2] = swap16(source.view().row(y)[x * 3 + 0]);
    }
  }
  const auto source_snapshot = source.snapshot_active();

  bgr_to_rgbBE_c(reinterpret_cast<uint8_t*>(actual.view().data()),
                 static_cast<int>(actual.view().pitch_bytes()),
                 reinterpret_cast<const uint8_t*>(source.view().data()),
                 static_cast<int>(source.view().pitch_bytes()), static_cast<int>(width_pixels),
                 static_cast<int>(height));

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()));
  EXPECT_EQ(format_hash(hash_active(expected.view().as_const())), "9f64ded7b9728cc6");
  EXPECT_TRUE(source.active_matches(source_snapshot));
  EXPECT_TRUE(source.memory_intact());
  EXPECT_TRUE(expected.memory_intact());
  EXPECT_TRUE(actual.memory_intact());
}

std::vector<ToY416Case> to_y416_cases() {
  const auto c_false = Variant<ToY416Function>{"c", ToY416_c<false>, IsaRequirement::Scalar};
  const auto c_true = Variant<ToY416Function>{"c", ToY416_c<true>, IsaRequirement::Scalar};
  const auto sse_false = Variant<ToY416Function>{"sse2", ToY416_sse2<false>, IsaRequirement::Sse2};
  const auto sse_true = Variant<ToY416Function>{"sse2", ToY416_sse2<true>, IsaRequirement::Sse2};
  return {make_to_y416_case(false, c_false, "c835b250e5c3f569"),
          make_to_y416_case(false, sse_false, "c835b250e5c3f569"),
          make_to_y416_case(true, c_true, "ad8e099a3bd0d8cf"),
          make_to_y416_case(true, sse_true, "ad8e099a3bd0d8cf")};
}

std::vector<BgraToArgbBeCase> bgra_to_argb_be_cases() {
  const auto hash = "f0a88e7431f4f42d";
  return {make_bgra_to_argb_be_case({"c", bgra_to_argbBE_c, IsaRequirement::Scalar}, hash),
          make_bgra_to_argb_be_case({"sse2", bgra_to_argbBE_sse2, IsaRequirement::Sse2}, hash),
          make_bgra_to_argb_be_case({"ssse3", bgra_to_argbBE_ssse3, IsaRequirement::Ssse3}, hash)};
}

class ToY416Kernels : public ::testing::TestWithParam<ToY416Case> {};

TEST_P(ToY416Kernels, MatchesScalarPackingAndPreservesInputs) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_to_y416_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, ToY416Kernels, ::testing::ValuesIn(to_y416_cases()),
                         [](const ::testing::TestParamInfo<ToY416Case>& info) {
                           return info.param.name;
                         });

class BgraToArgbBeKernels : public ::testing::TestWithParam<BgraToArgbBeCase> {};

TEST_P(BgraToArgbBeKernels, MatchesScalarByteSwapAndPreservesInputs) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_bgra_to_argb_be_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, BgraToArgbBeKernels, ::testing::ValuesIn(bgra_to_argb_be_cases()),
                         [](const ::testing::TestParamInfo<BgraToArgbBeCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
