#include <gtest/gtest.h>

#include "overlay_lighten_darken_test_helpers.h"

#include "support/cpu_features.h"

#include <array>
#include <string>
#include <vector>

namespace avsut::test {
namespace {

std::array<std::string, 3> overlay_darklighten_expected_hashes(
    OverlayDarkLightenOperation operation) {
  if (operation == OverlayDarkLightenOperation::Darken) {
    return {"e9481bbe97308fc6", "8ec350993c640c07", "febac0d279640497"};
  }
  return {"86dae5f4278cf537", "1582b61169e4bb35", "4bc39e90e21f211b"};
}

std::vector<OverlayDarkLightenCase> overlay_darklighten_cases() {
  constexpr std::size_t width = 23;
  constexpr std::size_t height = 4;
  constexpr std::size_t destination_pitch = 40;
  constexpr std::size_t overlay_pitch = 56;
  std::vector<OverlayDarkLightenCase> cases;
  for (const auto operation :
       {OverlayDarkLightenOperation::Darken, OverlayDarkLightenOperation::Lighten}) {
    const auto expected_hashes = overlay_darklighten_expected_hashes(operation);
    cases.push_back(make_overlay_darklighten_case(
        operation, width, height, destination_pitch, overlay_pitch, 4, 8,
        Variant<OverlayDarkLightenFunction>{"sse2",
                                            operation == OverlayDarkLightenOperation::Darken
                                                ? overlay_darken_sse2
                                                : overlay_lighten_sse2,
                                            IsaRequirement::Sse2},
        expected_hashes));
    cases.push_back(make_overlay_darklighten_case(
        operation, width, height, destination_pitch, overlay_pitch, 4, 8,
        Variant<OverlayDarkLightenFunction>{"sse4.1",
                                            operation == OverlayDarkLightenOperation::Darken
                                                ? overlay_darken_sse41
                                                : overlay_lighten_sse41,
                                            IsaRequirement::Sse41},
        expected_hashes));
  }
  return cases;
}

class OverlayDarkLightenKernels : public ::testing::TestWithParam<OverlayDarkLightenCase> {};

TEST_P(OverlayDarkLightenKernels, MatchesIndependentYDrivenReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_overlay_darklighten_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, OverlayDarkLightenKernels,
                         ::testing::ValuesIn(overlay_darklighten_cases()),
                         [](const ::testing::TestParamInfo<OverlayDarkLightenCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
