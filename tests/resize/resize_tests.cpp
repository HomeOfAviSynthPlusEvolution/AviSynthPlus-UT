#include <gtest/gtest.h>

#include "resize_test_helpers.h"

#include "support/cpu_features.h"

#include <vector>

namespace avsut::test {
namespace {

std::vector<ResizeVertical8Case> resize_vertical8_cases() {
  return {
      make_resize_vertical8_case(
          32, 7, 5, 64, 64,
          Variant<ResizeFunction>{"sse2", resize_v_sse2_planar,
                                  IsaRequirement::Sse2},
          "6f9964479c013e1b"),
      make_resize_vertical8_case(
          32, 7, 5, 64, 64,
          Variant<ResizeFunction>{"avx2", resize_v_avx2_planar_uint8_t,
                                  IsaRequirement::Avx2},
          "6f9964479c013e1b"),
  };
}

std::vector<ResizeVertical16Case> resize_vertical16_cases() {
  return {
      make_resize_vertical16_case(
          10, 32, 7, 5, 128, 128,
          Variant<ResizeFunction>{
              "sse2", resize_v_sse2_planar_uint16_t<true>,
              IsaRequirement::Sse2},
          "f7e598ccec48b4d1"),
      make_resize_vertical16_case(
          10, 32, 7, 5, 128, 128,
          Variant<ResizeFunction>{
              "avx2", resize_v_avx2_planar_uint16_t<true>,
              IsaRequirement::Avx2},
          "f7e598ccec48b4d1"),
      make_resize_vertical16_case(
          16, 32, 7, 5, 128, 128,
          Variant<ResizeFunction>{
              "sse2", resize_v_sse2_planar_uint16_t<false>,
              IsaRequirement::Sse2},
          "b5418b1d30a8c90c"),
      make_resize_vertical16_case(
          16, 32, 7, 5, 128, 128,
          Variant<ResizeFunction>{
              "avx2", resize_v_avx2_planar_uint16_t<false>,
              IsaRequirement::Avx2},
          "b5418b1d30a8c90c"),
  };
}

std::vector<ResizeHorizontal8Case> resize_horizontal8_cases() {
  return {
      make_resize_horizontal8_case(
          48, 32, 5, 64, 64,
          Variant<ResizeFunction>{
              "ssse3", resizer_h_ssse3_generic_uint8_16<std::uint8_t, true>,
              IsaRequirement::Ssse3},
          "540645551755f4d6"),
      make_resize_horizontal8_case(
          48, 32, 5, 64, 64,
          Variant<ResizeFunction>{"avx2", resizer_h_avx2_generic_uint8_t,
                                  IsaRequirement::Avx2},
          "540645551755f4d6"),
  };
}

std::vector<ResizeHorizontal16Case> resize_horizontal16_cases() {
  return {
      make_resize_horizontal16_case(
          10, 48, 32, 5, 128, 128,
          Variant<ResizeFunction>{
              "ssse3", resizer_h_ssse3_generic_uint8_16<std::uint16_t, true>,
              IsaRequirement::Ssse3},
          "020442318cd5e6ee"),
      make_resize_horizontal16_case(
          10, 48, 32, 5, 128, 128,
          Variant<ResizeFunction>{
              "avx2", resizer_h_avx2_generic_uint16_t<true>,
              IsaRequirement::Avx2},
          "020442318cd5e6ee"),
      make_resize_horizontal16_case(
          16, 48, 32, 5, 128, 128,
          Variant<ResizeFunction>{
              "ssse3", resizer_h_ssse3_generic_uint8_16<std::uint16_t, false>,
              IsaRequirement::Ssse3},
          "fa53387644097a8d"),
      make_resize_horizontal16_case(
          16, 48, 32, 5, 128, 128,
          Variant<ResizeFunction>{
              "avx2", resizer_h_avx2_generic_uint16_t<false>,
              IsaRequirement::Avx2},
          "fa53387644097a8d"),
  };
}

class ResizeVertical8Kernels
    : public ::testing::TestWithParam<ResizeVertical8Case> {};

TEST_P(ResizeVertical8Kernels, MatchesFixedCoefficientReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_resize_vertical8_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    ResizeVertical8Kernels,
    ::testing::ValuesIn(resize_vertical8_cases()),
    [](const ::testing::TestParamInfo<ResizeVertical8Case>& info) {
      return info.param.name;
    });

class ResizeVertical16Kernels
    : public ::testing::TestWithParam<ResizeVertical16Case> {};

TEST_P(ResizeVertical16Kernels, MatchesFixedCoefficientReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_resize_vertical16_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    ResizeVertical16Kernels,
    ::testing::ValuesIn(resize_vertical16_cases()),
    [](const ::testing::TestParamInfo<ResizeVertical16Case>& info) {
      return info.param.name;
    });

class ResizeHorizontal8Kernels
    : public ::testing::TestWithParam<ResizeHorizontal8Case> {};

TEST_P(ResizeHorizontal8Kernels, MatchesFixedCoefficientReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_resize_horizontal8_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    ResizeHorizontal8Kernels,
    ::testing::ValuesIn(resize_horizontal8_cases()),
    [](const ::testing::TestParamInfo<ResizeHorizontal8Case>& info) {
      return info.param.name;
    });

class ResizeHorizontal16Kernels
    : public ::testing::TestWithParam<ResizeHorizontal16Case> {};

TEST_P(ResizeHorizontal16Kernels, MatchesFixedCoefficientReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_resize_horizontal16_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    ResizeHorizontal16Kernels,
    ::testing::ValuesIn(resize_horizontal16_cases()),
    [](const ::testing::TestParamInfo<ResizeHorizontal16Case>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace avsut::test
