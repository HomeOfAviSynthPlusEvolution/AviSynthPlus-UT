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
#include <iostream>
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
  int bit_depth{8};
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
  return direction == PlanarMatrixDirection::YuvToRgb ? "YuvToRgb" : "RgbToYuv";
}

inline const char* planar_matrix_name(int matrix) {
  switch (matrix) {
    case AVS_MATRIX_BT709:
      return "Bt709";
    case AVS_MATRIX_BT2020_NCL:
      return "Bt2020Ncl";
    case AVS_MATRIX_BT2020_CL:
      return "Bt2020Cl";
    case AVS_MATRIX_BT470_M:
      return "Bt470M";
    case AVS_MATRIX_ST240_M:
      return "St240M";
    default:
      return "Bt601Family";
  }
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
    PlanarMatrixDirection direction, int matrix, bool source_full, bool destination_full,
    std::size_t width, std::size_t height, std::size_t source_pitch, std::size_t destination_pitch,
    Variant<PlanarMatrixVariant> variant, std::array<std::string, 3> expected_hashes,
    int bit_depth = 8) {
  PlanarMatrixCase result{
      direction, matrix, source_full, destination_full, bit_depth, width, height, source_pitch,
      destination_pitch, std::move(variant), std::move(expected_hashes), {}};
  std::ostringstream stream;
  stream << planar_direction_name(direction) << '_' << planar_matrix_name(matrix) << "_Src"
         << (source_full ? "Full" : "Limited") << "_Dst" << (destination_full ? "Full" : "Limited")
         << "_BitDepth" << bit_depth << "_Width" << width << "_Height" << height << "_SrcPitch"
         << source_pitch << "_DstPitch" << destination_pitch << "_PatternChannelAnchors_"
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

struct PlanarRgbToYuvFloatCoefficients {
  float y_b{};
  float y_g{};
  float y_r{};
  float u_b{};
  float u_g{};
  float u_r{};
  float v_b{};
  float v_g{};
  float v_r{};
  float input_offset{};
  float output_offset{};
};

inline int planar_round_symmetric(double value) {
  return static_cast<int>(value >= 0.0 ? value + 0.5 : value - 0.5);
}

inline void planar_kr_kb(int matrix, double& kr, double& kb) {
  switch (matrix) {
    case AVS_MATRIX_BT709:
      kr = 0.2126;
      kb = 0.0722;
      return;
    case AVS_MATRIX_BT2020_NCL:
    case AVS_MATRIX_BT2020_CL:
      kr = 0.2627;
      kb = 0.0593;
      return;
    case AVS_MATRIX_BT470_M:
      kr = 0.3;
      kb = 0.11;
      return;
    case AVS_MATRIX_ST240_M:
      kr = 0.212;
      kb = 0.087;
      return;
    default:
      kr = 0.299;
      kb = 0.114;
      return;
  }
}

inline PlanarMatrixCoefficients make_planar_yuv_to_rgb_coefficients(int matrix, bool source_full,
                                                                    bool destination_full,
                                                                    int bit_depth) {
  double kr{};
  double kb{};
  planar_kr_kb(matrix, kr, kb);
  const double kg = 1.0 - kr - kb;
  const double sample_scale = static_cast<double>(std::uint32_t{1} << (bit_depth - 8));
  const double maximum = static_cast<double>((std::uint32_t{1} << bit_depth) - 1U);
  const double y_span = source_full ? maximum : 219.0 * sample_scale;
  const double uv_span = source_full ? maximum / 2.0 : 112.0 * sample_scale;
  const double rgb_span = destination_full ? maximum : 219.0 * sample_scale;
  constexpr double scale = 8192.0;
  return {planar_round_symmetric(scale * rgb_span / y_span),
          planar_round_symmetric(scale * rgb_span / y_span),
          planar_round_symmetric(scale * rgb_span / y_span),
          planar_round_symmetric(scale * rgb_span * (1.0 - kb) / uv_span),
          planar_round_symmetric(scale * rgb_span * (kb - 1.0) * kb / (kg * uv_span)),
          0,
          0,
          planar_round_symmetric(scale * rgb_span * (kr - 1.0) * kr / (kg * uv_span)),
          planar_round_symmetric(scale * rgb_span * (1.0 - kr) / uv_span),
          source_full ? 0 : -(1 << (bit_depth - 4)),
          destination_full ? 0 : (1 << (bit_depth - 4)),
          13};
}

inline PlanarMatrixCoefficients make_planar_rgb_to_yuv_coefficients(int matrix, bool source_full,
                                                                    bool destination_full,
                                                                    int bit_depth) {
  double kr{};
  double kb{};
  planar_kr_kb(matrix, kr, kb);
  const double kg = 1.0 - kr - kb;
  const double sample_scale = static_cast<double>(std::uint32_t{1} << (bit_depth - 8));
  const double maximum = static_cast<double>((std::uint32_t{1} << bit_depth) - 1U);
  const double rgb_span = source_full ? maximum : 219.0 * sample_scale;
  const double y_span = destination_full ? maximum : 219.0 * sample_scale;
  const double uv_span = destination_full ? maximum / 2.0 : 112.0 * sample_scale;
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
      source_full ? 0 : -(1 << (bit_depth - 4)),
      destination_full ? 0 : (1 << (bit_depth - 4)),
      15};
  result.u_g -= result.u_b + result.u_g + result.u_r;
  result.v_g -= result.v_b + result.v_g + result.v_r;
  return result;
}

inline PlanarRgbToYuvFloatCoefficients make_planar_rgb_to_yuv_float_coefficients(
    int matrix, bool source_full, bool destination_full, int bit_depth) {
  double kr{};
  double kb{};
  planar_kr_kb(matrix, kr, kb);
  const double kg = 1.0 - kr - kb;
  const double sample_scale = static_cast<double>(std::uint32_t{1} << (bit_depth - 8));
  const double maximum = static_cast<double>((std::uint32_t{1} << bit_depth) - 1U);
  const double rgb_span = source_full ? maximum : 219.0 * sample_scale;
  const double y_span = destination_full ? maximum : 219.0 * sample_scale;
  const double uv_span = destination_full ? maximum / 2.0 : 112.0 * sample_scale;
  return {
      static_cast<float>(y_span * kb / rgb_span),
      static_cast<float>(y_span * kg / rgb_span),
      static_cast<float>(y_span * kr / rgb_span),
      static_cast<float>(uv_span / rgb_span),
      static_cast<float>(uv_span * kg / (kb - 1.0) / rgb_span),
      static_cast<float>(uv_span * kr / (kb - 1.0) / rgb_span),
      static_cast<float>(uv_span * kb / (kr - 1.0) / rgb_span),
      static_cast<float>(uv_span * kg / (kr - 1.0) / rgb_span),
      static_cast<float>(uv_span / rgb_span),
      source_full ? 0.0F : -static_cast<float>(1 << (bit_depth - 4)),
      destination_full ? 0.0F : static_cast<float>(1 << (bit_depth - 4)),
  };
}

inline std::uint16_t planar_clip_sample(std::int64_t value, int bit_depth) {
  const auto maximum = (std::int64_t{1} << bit_depth) - 1;
  return static_cast<std::uint16_t>(std::clamp<std::int64_t>(value, 0, maximum));
}

inline std::uint16_t planar_round_float_sample(float value, int bit_depth) {
  const int rounded = static_cast<int>(value + 0.5F);
  const int maximum = (1 << bit_depth) - 1;
  return static_cast<std::uint16_t>(std::clamp(rounded, 0, maximum));
}

inline std::uint16_t planar_shifted_component(int coefficient_a, int coefficient_b,
                                              int coefficient_c, std::int64_t a, std::int64_t b,
                                              std::int64_t c, int offset, int shift, int bit_depth) {
  const std::int64_t total = static_cast<std::int64_t>(coefficient_a) * a +
                             static_cast<std::int64_t>(coefficient_b) * b +
                             static_cast<std::int64_t>(coefficient_c) * c +
                             (std::int64_t{1} << (shift - 1)) +
                             (static_cast<std::int64_t>(offset) << shift);
  return planar_clip_sample(total >> shift, bit_depth);
}

template <typename T>
inline void fill_planar_matrix_inputs(PlaneView<T> plane0, PlaneView<T> plane1,
                                       PlaneView<T> plane2, int bit_depth,
                                       bool yuv_to_rgb = false, bool source_full = true) {
  const auto scale = std::uint32_t{1} << (bit_depth - 8);
  const auto maximum = (std::uint32_t{1} << bit_depth) - 1U;
  if (yuv_to_rgb) {
    const auto y_low = source_full ? 0U : 16U * scale;
    const auto y_high = source_full ? maximum : 235U * scale;
    const auto uv_low = source_full ? 0U : 16U * scale;
    const auto uv_high = source_full ? maximum : 240U * scale;
    const auto uv_mid = std::uint32_t{128} * scale;
    const auto y_mid = source_full ? (maximum + 1U) / 2U : 128U * scale;
    for (std::size_t row = 0; row < plane0.height(); ++row) {
      for (std::size_t column = 0; column < plane0.width(); ++column) {
        const auto anchor = (column + row * 3) % 12;
        switch (anchor) {
          case 0:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_mid);
            plane2.row(row)[column] = static_cast<T>(uv_mid);
            break;
          case 1:
            plane0.row(row)[column] = static_cast<T>(y_low);
            plane1.row(row)[column] = static_cast<T>(uv_mid);
            plane2.row(row)[column] = static_cast<T>(uv_mid);
            break;
          case 2:
            plane0.row(row)[column] = static_cast<T>(y_high);
            plane1.row(row)[column] = static_cast<T>(uv_mid);
            plane2.row(row)[column] = static_cast<T>(uv_mid);
            break;
          case 3:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_low);
            plane2.row(row)[column] = static_cast<T>(uv_mid);
            break;
          case 4:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_high);
            plane2.row(row)[column] = static_cast<T>(uv_mid);
            break;
          case 5:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_mid);
            plane2.row(row)[column] = static_cast<T>(uv_low);
            break;
          case 6:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_mid);
            plane2.row(row)[column] = static_cast<T>(uv_high);
            break;
          case 7:
            plane0.row(row)[column] = static_cast<T>(y_high);
            plane1.row(row)[column] = static_cast<T>(uv_high);
            plane2.row(row)[column] = static_cast<T>(uv_high);
            break;
          case 8:
            plane0.row(row)[column] = static_cast<T>(y_low);
            plane1.row(row)[column] = static_cast<T>(uv_low);
            plane2.row(row)[column] = static_cast<T>(uv_low);
            break;
          case 9:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_low);
            plane2.row(row)[column] = static_cast<T>(uv_high);
            break;
          case 10:
            plane0.row(row)[column] = static_cast<T>(y_mid);
            plane1.row(row)[column] = static_cast<T>(uv_high);
            plane2.row(row)[column] = static_cast<T>(uv_low);
            break;
          default:
            plane0.row(row)[column] = static_cast<T>(64U * scale);
            plane1.row(row)[column] = static_cast<T>(96U * scale);
            plane2.row(row)[column] = static_cast<T>(192U * scale);
            break;
        }
      }
    }
    return;
  }
  const std::array<std::uint32_t, 16> anchors{
      0U,       1U,       15U * scale,  16U * scale, 17U * scale, 63U * scale,
      64U * scale, 96U * scale, 127U * scale, 128U * scale, 192U * scale,
      235U * scale, 239U * scale, 240U * scale, 254U * scale, maximum};
  for (std::size_t row = 0; row < plane0.height(); ++row) {
    for (std::size_t column = 0; column < plane0.width(); ++column) {
      plane0.row(row)[column] = static_cast<T>(anchors[(column + row * 3) % anchors.size()]);
      plane1.row(row)[column] = static_cast<T>(anchors[(column * 5 + row * 7 + 2) % anchors.size()]);
      plane2.row(row)[column] = static_cast<T>(anchors[(column * 11 + row * 13 + 4) % anchors.size()]);
    }
  }
}

template <typename T>
inline void make_planar_matrix_reference(const PlanarMatrixCase& test_case,
                                         PlaneView<const T> source0, PlaneView<const T> source1,
                                         PlaneView<const T> source2, PlaneView<T> output0,
                                         PlaneView<T> output1, PlaneView<T> output2) {
  // Exact 16-bit RGB->YUV uses the upstream float workflow to avoid int32 overflow;
  // keep its independent range and rounding semantics in the reference.
  if (test_case.direction == PlanarMatrixDirection::RgbToYuv && test_case.bit_depth == 16) {
    const auto coefficients = make_planar_rgb_to_yuv_float_coefficients(
        test_case.matrix, test_case.source_full, test_case.destination_full, test_case.bit_depth);
    const auto input_offset = static_cast<int>(coefficients.input_offset);
    const float half_pixel = static_cast<float>(std::int64_t{1} << (test_case.bit_depth - 1));
    for (std::size_t row = 0; row < source0.height(); ++row) {
      for (std::size_t column = 0; column < source0.width(); ++column) {
        const float g = static_cast<float>(static_cast<int>(source0.row(row)[column]) + input_offset);
        const float b = static_cast<float>(static_cast<int>(source1.row(row)[column]) + input_offset);
        const float r = static_cast<float>(static_cast<int>(source2.row(row)[column]) + input_offset);
        const float y = coefficients.y_g * g + coefficients.y_b * b + coefficients.y_r * r +
                        coefficients.output_offset;
        const float u = coefficients.u_g * g + coefficients.u_b * b + coefficients.u_r * r +
                        half_pixel;
        const float v = coefficients.v_g * g + coefficients.v_b * b + coefficients.v_r * r +
                        half_pixel;
        output0.row(row)[column] = static_cast<T>(planar_round_float_sample(y, test_case.bit_depth));
        output1.row(row)[column] = static_cast<T>(planar_round_float_sample(u, test_case.bit_depth));
        output2.row(row)[column] = static_cast<T>(planar_round_float_sample(v, test_case.bit_depth));
      }
    }
    return;
  }

  const auto coefficients =
      test_case.direction == PlanarMatrixDirection::YuvToRgb
          ? make_planar_yuv_to_rgb_coefficients(test_case.matrix, test_case.source_full,
                                                test_case.destination_full, test_case.bit_depth)
          : make_planar_rgb_to_yuv_coefficients(test_case.matrix, test_case.source_full,
                                                test_case.destination_full, test_case.bit_depth);
  auto adjusted_coefficients = coefficients;
  if (test_case.direction == PlanarMatrixDirection::RgbToYuv && test_case.source_full &&
      test_case.destination_full) {
    const auto luma_sum = adjusted_coefficients.y_b + adjusted_coefficients.y_g +
                          adjusted_coefficients.y_r;
    adjusted_coefficients.y_g += (1 << adjusted_coefficients.shift) - luma_sum;
  }
  const auto half_pixel = std::int64_t{1} << (test_case.bit_depth - 1);
  for (std::size_t row = 0; row < source0.height(); ++row) {
    for (std::size_t column = 0; column < source0.width(); ++column) {
      const int source_a = source0.row(row)[column];
      const int source_b = source1.row(row)[column];
      const int source_c = source2.row(row)[column];
      if (test_case.direction == PlanarMatrixDirection::YuvToRgb) {
        const auto y = static_cast<std::int64_t>(source_a) + adjusted_coefficients.input_offset;
        const auto u = static_cast<std::int64_t>(source_b) - half_pixel;
        const auto v = static_cast<std::int64_t>(source_c) - half_pixel;
        output0.row(row)[column] =
            planar_shifted_component(adjusted_coefficients.y_g, adjusted_coefficients.u_g,
                                     adjusted_coefficients.v_g, y, u, v,
                                     adjusted_coefficients.output_offset, adjusted_coefficients.shift,
                                     test_case.bit_depth);
        output1.row(row)[column] =
            planar_shifted_component(adjusted_coefficients.y_b, adjusted_coefficients.u_b,
                                     adjusted_coefficients.v_b, y, u, v,
                                     adjusted_coefficients.output_offset, adjusted_coefficients.shift,
                                     test_case.bit_depth);
        output2.row(row)[column] =
            planar_shifted_component(adjusted_coefficients.y_r, adjusted_coefficients.u_r,
                                     adjusted_coefficients.v_r, y, u, v,
                                     adjusted_coefficients.output_offset, adjusted_coefficients.shift,
                                     test_case.bit_depth);
      } else {
        const auto g = static_cast<std::int64_t>(source_a) + adjusted_coefficients.input_offset;
        const auto b = static_cast<std::int64_t>(source_b) + adjusted_coefficients.input_offset;
        const auto r = static_cast<std::int64_t>(source_c) + adjusted_coefficients.input_offset;
        output0.row(row)[column] =
            planar_shifted_component(adjusted_coefficients.y_g, adjusted_coefficients.y_b,
                                     adjusted_coefficients.y_r, g, b, r,
                                     adjusted_coefficients.output_offset, adjusted_coefficients.shift,
                                     test_case.bit_depth);
        output1.row(row)[column] = planar_shifted_component(
            adjusted_coefficients.u_g, adjusted_coefficients.u_b, adjusted_coefficients.u_r, g, b, r,
            static_cast<int>(half_pixel), adjusted_coefficients.shift, test_case.bit_depth);
        output2.row(row)[column] = planar_shifted_component(
            adjusted_coefficients.v_g, adjusted_coefficients.v_b, adjusted_coefficients.v_r, g, b, r,
            static_cast<int>(half_pixel), adjusted_coefficients.shift, test_case.bit_depth);
      }
    }
  }
}

template <typename T, bool LessThan16Bit>
inline void call_planar_matrix_kernel_typed(const PlanarMatrixCase& test_case,
                                            BYTE* (&destination)[3], int (&destination_pitch)[3],
                                            const BYTE* (&source)[3],
                                            const int (&source_pitch)[3],
                                            const ConversionMatrix& matrix) {
  const int width = static_cast<int>(test_case.width);
  const int height = static_cast<int>(test_case.height);
  if (test_case.direction == PlanarMatrixDirection::YuvToRgb) {
    if (test_case.variant.function == PlanarMatrixVariant::C) {
      convert_yuv_to_planarrgb_c<ConversionDirection::YUV_TO_RGB, T, LessThan16Bit>(
          destination, destination_pitch, source, source_pitch, width, height, matrix,
          test_case.bit_depth, test_case.bit_depth, false);
    } else if (test_case.variant.function == PlanarMatrixVariant::Sse2) {
      convert_yuv_to_planarrgb_sse2<ConversionDirection::YUV_TO_RGB, T, LessThan16Bit>(
          destination, destination_pitch, source, source_pitch, width, height, matrix,
          test_case.bit_depth, test_case.bit_depth, false);
    } else {
      convert_yuv_to_planarrgb_avx2<ConversionDirection::YUV_TO_RGB, T, LessThan16Bit>(
          destination, destination_pitch, source, source_pitch, width, height, matrix,
          test_case.bit_depth, test_case.bit_depth, false);
    }
    return;
  }

  if (test_case.variant.function == PlanarMatrixVariant::C) {
    convert_yuv_to_planarrgb_c<ConversionDirection::RGB_TO_YUV, T, LessThan16Bit>(
        destination, destination_pitch, source, source_pitch, width, height, matrix,
        test_case.bit_depth, test_case.bit_depth, false);
  } else if (test_case.variant.function == PlanarMatrixVariant::Sse2) {
    convert_yuv_to_planarrgb_sse2<ConversionDirection::RGB_TO_YUV, T, LessThan16Bit>(
        destination, destination_pitch, source, source_pitch, width, height, matrix,
        test_case.bit_depth, test_case.bit_depth, false);
  } else {
    convert_yuv_to_planarrgb_avx2<ConversionDirection::RGB_TO_YUV, T, LessThan16Bit>(
        destination, destination_pitch, source, source_pitch, width, height, matrix,
        test_case.bit_depth, test_case.bit_depth, false);
  }
}

template <typename T>
inline std::size_t planar_matrix_allowed_output_end(const PlanarMatrixCase& test_case) {
  const std::size_t active_bytes = test_case.width * sizeof(T);
  if (test_case.variant.function == PlanarMatrixVariant::C) {
    return active_bytes;
  }
  const std::size_t block_bytes =
      test_case.variant.function == PlanarMatrixVariant::Sse2
          ? 8 * sizeof(T)
          : (test_case.bit_depth == 32 ? 64 : 32 * sizeof(T));
  return ((active_bytes + block_bytes - 1) / block_bytes) * block_bytes;
}

template <typename T>
inline void expect_planar_matrix_output_memory(const PlanarMatrixCase& test_case,
                                               const GuardedVideoBuffer<T>& buffer,
                                               const char* plane_name) {
  EXPECT_TRUE(buffer.guards_intact()) << test_case.name << ' ' << plane_name << " guards";
  const auto allowed_end = planar_matrix_allowed_output_end<T>(test_case);
  const bool permitted_tail_write =
      test_case.variant.function != PlanarMatrixVariant::C && buffer.guards_intact() &&
      !buffer.padding_intact() && buffer.padding_intact_from(allowed_end);
  if (permitted_tail_write) {
    std::cout << "[INFO] " << test_case.name << " modified output padding within the permitted "
              << "SIMD tail (active_row_bytes=" << test_case.width * sizeof(T)
              << ", protected_from_byte=" << allowed_end << ")\n";
  }
  EXPECT_TRUE(buffer.padding_intact_from(allowed_end))
      << test_case.name << ' ' << plane_name
      << " modified output padding after the permitted SIMD tail boundary " << allowed_end;
}

template <typename T>
inline void run_planar_matrix_case_typed(const PlanarMatrixCase& test_case) {
  GuardedVideoBuffer<T> source0(test_case.width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<T> source1(test_case.width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<T> source2(test_case.width, test_case.height, test_case.source_pitch, 64);
  GuardedVideoBuffer<T> expected0(test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<T> expected1(test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<T> expected2(test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<T> actual0(test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<T> actual1(test_case.width, test_case.height, test_case.destination_pitch, 64);
  GuardedVideoBuffer<T> actual2(test_case.width, test_case.height, test_case.destination_pitch, 64);
  fill_planar_matrix_inputs(source0.view(), source1.view(), source2.view(), test_case.bit_depth,
                            test_case.direction == PlanarMatrixDirection::YuvToRgb,
                            test_case.source_full);
  const auto source0_snapshot = source0.snapshot_active();
  const auto source1_snapshot = source1.snapshot_active();
  const auto source2_snapshot = source2.snapshot_active();
  make_planar_matrix_reference(test_case, source0.view().as_const(), source1.view().as_const(),
                               source2.view().as_const(), expected0.view(), expected1.view(),
                               expected2.view());

  ConversionMatrix matrix{};
  const int source_range = test_case.source_full ? AVS_COLORRANGE_FULL : AVS_COLORRANGE_LIMITED;
  const int destination_range =
      test_case.destination_full ? AVS_COLORRANGE_FULL : AVS_COLORRANGE_LIMITED;
  const bool matrix_built =
      test_case.direction == PlanarMatrixDirection::YuvToRgb
          ? do_BuildMatrix_Yuv2Rgb(test_case.matrix, source_range, destination_range, 13,
                                   test_case.bit_depth, matrix)
          : do_BuildMatrix_Rgb2Yuv(test_case.matrix, source_range, destination_range, 15,
                                   test_case.bit_depth, matrix);
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
  if constexpr (sizeof(T) == 1) {
    call_planar_matrix_kernel_typed<T, true>(test_case, destination, destination_pitch, source,
                                             source_pitch, matrix);
  } else if (test_case.bit_depth < 16) {
    call_planar_matrix_kernel_typed<T, true>(test_case, destination, destination_pitch, source,
                                             source_pitch, matrix);
  } else {
    call_planar_matrix_kernel_typed<T, false>(test_case, destination, destination_pitch, source,
                                              source_pitch, matrix);
  }

  EXPECT_TRUE(compare_exact(expected0.view().as_const(), actual0.view().as_const()))
      << test_case.name << " output G/Y";
  EXPECT_TRUE(compare_exact(expected1.view().as_const(), actual1.view().as_const()))
      << test_case.name << " output B/U";
  EXPECT_TRUE(compare_exact(expected2.view().as_const(), actual2.view().as_const()))
      << test_case.name << " output R/V";
  EXPECT_EQ(format_hash(hash_active(actual0.view().as_const())), test_case.expected_hashes[0])
      << test_case.name << " output G/Y";
  EXPECT_EQ(format_hash(hash_active(actual1.view().as_const())), test_case.expected_hashes[1])
      << test_case.name << " output B/U";
  EXPECT_EQ(format_hash(hash_active(actual2.view().as_const())), test_case.expected_hashes[2])
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
  expect_planar_matrix_output_memory(test_case, actual0, "output G/Y");
  expect_planar_matrix_output_memory(test_case, actual1, "output B/U");
  expect_planar_matrix_output_memory(test_case, actual2, "output R/V");
}

inline void run_planar_matrix_case(const PlanarMatrixCase& test_case) {
  if (test_case.bit_depth == 8) {
    run_planar_matrix_case_typed<std::uint8_t>(test_case);
  } else {
    run_planar_matrix_case_typed<std::uint16_t>(test_case);
  }
}

}  // namespace avsut::test
