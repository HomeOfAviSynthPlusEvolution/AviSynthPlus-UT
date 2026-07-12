#include <gtest/gtest.h>

#include "convert_yv24_rgb_test_helpers.h"

#include <vector>

namespace avsut::test {
namespace {

void add_packed_yv24_rgb_variants(std::vector<PackedYv24RgbCase>& cases,
                                  int pixel_step, bool has_alpha,
                                  std::size_t width, std::string expected_hash,
                                  bool include_avx2 = true) {
  constexpr std::size_t height = 3;
  constexpr std::size_t y_pitch = 64;
  constexpr std::size_t uv_pitch = 64;
  constexpr std::size_t alpha_pitch = 64;
  const std::size_t destination_pitch = pixel_step == 3 ? 128 : 192;
  cases.push_back(make_packed_yv24_rgb_case(
      pixel_step, has_alpha, width, height, y_pitch, uv_pitch, alpha_pitch,
      destination_pitch,
      Variant<PackedYv24RgbVariant>{"sse2", PackedYv24RgbVariant::Sse2,
                                    IsaRequirement::Sse2},
      expected_hash));
  cases.push_back(make_packed_yv24_rgb_case(
      pixel_step, has_alpha, width, height, y_pitch, uv_pitch, alpha_pitch,
      destination_pitch,
      Variant<PackedYv24RgbVariant>{"ssse3", PackedYv24RgbVariant::Ssse3,
                                    IsaRequirement::Ssse3},
      expected_hash));
  if (include_avx2) {
    cases.push_back(make_packed_yv24_rgb_case(
        pixel_step, has_alpha, width, height, y_pitch, uv_pitch, alpha_pitch,
        destination_pitch,
        Variant<PackedYv24RgbVariant>{"avx2", PackedYv24RgbVariant::Avx2,
                                      IsaRequirement::Avx2},
        std::move(expected_hash)));
  }
}

std::vector<PackedYv24RgbCase> packed_yv24_rgb_cases() {
  std::vector<PackedYv24RgbCase> cases;
  add_packed_yv24_rgb_variants(cases, 3, false, 35, "1a58022ce95cd6cd");
  add_packed_yv24_rgb_variants(cases, 4, false, 32, "8376395d6fa71ee6");
  add_packed_yv24_rgb_variants(cases, 4, true, 32, "456bf3a776f37d10");
  return cases;
}

class PackedYv24ToRgbKernels
    : public ::testing::TestWithParam<PackedYv24RgbCase> {};

TEST_P(PackedYv24ToRgbKernels,
       MatchesIndependentMatrixReferenceAcrossInstructionSets) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_packed_yv24_rgb_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(
    Kernels, PackedYv24ToRgbKernels,
    ::testing::ValuesIn(packed_yv24_rgb_cases()),
    [](const ::testing::TestParamInfo<PackedYv24RgbCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace avsut::test
