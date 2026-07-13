#include <gtest/gtest.h>

#include "text_overlay_compare_test_helpers.h"

#include "support/cpu_features.h"

#include <cstdint>
#include <vector>

namespace avsut::test {
namespace {

std::vector<TextOverlayCompareCase> text_overlay_compare_cases() {
  return {
      make_text_overlay_compare_case(
          "Rgb32", "AllChannels", 0xffffffffU, 4, 32, 3, 52, 68, 3, 9, 5, -7, 11, -13, 17.0,
          Variant<TextOverlayCompareFunction>{"sse2", compare_sse2, IsaRequirement::Sse2}),
      make_text_overlay_compare_case(
          "Rgb32", "BlueAndRed", 0x00ff00ffU, 4, 48, 4, 64, 80, 5, 12, 0, 0, 0, 0, 0.0,
          Variant<TextOverlayCompareFunction>{"sse2", compare_sse2, IsaRequirement::Sse2}),
      make_text_overlay_compare_case(
          "Yuy2", "Luma", 0x00ff00ffU, 4, 32, 3, 48, 64, 7, 2, 19, -23, 3, -4, 9.0,
          Variant<TextOverlayCompareFunction>{"sse2", compare_sse2, IsaRequirement::Sse2}),
  };
}

class TextOverlayCompareKernels : public ::testing::TestWithParam<TextOverlayCompareCase> {};

TEST_P(TextOverlayCompareKernels, MatchesIndependentMetrics) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_text_overlay_compare_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, TextOverlayCompareKernels,
                         ::testing::ValuesIn(text_overlay_compare_cases()),
                         [](const ::testing::TestParamInfo<TextOverlayCompareCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
