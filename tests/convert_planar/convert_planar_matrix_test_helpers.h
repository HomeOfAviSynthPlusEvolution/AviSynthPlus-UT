#pragma once

#include "convert/convert_matrix.h"

// This header uses AVS_UNUSED after avisynth.h undefines it.
#ifndef AVS_UNUSED
#define AVSUT_DEFINED_AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#endif
#include "convert/convert_planar.h"
#ifdef AVSUT_DEFINED_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_DEFINED_AVS_UNUSED
#endif

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

enum class PlanarMatrixDirection { YuvToRgb, RgbToYuv };
enum class PlanarMatrixVariant { C, Sse2, Avx2 };

struct PlanarMatrixCase {
  PlanarMatrixDirection direction{};
  int matrix{};
  bool source_full{};
  bool destination_full{};
  std::size_t width{};
  std::size_t height{};
  std::size_t source_pitch{};
  std::size_t destination_pitch{};
  Variant<PlanarMatrixVariant> variant;
  std::array<std::string, 3> expected_hashes;
  std::string name;
};

inline void PrintTo(const PlanarMatrixCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

inline const char* planar_direction_name(PlanarMatrixDirection direction) {
  return direction == PlanarMatrixDirection::YuvToRgb ? "YuvToRgb"
                                                      : "RgbToYuv";
}

inline const char* planar_matrix_name(int matrix) {
  return matrix == AVS_MATRIX_BT709 ? "Bt709" : "Bt601";
}

inline std::string planar_matrix_variant_name(const std::string& name) {
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

inline PlanarMatrixCase make_planar_matrix_case(
    PlanarMatrixDirection direction, int matrix, bool source_full,
    bool destination_full, std::size_t width, std::size_t height,
    std::size_t source_pitch, std::size_t destination_pitch,
    Variant<PlanarMatrixVariant> variant,
    std::array<std::string, 3> expected_hashes) {
  PlanarMatrixCase result{direction,
                          matrix,
                          source_full,
                          destination_full,
                          width,
                          height,
                          source_pitch,
                          destination_pitch,
                          std::move(variant),
                          std::move(expected_hashes),
                          {}};
  std::ostringstream stream;
  stream << planar_direction_name(direction) << '_' << planar_matrix_name(matrix)
         << "_Src" << (source_full ? "Full" : "Limited") << "_Dst"
         << (destination_full ? "Full" : "Limited") << "_Width" << width
         << "_Height" << height << "_SrcPitch" << source_pitch
         << "_DstPitch" << destination_pitch
         << "_PatternChannelAnchors_"
         << planar_matrix_variant_name(result.variant.name);
  result.name = stream.str();
  return result;
}

struct PlanarMatrixCoefficients {
  int y_b{};
  int y_g{};
  int y_r{};
  int u_b{};
  int u_g{};
  int u_r{};
  int v_b{};
  int v_g{};
  int v_r{};
  int input_offset{};
  int output_offset{};
  int shift{};
};

inline int planar_round_symmetric(double value) {
  return static_cast<int>(value >= 0.0 ? value + 0.5 : value - 0.5);
}

inline void planar_kr_kb(int matrix, double& kr, double& kb) {
  if (matrix == AVS_MATRIX_BT709) {
    kr = 0.2126;
    kb = 0.0722;
    return;
  }
  kr = 0.299;
  kb = 0.114;
}

inline PlanarMatrixCoefficients make_planar_yuv_to_rgb_coefficients(
    int matrix, bool source_full, bool destination_full) {
  double kr{};
  double kb{};
  planar_kr_kb(matrix, kr, kb);
  const double kg = 1.0 - kr - kb;
  const double y_span = source_full ? 255.0 : 219.0;
  const double uv_span = source_full ? 127.5 : 112.0;
  const double rgb_span = destination_full ? 255.0 : 219.0;
  constexpr double scale = 8192.0;
  return {planar_round_symmetric(scale * rgb_span / y_span),
          planar_round_symmetric(scale * rgb_span / y_span),
          planar_round_symmetric(scale * rgb_span / y_span),
          planar_round_symmetric(scale * rgb_span * (1.0 - kb) / uv_span),
          planar_round_symmetric(scale * rgb_span * (kb - 1.0) * kb /
                                 (kg * uv_span)),
          0,
          0,
          planar_round_symmetric(scale * rgb_span * (kr - 1.0) * kr /
                                 (kg * uv_span)),
          planar_round_symmetric(scale * rgb_span * (1.0 - kr) / uv_span),
          source_full ? 0 : -16,
          destination_full ? 0 : 16,
          13};
}

inline PlanarMatrixCoefficients make_planar_rgb_to_yuv_coefficients(
    int matrix, bool source_full, bool destination_full) {
  double kr{};
  double kb{};
  planar_kr_kb(matrix, kr, kb);
  const double kg = 1.0 - kr - kb;
  const double rgb_span = source_full ? 255.0 : 219.0;
  const double y_span = destination_full ? 255.0 : 219.0;
  const double uv_span = destination_full ? 127.5 : 112.0;
  constexpr double scale = 32768.0;
  PlanarMatrixCoefficients result{
      planar_round_symmetric(scale * y_span * kb / rgb_span),
      planar_round_symmetric(scale * y_span * kg / rgb_span),
      planar_round_symmetric(scale * y_span * kr / rgb_span),
      planar_round_symmetric(scale * uv_span / rgb_span),
      planar_round_symmetric(scale * uv_span * kg / (kb - 1.0) / rgb_span),
      planar_round_symmetric(scale * uv_span * kr / (kb - 1.0) / rgb_span),
      planar_round_symmetric(scale * uv_span * kb / (kr - 1.0) / rgb_span),
      planar_round_symmetric(scale * uv_span * kg / (kr - 1.0) / rgb_span),
      planar_round_symmetric(scale * uv_span / rgb_span),
      source_full ? 0 : -16,
      destination_full ? 0 : 16,
      15};
  result.u_g -= result.u_b + result.u_g + result.u_r;
  result.v_g -= result.v_b + result.v_g + result.v_r;
  return result;
}

inline std::uint8_t planar_clip_byte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

inline std::uint8_t planar_shifted_component(int coefficient_a,
                                              int coefficient_b,
                                              int coefficient_c, int a,
                                              int b, int c, int offset,
                                              int shift) {
  const int total = coefficient_a * a + coefficient_b * b +
                    coefficient_c * c + (1 << (shift - 1)) +
                    (offset << shift);
  return planar_clip_byte(total >> shift);
}

inline void fill_planar_matrix_inputs(PlaneView<std::uint8_t> plane0,
                                      PlaneView<std::uint8_t> plane1,
                                      PlaneView<std::uint8_t> plane2) {
  constexpr std::array<std::uint8_t, 16> anchors{
      0, 1, 15, 16, 17, 63, 64, 96,
      127, 128, 192, 235, 239, 240, 254, 255};
  for (std::size_t row = 0; row < plane0.height(); ++row) {
    for (std::size_t column = 0; column < plane0.width(); ++column) {
      plane0.row(row)[column] = anchors[(column + row * 3) % anchors.size()];
      plane1.row(row)[column] =
          anchors[(column * 5 + row * 7 + 2) % anchors.size()];
      plane2.row(row)[column] =
          anchors[(column * 11 + row * 13 + 4) % anchors.size()];
    }
  }
}

inline void make_planar_matrix_reference(
    const PlanarMatrixCase& test_case, PlaneView<const std::uint8_t> source0,
    PlaneView<const std::uint8_t> source1,
    PlaneView<const std::uint8_t> source2, PlaneView<std::uint8_t> output0,
    PlaneView<std::uint8_t> output1, PlaneView<std::uint8_t> output2) {
  const auto coefficients =
      test_case.direction == PlanarMatrixDirection::YuvToRgb
          ? make_planar_yuv_to_rgb_coefficients(
                test_case.matrix, test_case.source_full,
                test_case.destination_full)
          : make_planar_rgb_to_yuv_coefficients(
                test_case.matrix, test_case.source_full,
                test_case.destination_full);
  for (std::size_t row = 0; row < source0.height(); ++row) {
    for (std::size_t column = 0; column < source0.width(); ++column) {
      const int source_a = source0.row(row)[column];
      const int source_b = source1.row(row)[column];
      const int source_c = source2.row(row)[column];
      if (test_case.direction == PlanarMatrixDirection::YuvToRgb) {
        const int y = source_a + coefficients.input_offset;
        const int u = source_b - 128;
        const int v = source_c - 128;
        output0.row(row)[column] = planar_shifted_component(
            coefficients.y_g, coefficients.u_g, coefficients.v_g, y, u, v,
            coefficients.output_offset, coefficients.shift);
        output1.row(row)[column] = planar_shifted_component(
            coefficients.y_b, coefficients.u_b, coefficients.v_b, y, u, v,
            coefficients.output_offset, coefficients.shift);
        output2.row(row)[column] = planar_shifted_component(
            coefficients.y_r, coefficients.u_r, coefficients.v_r, y, u, v,
            coefficients.output_offset, coefficients.shift);
      } else {
        const int g = source_a + coefficients.input_offset;
        const int b = source_b + coefficients.input_offset;
        const int r = source_c + coefficients.input_offset;
        output0.row(row)[column] = planar_shifted_component(
            coefficients.y_g, coefficients.y_b, coefficients.y_r, g, b, r,
            coefficients.output_offset, coefficients.shift);
        output1.row(row)[column] = planar_shifted_component(
            coefficients.u_g, coefficients.u_b, coefficients.u_r, g, b, r,
            128, coefficients.shift);
        output2.row(row)[column] = planar_shifted_component(
            coefficients.v_g, coefficients.v_b, coefficients.v_r, g, b, r,
            128, coefficients.shift);
      }
    }
  }
}

inline void call_planar_matrix_kernel(const PlanarMatrixCase& test_case,
                                      BYTE* (&destination)[3],
                                      int (&destination_pitch)[3],
                                      const BYTE* (&source)[3],
                                      const int (&source_pitch)[3],
                                      const ConversionMatrix& matrix) {
  const int width = static_cast<int>(test_case.width);
  const int height = static_cast<int>(test_case.height);
  if (test_case.direction == PlanarMatrixDirection::YuvToRgb) {
    if (test_case.variant.function == PlanarMatrixVariant::C) {
      convert_yuv_to_planarrgb_c<ConversionDirection::YUV_TO_RGB,
                                  std::uint8_t, true>(
          destination, destination_pitch, source, source_pitch, width, height,
          matrix, 8, 8, false);
    } else if (test_case.variant.function == PlanarMatrixVariant::Sse2) {
      convert_yuv_to_planarrgb_sse2<ConversionDirection::YUV_TO_RGB,
                                     std::uint8_t, true>(
          destination, destination_pitch, source, source_pitch, width, height,
          matrix, 8, 8, false);
    } else {
      convert_yuv_to_planarrgb_avx2<ConversionDirection::YUV_TO_RGB,
                                     std::uint8_t, true>(
          destination, destination_pitch, source, source_pitch, width, height,
          matrix, 8, 8, false);
    }
    return;
  }

  if (test_case.variant.function == PlanarMatrixVariant::C) {
    convert_yuv_to_planarrgb_c<ConversionDirection::RGB_TO_YUV,
                                std::uint8_t, true>(
        destination, destination_pitch, source, source_pitch, width, height,
        matrix, 8, 8, false);
  } else if (test_case.variant.function == PlanarMatrixVariant::Sse2) {
    convert_yuv_to_planarrgb_sse2<ConversionDirection::RGB_TO_YUV,
                                   std::uint8_t, true>(
        destination, destination_pitch, source, source_pitch, width, height,
        matrix, 8, 8, false);
  } else {
    convert_yuv_to_planarrgb_avx2<ConversionDirection::RGB_TO_YUV,
                                   std::uint8_t, true>(
        destination, destination_pitch, source, source_pitch, width, height,
        matrix, 8, 8, false);
  }
}

inline void run_planar_matrix_case(const PlanarMatrixCase& test_case) {
  GuardedVideoBuffer<std::uint8_t> source0(
      test_case.width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> source1(
      test_case.width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> source2(
      test_case.width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> expected0(
      test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> expected1(
      test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> expected2(
      test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> actual0(
      test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> actual1(
      test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<std::uint8_t> actual2(
      test_case.width, test_case.height, test_case.destination_pitch, 64);
  fill_planar_matrix_inputs(source0.view(), source1.view(), source2.view());
  const auto source0_snapshot = source0.snapshot_active();
  const auto source1_snapshot = source1.snapshot_active();
  const auto source2_snapshot = source2.snapshot_active();
  make_planar_matrix_reference(
      test_case, source0.view().as_const(), source1.view().as_const(),
      source2.view().as_const(), expected0.view(), expected1.view(),
      expected2.view());

  ConversionMatrix matrix{};
  const int source_range = test_case.source_full ? AVS_COLORRANGE_FULL
                                                  : AVS_COLORRANGE_LIMITED;
  const int destination_range = test_case.destination_full
                                    ? AVS_COLORRANGE_FULL
                                    : AVS_COLORRANGE_LIMITED;
  const bool matrix_built =
      test_case.direction == PlanarMatrixDirection::YuvToRgb
          ? do_BuildMatrix_Yuv2Rgb(test_case.matrix, source_range,
                                   destination_range, 13, 8, matrix)
          : do_BuildMatrix_Rgb2Yuv(test_case.matrix, source_range,
                                   destination_range, 15, 8, matrix);
  ASSERT_TRUE(matrix_built);

  BYTE* destination[3]{reinterpret_cast<BYTE*>(actual0.view().data()),
                       reinterpret_cast<BYTE*>(actual1.view().data()),
                       reinterpret_cast<BYTE*>(actual2.view().data())};
  int destination_pitch[3]{static_cast<int>(test_case.destination_pitch),
                           static_cast<int>(test_case.destination_pitch),
                           static_cast<int>(test_case.destination_pitch)};
  const BYTE* source[3]{reinterpret_cast<const BYTE*>(source0.view().data()),
                        reinterpret_cast<const BYTE*>(source1.view().data()),
                        reinterpret_cast<const BYTE*>(source2.view().data())};
  const int source_pitch[3]{static_cast<int>(test_case.source_pitch),
                            static_cast<int>(test_case.source_pitch),
                            static_cast<int>(test_case.source_pitch)};
  call_planar_matrix_kernel(test_case, destination, destination_pitch, source,
                            source_pitch, matrix);

  EXPECT_TRUE(compare_exact(expected0.view().as_const(), actual0.view().as_const()))
      << test_case.name << " output G/Y";
  EXPECT_TRUE(compare_exact(expected1.view().as_const(), actual1.view().as_const()))
      << test_case.name << " output B/U";
  EXPECT_TRUE(compare_exact(expected2.view().as_const(), actual2.view().as_const()))
      << test_case.name << " output R/V";
  EXPECT_EQ(format_hash(hash_active(actual0.view().as_const())),
            test_case.expected_hashes[0])
      << test_case.name << " output G/Y";
  EXPECT_EQ(format_hash(hash_active(actual1.view().as_const())),
            test_case.expected_hashes[1])
      << test_case.name << " output B/U";
  EXPECT_EQ(format_hash(hash_active(actual2.view().as_const())),
            test_case.expected_hashes[2])
      << test_case.name << " output R/V";
  EXPECT_TRUE(source0.active_matches(source0_snapshot)) << test_case.name;
  EXPECT_TRUE(source1.active_matches(source1_snapshot)) << test_case.name;
  EXPECT_TRUE(source2.active_matches(source2_snapshot)) << test_case.name;
  EXPECT_TRUE(source0.memory_intact()) << test_case.name;
  EXPECT_TRUE(source1.memory_intact()) << test_case.name;
  EXPECT_TRUE(source2.memory_intact()) << test_case.name;
  EXPECT_TRUE(expected0.memory_intact()) << test_case.name;
  EXPECT_TRUE(expected1.memory_intact()) << test_case.name;
  EXPECT_TRUE(expected2.memory_intact()) << test_case.name;
  EXPECT_TRUE(actual0.memory_intact()) << test_case.name;
  EXPECT_TRUE(actual1.memory_intact()) << test_case.name;
  EXPECT_TRUE(actual2.memory_intact()) << test_case.name;
}

}  // namespace avsut::test
