#include "convert/convert_helper.h"
#include "convert/convert_matrix.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace avsut::test {
namespace {

struct KrKbCase {
  const char* name;
  int matrix;
  double kr;
  double kb;
};

void PrintTo(const KrKbCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

constexpr std::array<KrKbCase, 8> kr_kb_cases{{
    {"Bt470Bg", AVS_MATRIX_BT470_BG, 0.299, 0.114},
    {"St170M", AVS_MATRIX_ST170_M, 0.299, 0.114},
    {"Bt709", AVS_MATRIX_BT709, 0.2126, 0.0722},
    {"Bt2020Ncl", AVS_MATRIX_BT2020_NCL, 0.2627, 0.0593},
    {"Bt2020Cl", AVS_MATRIX_BT2020_CL, 0.2627, 0.0593},
    {"Bt470M", AVS_MATRIX_BT470_M, 0.3, 0.11},
    {"St240M", AVS_MATRIX_ST240_M, 0.212, 0.087},
    {"Average", AVS_MATRIX_AVERAGE, 1.0 / 3.0, 1.0 / 3.0},
}};

class KrKbTable : public ::testing::TestWithParam<KrKbCase> {};

TEST_P(KrKbTable, ReturnsIndependentMatrixTableEntry) {
  const auto& test_case = GetParam();
  double kr = -1.0;
  double kb = -1.0;

  ASSERT_TRUE(GetKrKb(test_case.matrix, kr, kb)) << test_case.name;
  EXPECT_DOUBLE_EQ(kr, test_case.kr) << test_case.name;
  EXPECT_DOUBLE_EQ(kb, test_case.kb) << test_case.name;
}

INSTANTIATE_TEST_SUITE_P(
    MatrixIds,
    KrKbTable,
    ::testing::ValuesIn(kr_kb_cases),
    [](const ::testing::TestParamInfo<KrKbCase>& info) { return info.param.name; });

TEST(MatrixIds, RejectsIdentityAndUnsupportedMatrixIds) {
  constexpr std::array<int, 8> unsupported{
      AVS_MATRIX_RGB,
      AVS_MATRIX_UNSPECIFIED,
      AVS_MATRIX_YCGCO,
      AVS_MATRIX_ICTCP,
      AVS_MATRIX_CHROMATICITY_DERIVED_NCL,
      AVS_MATRIX_CHROMATICITY_DERIVED_CL,
      -1,
      12345,
  };

  for (const int matrix : unsupported) {
    double kr = 0.0;
    double kb = 0.0;
    EXPECT_FALSE(GetKrKb(matrix, kr, kb)) << "matrix=" << matrix;
  }
}

enum class MatrixDirection { RgbToYuv, YuvToRgb };

const char* direction_name(MatrixDirection direction) {
  return direction == MatrixDirection::RgbToYuv ? "RgbToYuv" : "YuvToRgb";
}

struct MatrixBuildCase {
  std::string name;
  int matrix;
  MatrixDirection direction;
  bool source_full;
  bool destination_full;
  int bit_depth;
};

void PrintTo(const MatrixBuildCase& test_case, std::ostream* stream) {
  *stream << test_case.name;
}

std::vector<MatrixBuildCase> matrix_build_cases() {
  std::vector<MatrixBuildCase> cases;
  constexpr std::array<bool, 2> ranges{false, true};
  for (const auto& matrix_case : kr_kb_cases) {
    for (const auto direction : {MatrixDirection::RgbToYuv, MatrixDirection::YuvToRgb}) {
      for (const bool source_full : ranges) {
        for (const bool destination_full : ranges) {
          cases.push_back(MatrixBuildCase{
              std::string(matrix_case.name) + "_" + direction_name(direction) + "_Src" +
                  (source_full ? "Full" : "Limited") + "_Dst" +
                  (destination_full ? "Full" : "Limited") + "_Bit8",
              matrix_case.matrix,
              direction,
              source_full,
              destination_full,
              8});
        }
      }
    }
  }
  for (const auto direction : {MatrixDirection::RgbToYuv, MatrixDirection::YuvToRgb}) {
    cases.push_back(MatrixBuildCase{
        std::string("RgbIdentity_") + direction_name(direction) + "_SrcFull_DstFull_Bit8",
        AVS_MATRIX_RGB,
        direction,
        true,
        true,
        8});
  }
  return cases;
}

struct RangeValues {
  double offset;
  double span;
};

RangeValues luma_range(bool full, int bit_depth) {
  const double scale = static_cast<double>(std::uint32_t{1} << (bit_depth - 8));
  return full ? RangeValues{0.0, static_cast<double>((std::uint32_t{1} << bit_depth) - 1U)}
              : RangeValues{16.0 * scale, 219.0 * scale};
}

RangeValues chroma_range(bool full, int bit_depth) {
  const double scale = static_cast<double>(std::uint32_t{1} << (bit_depth - 8));
  return full ? RangeValues{static_cast<double>(std::uint32_t{1} << (bit_depth - 1)),
                            static_cast<double>((std::uint32_t{1} << bit_depth) - 1U) / 2.0}
              : RangeValues{static_cast<double>(std::uint32_t{1} << (bit_depth - 1)),
                            112.0 * scale};
}

RangeValues float_luma_range(bool full) {
  return full ? RangeValues{0.0, 1.0} : RangeValues{16.0 / 255.0, 219.0 / 255.0};
}

int round_symmetric(double value) {
  return static_cast<int>(value >= 0.0 ? value + 0.5 : value - 0.5);
}

bool matrix_kr_kb(int matrix, double& kr, double& kb) {
  if (matrix == AVS_MATRIX_RGB) {
    kr = 0.0;
    kb = 0.0;
    return true;
  }
  for (const auto& test_case : kr_kb_cases) {
    if (matrix == test_case.matrix) {
      kr = test_case.kr;
      kb = test_case.kb;
      return true;
    }
  }
  return false;
}

struct ExpectedMatrix {
  int y_r{};
  int y_g{};
  int y_b{};
  int u_r{};
  int u_g{};
  int u_b{};
  int v_r{};
  int v_g{};
  int v_b{};
  float y_r_f{};
  float y_g_f{};
  float y_b_f{};
  float u_r_f{};
  float u_g_f{};
  float u_b_f{};
  float v_r_f{};
  float v_g_f{};
  float v_b_f{};
  int offset_y{};
  float offset_y_f{};
  int offset_rgb{};
  float offset_rgb_f{};
  int offset_in{};
  float offset_in_f{};
  int offset_out{};
  float offset_out_f{};
  float target_span_f{};
  float target_span_f_32{};
  float offset_out_f_32{};
};

ExpectedMatrix expected_matrix(const MatrixBuildCase& test_case) {
  ExpectedMatrix expected{};
  double kr = 0.0;
  double kb = 0.0;
  EXPECT_TRUE(matrix_kr_kb(test_case.matrix, kr, kb));
  const double kg = 1.0 - kr - kb;
  const auto source_luma = luma_range(test_case.source_full, test_case.bit_depth);
  const auto destination_luma = luma_range(test_case.destination_full, test_case.bit_depth);
  const auto source_chroma = chroma_range(test_case.source_full, test_case.bit_depth);
  const auto destination_chroma = chroma_range(test_case.destination_full, test_case.bit_depth);
  const auto destination_luma_32 = float_luma_range(test_case.destination_full);
  const int shift = test_case.direction == MatrixDirection::RgbToYuv ? 15 : 13;
  const double scale = static_cast<double>(1 << shift);

  if (test_case.direction == MatrixDirection::RgbToYuv) {
    const double y_b = destination_luma.span * kb / source_luma.span;
    const double y_g = destination_luma.span * kg / source_luma.span;
    const double y_r = destination_luma.span * kr / source_luma.span;
    const double u_b = destination_chroma.span / source_luma.span;
    const double u_g = destination_chroma.span * kg / (kb - 1.0) / source_luma.span;
    const double u_r = destination_chroma.span * kr / (kb - 1.0) / source_luma.span;
    const double v_b = destination_chroma.span * kb / (kr - 1.0) / source_luma.span;
    const double v_g = destination_chroma.span * kg / (kr - 1.0) / source_luma.span;
    const double v_r = destination_chroma.span / source_luma.span;

    expected.y_b_f = static_cast<float>(y_b);
    expected.y_g_f = static_cast<float>(y_g);
    expected.y_r_f = static_cast<float>(y_r);
    expected.u_b_f = static_cast<float>(u_b);
    expected.u_g_f = static_cast<float>(u_g);
    expected.u_r_f = static_cast<float>(u_r);
    expected.v_b_f = static_cast<float>(v_b);
    expected.v_g_f = static_cast<float>(v_g);
    expected.v_r_f = static_cast<float>(v_r);
    expected.offset_y_f = static_cast<float>(destination_luma.offset);
    expected.offset_rgb_f = static_cast<float>(-source_luma.offset);

    expected.y_b = round_symmetric(scale * y_b);
    expected.y_g = round_symmetric(scale * y_g);
    expected.y_r = round_symmetric(scale * y_r);
    expected.u_b = round_symmetric(scale * u_b);
    expected.u_g = round_symmetric(scale * u_g);
    expected.u_r = round_symmetric(scale * u_r);
    expected.v_b = round_symmetric(scale * v_b);
    expected.v_g = round_symmetric(scale * v_g);
    expected.v_r = round_symmetric(scale * v_r);
    if (test_case.source_full && test_case.destination_full) {
      expected.y_g += static_cast<int>(scale) - expected.y_b - expected.y_g - expected.y_r;
    }
    expected.u_g -= expected.u_b + expected.u_g + expected.u_r;
    expected.v_g -= expected.v_b + expected.v_g + expected.v_r;
    expected.offset_y = static_cast<int>(destination_luma.offset);
    expected.offset_rgb = -static_cast<int>(source_luma.offset);
    expected.offset_in = expected.offset_rgb;
    expected.offset_in_f = expected.offset_rgb_f;
    expected.offset_out = expected.offset_y;
    expected.offset_out_f = expected.offset_y_f;
  } else {
    const double y = destination_luma.span / source_luma.span;
    const double u_b = destination_luma.span * (1.0 - kb) / source_chroma.span;
    const double u_g = destination_luma.span * (kb - 1.0) * kb / kg / source_chroma.span;
    const double u_r = 0.0;
    const double v_b = 0.0;
    const double v_g = destination_luma.span * (kr - 1.0) * kr / kg / source_chroma.span;
    const double v_r = destination_luma.span * (1.0 - kr) / source_chroma.span;

    expected.y_b_f = static_cast<float>(y);
    expected.y_g_f = static_cast<float>(y);
    expected.y_r_f = static_cast<float>(y);
    expected.u_b_f = static_cast<float>(u_b);
    expected.u_g_f = static_cast<float>(u_g);
    expected.u_r_f = static_cast<float>(u_r);
    expected.v_b_f = static_cast<float>(v_b);
    expected.v_g_f = static_cast<float>(v_g);
    expected.v_r_f = static_cast<float>(v_r);
    expected.offset_y_f = static_cast<float>(-source_luma.offset);
    expected.offset_rgb_f = static_cast<float>(destination_luma.offset);

    expected.y_b = round_symmetric(scale * y);
    expected.y_g = expected.y_b;
    expected.y_r = expected.y_b;
    expected.u_b = round_symmetric(scale * u_b);
    expected.u_g = round_symmetric(scale * u_g);
    expected.u_r = round_symmetric(scale * u_r);
    expected.v_b = round_symmetric(scale * v_b);
    expected.v_g = round_symmetric(scale * v_g);
    expected.v_r = round_symmetric(scale * v_r);
    expected.offset_y = round_symmetric(-source_luma.offset);
    expected.offset_rgb = round_symmetric(destination_luma.offset);
    expected.offset_in = expected.offset_y;
    expected.offset_in_f = expected.offset_y_f;
    expected.offset_out = expected.offset_rgb;
    expected.offset_out_f = expected.offset_rgb_f;
    expected.target_span_f = static_cast<float>(destination_luma.span);
  }

  expected.target_span_f_32 = static_cast<float>(destination_luma_32.span);
  expected.offset_out_f_32 = static_cast<float>(destination_luma_32.offset);
  return expected;
}

void expect_matrix_matches(const ConversionMatrix& actual, const ExpectedMatrix& expected,
                           const std::string& name) {
  EXPECT_EQ(actual.y_r, expected.y_r) << name;
  EXPECT_EQ(actual.y_g, expected.y_g) << name;
  EXPECT_EQ(actual.y_b, expected.y_b) << name;
  EXPECT_EQ(actual.u_r, expected.u_r) << name;
  EXPECT_EQ(actual.u_g, expected.u_g) << name;
  EXPECT_EQ(actual.u_b, expected.u_b) << name;
  EXPECT_EQ(actual.v_r, expected.v_r) << name;
  EXPECT_EQ(actual.v_g, expected.v_g) << name;
  EXPECT_EQ(actual.v_b, expected.v_b) << name;
  EXPECT_NEAR(actual.y_r_f, expected.y_r_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.y_g_f, expected.y_g_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.y_b_f, expected.y_b_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.u_r_f, expected.u_r_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.u_g_f, expected.u_g_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.u_b_f, expected.u_b_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.v_r_f, expected.v_r_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.v_g_f, expected.v_g_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.v_b_f, expected.v_b_f, 1.0e-6F) << name;
  EXPECT_EQ(actual.offset_y, expected.offset_y) << name;
  EXPECT_NEAR(actual.offset_y_f, expected.offset_y_f, 1.0e-6F) << name;
  EXPECT_EQ(actual.offset_rgb, expected.offset_rgb) << name;
  EXPECT_NEAR(actual.offset_rgb_f, expected.offset_rgb_f, 1.0e-6F) << name;
  EXPECT_EQ(actual.offset_in, expected.offset_in) << name;
  EXPECT_NEAR(actual.offset_in_f, expected.offset_in_f, 1.0e-6F) << name;
  EXPECT_EQ(actual.offset_out, expected.offset_out) << name;
  EXPECT_NEAR(actual.offset_out_f, expected.offset_out_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.target_span_f, expected.target_span_f, 1.0e-6F) << name;
  EXPECT_NEAR(actual.target_span_f_32, expected.target_span_f_32, 1.0e-6F) << name;
  EXPECT_NEAR(actual.offset_out_f_32, expected.offset_out_f_32, 1.0e-6F) << name;
}

class MatrixBuilder : public ::testing::TestWithParam<MatrixBuildCase> {};

TEST_P(MatrixBuilder, AcceptsSupportedIdsAndInitializesMatrixFields) {
  const auto& test_case = GetParam();
  const int source_range = test_case.source_full ? AVS_COLORRANGE_FULL : AVS_COLORRANGE_LIMITED;
  const int destination_range =
      test_case.destination_full ? AVS_COLORRANGE_FULL : AVS_COLORRANGE_LIMITED;
  ConversionMatrix actual{};
  const bool built = test_case.direction == MatrixDirection::RgbToYuv
                         ? do_BuildMatrix_Rgb2Yuv(test_case.matrix, source_range, destination_range,
                                                  15, test_case.bit_depth, actual)
                         : do_BuildMatrix_Yuv2Rgb(test_case.matrix, source_range, destination_range,
                                                  13, test_case.bit_depth, actual);

  ASSERT_TRUE(built) << test_case.name;
  expect_matrix_matches(actual, expected_matrix(test_case), test_case.name);
}

INSTANTIATE_TEST_SUITE_P(
    Supported,
    MatrixBuilder,
    ::testing::ValuesIn(matrix_build_cases()),
    [](const ::testing::TestParamInfo<MatrixBuildCase>& info) { return info.param.name; });

TEST(MatrixBuilder, RejectsUnsupportedIdsAndInvalidRanges) {
  constexpr std::array<int, 6> unsupported{
      AVS_MATRIX_UNSPECIFIED,
      AVS_MATRIX_YCGCO,
      AVS_MATRIX_ICTCP,
      AVS_MATRIX_CHROMATICITY_DERIVED_NCL,
      AVS_MATRIX_CHROMATICITY_DERIVED_CL,
      12345,
  };
  constexpr std::array<MatrixDirection, 2> directions{MatrixDirection::RgbToYuv,
                                                       MatrixDirection::YuvToRgb};
  for (const auto direction : directions) {
    for (const int matrix : unsupported) {
      ConversionMatrix actual{};
      const bool built = direction == MatrixDirection::RgbToYuv
                             ? do_BuildMatrix_Rgb2Yuv(matrix, AVS_COLORRANGE_FULL,
                                                      AVS_COLORRANGE_LIMITED, 15, 8, actual)
                             : do_BuildMatrix_Yuv2Rgb(matrix, AVS_COLORRANGE_FULL,
                                                      AVS_COLORRANGE_LIMITED, 13, 8, actual);
      EXPECT_FALSE(built) << "direction=" << direction_name(direction) << " matrix=" << matrix;
    }

    for (const int invalid_source : {-1, 2, 99}) {
      ConversionMatrix actual{};
      const bool built = direction == MatrixDirection::RgbToYuv
                             ? do_BuildMatrix_Rgb2Yuv(AVS_MATRIX_BT709, invalid_source,
                                                      AVS_COLORRANGE_FULL, 15, 8, actual)
                             : do_BuildMatrix_Yuv2Rgb(AVS_MATRIX_BT709, invalid_source,
                                                      AVS_COLORRANGE_FULL, 13, 8, actual);
      EXPECT_FALSE(built) << "direction=" << direction_name(direction)
                          << " invalid_source=" << invalid_source;
    }

    for (const int invalid_destination : {-1, 2, 99}) {
      ConversionMatrix actual{};
      const bool built = direction == MatrixDirection::RgbToYuv
                             ? do_BuildMatrix_Rgb2Yuv(AVS_MATRIX_BT709, AVS_COLORRANGE_LIMITED,
                                                      invalid_destination, 15, 8, actual)
                             : do_BuildMatrix_Yuv2Rgb(AVS_MATRIX_BT709, AVS_COLORRANGE_LIMITED,
                                                      invalid_destination, 13, 8, actual);
      EXPECT_FALSE(built) << "direction=" << direction_name(direction)
                          << " invalid_destination=" << invalid_destination;
    }
  }
}

}  // namespace
}  // namespace avsut::test
