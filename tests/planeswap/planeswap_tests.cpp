#include <gtest/gtest.h>

#include "planeswap_test_helpers.h"

#include "support/cpu_features.h"

#include <cstdint>
#include <vector>

namespace avsut::test {
namespace {

std::vector<Yuy2SwapCase> yuy2_cases() {
  constexpr auto hash = "aa4fa9f290b9d1f8";
  return {
      make_yuy2_case(
          48, 5, 64, 64,
          Variant<PlaneSwapFuncPtr>{"sse2", yuy2_swap_sse2, IsaRequirement::Sse2},
          hash),
      make_yuy2_case(
          48, 5, 64, 64,
          Variant<PlaneSwapFuncPtr>{"ssse3", yuy2_swap_ssse3, IsaRequirement::Ssse3},
          hash),
  };
}

template <int Channel>
void add_rgb32_variants(std::vector<RgbExtractCase>& cases, const char* hash) {
  cases.push_back(make_rgb_case(
      "Rgb32", Channel, 48, 5, 256, 64, 1,
      extract_packed_rgb32_channel_sse2<Channel>,
      Variant<PlaneSwapFuncPtr>{"sse2", extract_packed_rgb32_channel_sse2<Channel>,
                                IsaRequirement::Sse2},
      hash));
  cases.push_back(make_rgb_case(
      "Rgb32", Channel, 48, 5, 256, 64, 1,
      extract_packed_rgb32_channel_sse2<Channel>,
      Variant<PlaneSwapFuncPtr>{"avx2", extract_packed_rgb32_channel_avx2<Channel>,
                                IsaRequirement::Avx2},
      hash));
}

template <int Channel>
void add_rgb64_variants(std::vector<RgbExtractCase>& cases, const char* hash) {
  cases.push_back(make_rgb_case(
      "Rgb64", Channel, 24, 5, 256, 64, 2,
      extract_packed_rgb64_channel_sse2<Channel>,
      Variant<PlaneSwapFuncPtr>{"sse2", extract_packed_rgb64_channel_sse2<Channel>,
                                IsaRequirement::Sse2},
      hash));
  cases.push_back(make_rgb_case(
      "Rgb64", Channel, 24, 5, 256, 64, 2,
      extract_packed_rgb64_channel_sse2<Channel>,
      Variant<PlaneSwapFuncPtr>{"avx2", extract_packed_rgb64_channel_avx2<Channel>,
                                IsaRequirement::Avx2},
      hash));
}

std::vector<RgbExtractCase> rgb_cases() {
  std::vector<RgbExtractCase> cases;
  add_rgb32_variants<0>(cases, "83e072affe6ff2fa");
  add_rgb32_variants<1>(cases, "0bab6d79b4204b2e");
  add_rgb32_variants<2>(cases, "12f749ece346178c");
  add_rgb32_variants<3>(cases, "9eb04fcf1f5aa690");
  add_rgb64_variants<0>(cases, "4605dd1df18b267b");
  add_rgb64_variants<1>(cases, "cb3d6055cea48481");
  add_rgb64_variants<2>(cases, "1a89f7aa82a78924");
  add_rgb64_variants<3>(cases, "4f5ec488a42ad558");
  return cases;
}

class Yuy2SwapKernels : public ::testing::TestWithParam<Yuy2SwapCase> {};

TEST_P(Yuy2SwapKernels, SwapsChromaBytePositions) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_yuy2_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    Yuy2SwapKernels,
    ::testing::ValuesIn(yuy2_cases()),
    [](const ::testing::TestParamInfo<Yuy2SwapCase>& info) {
      return info.param.name;
    });

class RgbExtractKernels : public ::testing::TestWithParam<RgbExtractCase> {};

TEST_P(RgbExtractKernels, ExtractsBottomUpPackedChannel) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  if (test_case.bytes_per_channel == 1) {
    run_rgb_case_typed<std::uint8_t>(test_case);
  } else {
    run_rgb_case_typed<std::uint16_t>(test_case);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Kernels,
    RgbExtractKernels,
    ::testing::ValuesIn(rgb_cases()),
    [](const ::testing::TestParamInfo<RgbExtractCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace avsut::test
