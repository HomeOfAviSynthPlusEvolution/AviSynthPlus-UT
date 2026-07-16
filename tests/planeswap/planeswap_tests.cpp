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
      make_yuy2_case(48, 5, 64, 64,
                     Variant<PlaneSwapFuncPtr>{"sse2", yuy2_swap_sse2, IsaRequirement::Sse2}, hash),
      make_yuy2_case(48, 5, 64, 64,
                     Variant<PlaneSwapFuncPtr>{"ssse3", yuy2_swap_ssse3, IsaRequirement::Ssse3},
                     hash),
      make_yuy2_case(
          80, 7, 96, 112,
          Variant<PlaneSwapFuncPtr>{"sse2", yuy2_swap_sse2, IsaRequirement::Sse2},
          "a2242d88e87142cc", 0xF30A5A01U),
      make_yuy2_case(
          80, 7, 96, 112,
          Variant<PlaneSwapFuncPtr>{"ssse3", yuy2_swap_ssse3, IsaRequirement::Ssse3},
          "a2242d88e87142cc", 0xF30A5A01U),
  };
}

std::vector<Yuy2UvToYCase> yuy2_uv_to_y_cases() {
  return {
      make_yuy2_uv_to_y_case(
          "Yuy2UvToY", true, 32, 5, 80, 64, 1,
          Variant<Yuy2UvToYFuncPtr>{"sse2", yuy2_uvtoy_sse2, IsaRequirement::Sse2},
          "c6e351ce228df1ae"),
      make_yuy2_uv_to_y_case(
          "Yuy2UvToY", true, 32, 5, 80, 64, 3,
          Variant<Yuy2UvToYFuncPtr>{"sse2", yuy2_uvtoy_sse2, IsaRequirement::Sse2},
          "83356ed499ef5dc7"),
      make_yuy2_uv_to_y_case(
          "Yuy2UvToY8", false, 32, 5, 160, 64, 1,
          Variant<Yuy2UvToYFuncPtr>{"sse2", yuy2_uvtoy8_sse2, IsaRequirement::Sse2},
          "5b73aaadb3308435"),
      make_yuy2_uv_to_y_case(
          "Yuy2UvToY8", false, 32, 5, 160, 64, 3,
          Variant<Yuy2UvToYFuncPtr>{"sse2", yuy2_uvtoy8_sse2, IsaRequirement::Sse2},
          "ce501d893cec226c"),
  };
}

std::vector<Yuy2ToUvCase> yuy2_to_uv_cases() {
  return {
      make_yuy2_to_uv_case(
          true, 64, 5, 96, 64, 96,
          Variant<Yuy2ToUvFuncPtr>{"sse2", yuy2_ytouv_sse2<true>, IsaRequirement::Sse2},
          "601073aa4c80ec7d"),
      make_yuy2_to_uv_case(
          false, 64, 5, 96, 64, 96,
          Variant<Yuy2ToUvFuncPtr>{"sse2", yuy2_ytouv_sse2<false>, IsaRequirement::Sse2},
          "e524cc0b56ef34d5"),
  };
}

template <int Channel>
void add_rgb32_variants(std::vector<RgbExtractCase>& cases, const char* hash,
                        std::size_t width_pixels = 48, std::size_t height = 5,
                        std::size_t source_pitch = 256, std::size_t destination_pitch = 64,
                        std::uint32_t seed = 0) {
  cases.push_back(
      make_rgb_case("Rgb32", Channel, width_pixels, height, source_pitch, destination_pitch, 1,
                    extract_packed_rgb32_channel_sse2<Channel>,
                    Variant<PlaneSwapFuncPtr>{"sse2", extract_packed_rgb32_channel_sse2<Channel>,
                                              IsaRequirement::Sse2},
                    hash, seed));
  cases.push_back(
      make_rgb_case("Rgb32", Channel, width_pixels, height, source_pitch, destination_pitch, 1,
                    extract_packed_rgb32_channel_sse2<Channel>,
                    Variant<PlaneSwapFuncPtr>{"avx2", extract_packed_rgb32_channel_avx2<Channel>,
                                              IsaRequirement::Avx2},
                    hash, seed));
}

template <int Channel>
void add_rgb64_variants(std::vector<RgbExtractCase>& cases, const char* hash,
                        std::size_t width_pixels = 24, std::size_t height = 5,
                        std::size_t source_pitch = 256, std::size_t destination_pitch = 64,
                        std::uint32_t seed = 0) {
  cases.push_back(
      make_rgb_case("Rgb64", Channel, width_pixels, height, source_pitch, destination_pitch, 2,
                    extract_packed_rgb64_channel_sse2<Channel>,
                    Variant<PlaneSwapFuncPtr>{"sse2", extract_packed_rgb64_channel_sse2<Channel>,
                                              IsaRequirement::Sse2},
                    hash, seed));
  cases.push_back(
      make_rgb_case("Rgb64", Channel, width_pixels, height, source_pitch, destination_pitch, 2,
                    extract_packed_rgb64_channel_sse2<Channel>,
                    Variant<PlaneSwapFuncPtr>{"avx2", extract_packed_rgb64_channel_avx2<Channel>,
                                              IsaRequirement::Avx2},
                    hash, seed));
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
  add_rgb32_variants<0>(cases, "5cbf32337ea80618", 48, 7, 224, 96, 0xF30A5A02U);
  add_rgb32_variants<1>(cases, "ee7299fe18dcebb0", 48, 7, 224, 96, 0xF30A5A02U);
  add_rgb32_variants<2>(cases, "34af9542d8d39af2", 48, 7, 224, 96, 0xF30A5A02U);
  add_rgb32_variants<3>(cases, "50bf31616868b922", 48, 7, 224, 96, 0xF30A5A02U);
  add_rgb64_variants<0>(cases, "e793c2d8482429fc", 24, 7, 224, 96, 0xF30A5A02U);
  add_rgb64_variants<1>(cases, "45bbc7ac2b974972", 24, 7, 224, 96, 0xF30A5A02U);
  add_rgb64_variants<2>(cases, "829a07e146fb7ade", 24, 7, 224, 96, 0xF30A5A02U);
  add_rgb64_variants<3>(cases, "c17913142b387d16", 24, 7, 224, 96, 0xF30A5A02U);
  return cases;
}

template <int Channel>
void add_rgb_noalpha_case(std::vector<RgbNoAlphaExtractCase>& cases, const char* format,
                          std::size_t width_pixels, std::size_t source_pitch,
                          std::size_t destination_pitch, std::size_t bytes_per_channel,
                          PlaneSwapFuncPtr function, const char* hash, std::size_t height = 5,
                          std::uint32_t seed = 0) {
  cases.push_back(make_rgb_noalpha_case(
      format, Channel, width_pixels, height, source_pitch, destination_pitch, bytes_per_channel,
      Variant<PlaneSwapFuncPtr>{"avx2", function, IsaRequirement::Avx2}, hash, seed));
}

std::vector<RgbNoAlphaExtractCase> rgb_noalpha_cases() {
  std::vector<RgbNoAlphaExtractCase> cases;
  add_rgb_noalpha_case<0>(cases, "Rgb24", 47, 192, 64, 1,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint8_t, 0>,
                          "a650ba0745647676");
  add_rgb_noalpha_case<1>(cases, "Rgb24", 47, 192, 64, 1,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint8_t, 1>,
                          "cd9d20bf80d378c7");
  add_rgb_noalpha_case<2>(cases, "Rgb24", 47, 192, 64, 1,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint8_t, 2>,
                          "035195bbbfc63848");
  add_rgb_noalpha_case<0>(cases, "Rgb48", 23, 192, 64, 2,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint16_t, 0>,
                          "69c3ecab4b70d217");
  add_rgb_noalpha_case<1>(cases, "Rgb48", 23, 192, 64, 2,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint16_t, 1>,
                          "96ce1a6ad4f30215");
  add_rgb_noalpha_case<2>(cases, "Rgb48", 23, 192, 64, 2,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint16_t, 2>,
                          "d22b81f3c24d3a57");
  add_rgb_noalpha_case<0>(cases, "Rgb24", 35, 160, 96, 1,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint8_t, 0>,
                          "36e2e03680f80fdc", 7, 0xF30A5A03U);
  add_rgb_noalpha_case<1>(cases, "Rgb24", 35, 160, 96, 1,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint8_t, 1>,
                          "19828922c8712cfe", 7, 0xF30A5A03U);
  add_rgb_noalpha_case<2>(cases, "Rgb24", 35, 160, 96, 1,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint8_t, 2>,
                          "75213d04cfeb0e1e", 7, 0xF30A5A03U);
  add_rgb_noalpha_case<0>(cases, "Rgb48", 19, 144, 96, 2,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint16_t, 0>,
                          "0625dffbc37144ad", 7, 0xF30A5A04U);
  add_rgb_noalpha_case<1>(cases, "Rgb48", 19, 144, 96, 2,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint16_t, 1>,
                          "cd52d5bbfb8575a4", 7, 0xF30A5A04U);
  add_rgb_noalpha_case<2>(cases, "Rgb48", 19, 144, 96, 2,
                          extract_packed_rgb_noalpha_channel_avx2<std::uint16_t, 2>,
                          "9953546f9cdb50b3", 7, 0xF30A5A04U);
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

INSTANTIATE_TEST_SUITE_P(Kernels, Yuy2SwapKernels, ::testing::ValuesIn(yuy2_cases()),
                         [](const ::testing::TestParamInfo<Yuy2SwapCase>& info) {
                           return info.param.name;
                         });

class Yuy2UvToYKernels : public ::testing::TestWithParam<Yuy2UvToYCase> {};

TEST_P(Yuy2UvToYKernels, ExtractsChromaWithIndependentNeutralChromaReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_yuy2_uv_to_y_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, Yuy2UvToYKernels, ::testing::ValuesIn(yuy2_uv_to_y_cases()),
                         [](const ::testing::TestParamInfo<Yuy2UvToYCase>& info) {
                           return info.param.name;
                         });

class Yuy2ToUvKernels : public ::testing::TestWithParam<Yuy2ToUvCase> {};

TEST_P(Yuy2ToUvKernels, InterleavesPlanarComponentsWithBothLumaModes) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_yuy2_to_uv_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, Yuy2ToUvKernels, ::testing::ValuesIn(yuy2_to_uv_cases()),
                         [](const ::testing::TestParamInfo<Yuy2ToUvCase>& info) {
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

INSTANTIATE_TEST_SUITE_P(Kernels, RgbExtractKernels, ::testing::ValuesIn(rgb_cases()),
                         [](const ::testing::TestParamInfo<RgbExtractCase>& info) {
                           return info.param.name;
                         });

class RgbNoAlphaExtractKernels : public ::testing::TestWithParam<RgbNoAlphaExtractCase> {};

TEST_P(RgbNoAlphaExtractKernels, ExtractsBottomUpPackedChannelWithOverlappingTail) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  if (test_case.bytes_per_channel == 1) {
    run_rgb_noalpha_case_typed<std::uint8_t>(test_case);
  } else {
    run_rgb_noalpha_case_typed<std::uint16_t>(test_case);
  }
}

INSTANTIATE_TEST_SUITE_P(Kernels, RgbNoAlphaExtractKernels,
                         ::testing::ValuesIn(rgb_noalpha_cases()),
                         [](const ::testing::TestParamInfo<RgbNoAlphaExtractCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
