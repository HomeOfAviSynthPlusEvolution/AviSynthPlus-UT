#include <gtest/gtest.h>

#include "convert_planar_matrix_test_helpers.h"

#include <array>
#include <vector>

namespace avsut::test {
namespace {

void add_planar_matrix_variants(std::vector<PlanarMatrixCase>& cases,
                                PlanarMatrixDirection direction, int matrix, bool source_full,
                                bool destination_full, std::array<std::string, 3> expected_hashes) {
  constexpr std::size_t width = 32;
  constexpr std::size_t height = 5;
  constexpr std::size_t source_pitch = 64;
  constexpr std::size_t destination_pitch = 64;
  cases.push_back(make_planar_matrix_case(
      direction, matrix, source_full, destination_full, width, height, source_pitch,
      destination_pitch,
      Variant<PlanarMatrixVariant>{"c", PlanarMatrixVariant::C, IsaRequirement::Scalar},
      expected_hashes));
  cases.push_back(make_planar_matrix_case(
      direction, matrix, source_full, destination_full, width, height, source_pitch,
      destination_pitch,
      Variant<PlanarMatrixVariant>{"sse2", PlanarMatrixVariant::Sse2, IsaRequirement::Sse2},
      expected_hashes));
  cases.push_back(make_planar_matrix_case(
      direction, matrix, source_full, destination_full, width, height, source_pitch,
      destination_pitch,
      Variant<PlanarMatrixVariant>{"avx2", PlanarMatrixVariant::Avx2, IsaRequirement::Avx2},
      std::move(expected_hashes)));
}

std::vector<PlanarMatrixCase> planar_matrix_cases() {
  std::vector<PlanarMatrixCase> cases;
  add_planar_matrix_variants(cases, PlanarMatrixDirection::YuvToRgb, AVS_MATRIX_BT709, false, true,
                             {"f325006d548ebb54", "facfe79dc019c4b8", "e4032aafc3eff0f3"});
  add_planar_matrix_variants(cases, PlanarMatrixDirection::RgbToYuv, AVS_MATRIX_BT470_BG, true,
                             false, {"31d45deab1a469cd", "5daee92036e3c06d", "3d34e7e301d4927a"});
  return cases;
}

class PlanarMatrixKernels : public ::testing::TestWithParam<PlanarMatrixCase> {};

TEST_P(PlanarMatrixKernels, MatchesIndependentMatrixReferenceAcrossInstructionSets) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_planar_matrix_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, PlanarMatrixKernels, ::testing::ValuesIn(planar_matrix_cases()),
                         [](const ::testing::TestParamInfo<PlanarMatrixCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
