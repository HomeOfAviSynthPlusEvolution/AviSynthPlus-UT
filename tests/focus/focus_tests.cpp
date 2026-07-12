#include <gtest/gtest.h>

#include "focus_test_helpers.h"

#include "support/cpu_features.h"

#include <vector>

namespace avsut::test {
namespace {

constexpr std::size_t kBlurAmount = 16384;
constexpr std::size_t kSharpenAmount = 49152;

std::vector<FocusHorizontal8Case> focus_horizontal8_cases() {
  std::vector<FocusHorizontal8Case> cases;
  for (const auto amount : {kBlurAmount, kSharpenAmount}) {
    cases.push_back(make_focus_horizontal8_case(
        37, 5, 64, amount,
        Variant<FocusHorizontal8FuncPtr>{"sse2", af_horizontal_planar_sse2,
                                          IsaRequirement::Sse2},
        amount == kBlurAmount ? "96f6319cb1fc053f" : "5ce7eeb81c783d33"));
    cases.push_back(make_focus_horizontal8_case(
        67, 5, 96, amount,
        Variant<FocusHorizontal8FuncPtr>{"avx2", af_horizontal_planar_avx2,
                                         IsaRequirement::Avx2},
        amount == kBlurAmount ? "aadf7fda96287d75" : "c68060bea1c036ea"));
  }
  return cases;
}

std::vector<FocusHorizontal16Case> focus_horizontal16_cases() {
  std::vector<FocusHorizontal16Case> cases;
  for (const auto amount : {kBlurAmount, kSharpenAmount}) {
    cases.push_back(make_focus_horizontal16_case(
        19, 5, 64, amount, 16,
        Variant<FocusHorizontal16FuncPtr>{
            "sse2", af_horizontal_planar_uint16_t_sse2, IsaRequirement::Sse2},
        amount == kBlurAmount ? "db533c589a76d12c" : "362aacc9ba8a05ff"));
    cases.push_back(make_focus_horizontal16_case(
        19, 5, 64, amount, 16,
        Variant<FocusHorizontal16FuncPtr>{
            "sse41", af_horizontal_planar_uint16_t_sse41,
            IsaRequirement::Sse41},
        amount == kBlurAmount ? "db533c589a76d12c" : "362aacc9ba8a05ff"));
    cases.push_back(make_focus_horizontal16_case(
        35, 5, 96, amount, 16,
        Variant<FocusHorizontal16FuncPtr>{
            "avx2", af_horizontal_planar_uint16_t_avx2, IsaRequirement::Avx2},
        amount == kBlurAmount ? "7b8dd1fffa3dfcc9" : "b48673d2a3f16b46"));
  }
  return cases;
}

std::vector<FocusVertical8Case> focus_vertical8_cases() {
  std::vector<FocusVertical8Case> cases;
  for (const auto amount : {kBlurAmount, kSharpenAmount}) {
    cases.push_back(make_focus_vertical8_case(
        64, 5, 64, amount,
        Variant<FocusVertical8FuncPtr>{"sse2", af_vertical_sse2,
                                       IsaRequirement::Sse2},
        amount == kBlurAmount ? "cc86e5ffb527ad5c" : "85c6e277c2156823"));
    cases.push_back(make_focus_vertical8_case(
        64, 5, 64, amount,
        Variant<FocusVertical8FuncPtr>{"avx2", af_vertical_avx2,
                                       IsaRequirement::Avx2},
        amount == kBlurAmount ? "cc86e5ffb527ad5c" : "85c6e277c2156823"));
  }
  return cases;
}

std::vector<FocusVertical16Case> focus_vertical16_cases() {
  std::vector<FocusVertical16Case> cases;
  for (const auto amount : {kBlurAmount, kSharpenAmount}) {
    cases.push_back(make_focus_vertical16_case(
        32, 5, 64, amount,
        Variant<FocusVertical16FuncPtr>{
            "sse2", af_vertical_uint16_t_sse2, IsaRequirement::Sse2},
        amount == kBlurAmount ? "34d727de54e3bf82" : "ac8dcc0705230d02"));
    cases.push_back(make_focus_vertical16_case(
        32, 5, 64, amount,
        Variant<FocusVertical16FuncPtr>{
            "sse41", af_vertical_uint16_t_sse41, IsaRequirement::Sse41},
        amount == kBlurAmount ? "34d727de54e3bf82" : "ac8dcc0705230d02"));
    cases.push_back(make_focus_vertical16_case(
        32, 5, 64, amount,
        Variant<FocusVertical16FuncPtr>{"avx2", af_vertical_uint16_t_avx2,
                                       IsaRequirement::Avx2},
        amount == kBlurAmount ? "34d727de54e3bf82" : "ac8dcc0705230d02"));
  }
  return cases;
}

class FocusHorizontal8Kernels
    : public ::testing::TestWithParam<FocusHorizontal8Case> {};

TEST_P(FocusHorizontal8Kernels, MatchesThreeTapReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_focus_horizontal8_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    FocusHorizontal8Kernels,
    ::testing::ValuesIn(focus_horizontal8_cases()),
    [](const ::testing::TestParamInfo<FocusHorizontal8Case>& info) {
      return info.param.name;
    });

class FocusHorizontal16Kernels
    : public ::testing::TestWithParam<FocusHorizontal16Case> {};

TEST_P(FocusHorizontal16Kernels, MatchesThreeTapReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_focus_horizontal16_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    FocusHorizontal16Kernels,
    ::testing::ValuesIn(focus_horizontal16_cases()),
    [](const ::testing::TestParamInfo<FocusHorizontal16Case>& info) {
      return info.param.name;
    });

class FocusVertical8Kernels
    : public ::testing::TestWithParam<FocusVertical8Case> {};

TEST_P(FocusVertical8Kernels, MatchesThreeTapReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_focus_vertical_case<std::uint8_t>(test_case, test_case.variant.function);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    FocusVertical8Kernels,
    ::testing::ValuesIn(focus_vertical8_cases()),
    [](const ::testing::TestParamInfo<FocusVertical8Case>& info) {
      return info.param.name;
    });

class FocusVertical16Kernels
    : public ::testing::TestWithParam<FocusVertical16Case> {};

TEST_P(FocusVertical16Kernels, MatchesThreeTapReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_focus_vertical_case<std::uint16_t>(test_case,
                                         test_case.variant.function);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    FocusVertical16Kernels,
    ::testing::ValuesIn(focus_vertical16_cases()),
    [](const ::testing::TestParamInfo<FocusVertical16Case>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace avsut::test
