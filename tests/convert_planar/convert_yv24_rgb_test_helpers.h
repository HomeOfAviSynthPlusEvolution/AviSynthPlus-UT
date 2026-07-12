#pragma once

#include "convert/convert_matrix.h"
#include "convert/intel/convert_planar_avx2.h"
#include "convert/intel/convert_planar_sse.h"

#include "support/comparators.h"
#include "support/cpu_features.h"
#include "support/guarded_video_buffer.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace avsut::test {

enum class PackedYv24RgbVariant { Sse2, Ssse3, Avx2 };

struct PackedYv24RgbCase {
  int pixel_step{};
  bool has_alpha{};
  std::size_t width{};
  std::size_t height{};
  std::size_t y_pitch{};
  std::size_t uv_pitch{};
  std::size_t alpha_pitch{};
  std::size_t destination_pitch{};
  Variant<PackedYv24RgbVariant> variant;
  std::string expected_hash;
  std::string name;
};

inline void PrintTo(const PackedYv24RgbCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

inline std::string packed_yv24_rgb_variant_name(const std::string& name) {
  std::string result = "Variant";
  bool capitalize = true;
  for (const char character : name) {
    result.push_back(capitalize && character >= 'a' && character <= 'z'
                         ? static_cast<char>(character - ('a' - 'A'))
                         : character);
    capitalize = false;
  }
  return result;
}

inline PackedYv24RgbCase make_packed_yv24_rgb_case(
    int pixel_step, bool has_alpha, std::size_t width, std::size_t height, std::size_t y_pitch,
    std::size_t uv_pitch, std::size_t alpha_pitch, std::size_t destination_pitch,
    Variant<PackedYv24RgbVariant> variant, std::string expected_hash) {
  PackedYv24RgbCase result{pixel_step,
                           has_alpha,
                           width,
                           height,
                           y_pitch,
                           uv_pitch,
                           alpha_pitch,
                           destination_pitch,
                           std::move(variant),
                           std::move(expected_hash),
                           {}};
  std::ostringstream stream;
  stream << "Bt709_LimitedToFull_" << (pixel_step == 3 ? "Bgr24" : "Bgr32")
         << (has_alpha ? "_SourceAlpha" : "_OpaqueAlpha") << "_Width" << width << "_Height"
         << height << "_YPitch" << y_pitch << "_UvPitch" << uv_pitch << "_AlphaPitch" << alpha_pitch
         << "_DstPitch" << destination_pitch << "_PatternChannelAnchors_"
         << packed_yv24_rgb_variant_name(result.variant.name);
  result.name = stream.str();
  return result;
}

struct PackedYuvToRgbCoefficients {
  int y_b{};
  int u_b{};
  int v_b{};
  int y_g{};
  int u_g{};
  int v_g{};
  int y_r{};
  int u_r{};
  int v_r{};
  int offset_y{};
  int offset_rgb{};
};

inline int round_symmetric(double value) {
  return static_cast<int>(value >= 0.0 ? value + 0.5 : value - 0.5);
}

inline PackedYuvToRgbCoefficients make_bt709_limited_to_full_coefficients() {
  constexpr double kr = 0.2126;
  constexpr double kb = 0.0722;
  constexpr double kg = 1.0 - kr - kb;
  constexpr double y_span = 219.0;
  constexpr double uv_span = 112.0;
  constexpr double rgb_span = 255.0;
  constexpr double scale = 8192.0;

  return {round_symmetric(scale * rgb_span / y_span),
          round_symmetric(scale * rgb_span * (1.0 - kb) / uv_span),
          0,
          round_symmetric(scale * rgb_span / y_span),
          round_symmetric(scale * rgb_span * (kb - 1.0) * kb / (kg * uv_span)),
          round_symmetric(scale * rgb_span * (kr - 1.0) * kr / (kg * uv_span)),
          round_symmetric(scale * rgb_span / y_span),
          0,
          round_symmetric(scale * rgb_span * (1.0 - kr) / uv_span),
          -16,
          0};
}

inline std::uint8_t clip_byte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

inline std::uint8_t packed_yuv_to_rgb_reference_component(
    int y, int u, int v, int y_coefficient, int u_coefficient, int v_coefficient,
    const PackedYuvToRgbCoefficients& coefficients) {
  const int total = y_coefficient * (y + coefficients.offset_y) + u_coefficient * (u - 128) +
                    v_coefficient * (v - 128) + 4096 + (coefficients.offset_rgb << 13);
  return clip_byte(total >> 13);
}

inline void fill_yv24_channel_anchors(PlaneView<std::uint8_t> y_plane,
                                      PlaneView<std::uint8_t> u_plane,
                                      PlaneView<std::uint8_t> v_plane,
                                      PlaneView<std::uint8_t> alpha_plane) {
  constexpr std::array<std::uint8_t, 16> y_anchors{0,   15,  16,  17,  63,  64,  127, 128,
                                                   235, 236, 239, 240, 254, 255, 96,  192};
  constexpr std::array<std::uint8_t, 16> uv_anchors{0,   1,   15,  16,  17,  63,  64,  127,
                                                    128, 129, 192, 239, 240, 254, 255, 96};
  constexpr std::array<std::uint8_t, 8> alpha_anchors{0, 1, 31, 64, 127, 128, 192, 255};
  for (std::size_t row = 0; row < y_plane.height(); ++row) {
    for (std::size_t column = 0; column < y_plane.width(); ++column) {
      y_plane.row(row)[column] = y_anchors[(column + row * 3) % y_anchors.size()];
      u_plane.row(row)[column] = uv_anchors[(column * 5 + row * 7) % uv_anchors.size()];
      v_plane.row(row)[column] = uv_anchors[(column * 11 + row * 13 + 4) % uv_anchors.size()];
      alpha_plane.row(row)[column] = alpha_anchors[(column + row * 5) % alpha_anchors.size()];
    }
  }
}

inline void make_packed_yv24_rgb_reference(PlaneView<const std::uint8_t> y_plane,
                                           PlaneView<const std::uint8_t> u_plane,
                                           PlaneView<const std::uint8_t> v_plane,
                                           PlaneView<const std::uint8_t> alpha_plane,
                                           PlaneView<std::uint8_t> destination, int pixel_step,
                                           bool has_alpha) {
  const auto coefficients = make_bt709_limited_to_full_coefficients();
  for (std::size_t source_row = 0; source_row < y_plane.height(); ++source_row) {
    auto* destination_row = destination.row(destination.height() - 1 - source_row);
    for (std::size_t column = 0; column < y_plane.width(); ++column) {
      destination_row[column * pixel_step + 0] = packed_yuv_to_rgb_reference_component(
          y_plane.row(source_row)[column], u_plane.row(source_row)[column],
          v_plane.row(source_row)[column], coefficients.y_b, coefficients.u_b, coefficients.v_b,
          coefficients);
      destination_row[column * pixel_step + 1] = packed_yuv_to_rgb_reference_component(
          y_plane.row(source_row)[column], u_plane.row(source_row)[column],
          v_plane.row(source_row)[column], coefficients.y_g, coefficients.u_g, coefficients.v_g,
          coefficients);
      destination_row[column * pixel_step + 2] = packed_yuv_to_rgb_reference_component(
          y_plane.row(source_row)[column], u_plane.row(source_row)[column],
          v_plane.row(source_row)[column], coefficients.y_r, coefficients.u_r, coefficients.v_r,
          coefficients);
      if (pixel_step == 4) {
        destination_row[column * pixel_step + 3] =
            has_alpha ? alpha_plane.row(source_row)[column] : 255;
      }
    }
  }
}

inline void call_packed_yv24_rgb_kernel(const PackedYv24RgbCase& test_case, BYTE* destination,
                                        const BYTE* y_plane, const BYTE* u_plane,
                                        const BYTE* v_plane, const BYTE* alpha_plane,
                                        const ConversionMatrix& matrix) {
  const auto call = [&](auto function) {
    function(destination, y_plane, u_plane, v_plane, alpha_plane, test_case.destination_pitch,
             test_case.y_pitch, test_case.uv_pitch, test_case.alpha_pitch, test_case.width,
             test_case.height, matrix);
  };

  if (test_case.pixel_step == 3) {
    if (test_case.variant.function == PackedYv24RgbVariant::Sse2) {
      if (test_case.has_alpha) {
        call(convert_yv24_to_rgb_sse2<3, true>);
      } else {
        call(convert_yv24_to_rgb_sse2<3, false>);
      }
    } else if (test_case.variant.function == PackedYv24RgbVariant::Ssse3) {
      if (test_case.has_alpha) {
        call(convert_yv24_to_rgb_ssse3<3, true>);
      } else {
        call(convert_yv24_to_rgb_ssse3<3, false>);
      }
    } else if (test_case.has_alpha) {
      call(convert_yv24_to_rgb_avx2<3, true>);
    } else {
      call(convert_yv24_to_rgb_avx2<3, false>);
    }
    return;
  }

  if (test_case.variant.function == PackedYv24RgbVariant::Sse2) {
    if (test_case.has_alpha) {
      call(convert_yv24_to_rgb_sse2<4, true>);
    } else {
      call(convert_yv24_to_rgb_sse2<4, false>);
    }
  } else if (test_case.variant.function == PackedYv24RgbVariant::Ssse3) {
    if (test_case.has_alpha) {
      call(convert_yv24_to_rgb_ssse3<4, true>);
    } else {
      call(convert_yv24_to_rgb_ssse3<4, false>);
    }
  } else if (test_case.has_alpha) {
    call(convert_yv24_to_rgb_avx2<4, true>);
  } else {
    call(convert_yv24_to_rgb_avx2<4, false>);
  }
}

inline void run_packed_yv24_rgb_case(const PackedYv24RgbCase& test_case) {
  GuardedVideoBuffer<std::uint8_t> y_plane(test_case.width, test_case.height, test_case.y_pitch,
                                           64);
  GuardedVideoBuffer<std::uint8_t> u_plane(test_case.width, test_case.height, test_case.uv_pitch,
                                           64);
  GuardedVideoBuffer<std::uint8_t> v_plane(test_case.width, test_case.height, test_case.uv_pitch,
                                           64);
  GuardedVideoBuffer<std::uint8_t> alpha_plane(test_case.width, test_case.height,
                                               test_case.alpha_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> expected(
      test_case.width * static_cast<std::size_t>(test_case.pixel_step), test_case.height,
      test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> actual(
      test_case.width * static_cast<std::size_t>(test_case.pixel_step), test_case.height,
      test_case.destination_pitch, 64);
  fill_yv24_channel_anchors(y_plane.view(), u_plane.view(), v_plane.view(), alpha_plane.view());
  const auto y_snapshot = y_plane.snapshot_active();
  const auto u_snapshot = u_plane.snapshot_active();
  const auto v_snapshot = v_plane.snapshot_active();
  const auto alpha_snapshot = alpha_plane.snapshot_active();
  make_packed_yv24_rgb_reference(y_plane.view().as_const(), u_plane.view().as_const(),
                                 v_plane.view().as_const(), alpha_plane.view().as_const(),
                                 expected.view(), test_case.pixel_step, test_case.has_alpha);

  ConversionMatrix matrix{};
  ASSERT_TRUE(do_BuildMatrix_Yuv2Rgb(AVS_MATRIX_BT709, AVS_COLORRANGE_LIMITED, AVS_COLORRANGE_FULL,
                                     13, 8, matrix));
  call_packed_yv24_rgb_kernel(
      test_case, reinterpret_cast<BYTE*>(actual.view().data()),
      reinterpret_cast<const BYTE*>(y_plane.view().data()),
      reinterpret_cast<const BYTE*>(u_plane.view().data()),
      reinterpret_cast<const BYTE*>(v_plane.view().data()),
      test_case.has_alpha ? reinterpret_cast<const BYTE*>(alpha_plane.view().data()) : nullptr,
      matrix);

  EXPECT_TRUE(compare_exact(expected.view().as_const(), actual.view().as_const()))
      << test_case.name;
  EXPECT_EQ(format_hash(hash_active(actual.view().as_const())), test_case.expected_hash)
      << test_case.name;
  EXPECT_TRUE(y_plane.active_matches(y_snapshot)) << test_case.name;
  EXPECT_TRUE(u_plane.active_matches(u_snapshot)) << test_case.name;
  EXPECT_TRUE(v_plane.active_matches(v_snapshot)) << test_case.name;
  EXPECT_TRUE(alpha_plane.active_matches(alpha_snapshot)) << test_case.name;
  EXPECT_TRUE(y_plane.memory_intact()) << test_case.name;
  EXPECT_TRUE(u_plane.memory_intact()) << test_case.name;
  EXPECT_TRUE(v_plane.memory_intact()) << test_case.name;
  EXPECT_TRUE(alpha_plane.memory_intact()) << test_case.name;
  EXPECT_TRUE(expected.memory_intact()) << test_case.name;
  EXPECT_TRUE(actual.memory_intact()) << test_case.name;
}

}  // namespace avsut::test
