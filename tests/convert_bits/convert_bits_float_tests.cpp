#include <gtest/gtest.h>

#include "convert_bits_float_test_helpers.h"

#include <cstdint>
#include <vector>

namespace avsut::test {
namespace {

std::vector<ConvertBitsFloatCase> convert_bits_float_cases() {
  std::vector<ConvertBitsFloatCase> cases;
  add_float_to_integer_variants<std::uint8_t, false, true, true>(
      cases, FloatConversion::FloatToUInt8, 8, "cf598ce5195e6d41");
  add_float_to_integer_variants<std::uint16_t, false, false, true>(
      cases, FloatConversion::FloatToUInt16, 10, "7594f94a03511f77");
  add_float_to_integer_variants<std::uint16_t, true, true, false>(
      cases, FloatConversion::FloatToUInt16, 10, "c186b634e598194e");
  add_float_to_integer_variants<std::uint16_t, true, false, true>(
      cases, FloatConversion::FloatToUInt16, 16, "8b3b6d4629122a90");
  add_integer_to_float_case<std::uint8_t, false, false, true>(cases, FloatConversion::UInt8ToFloat,
                                                              8, 64);
  add_integer_to_float_case<std::uint16_t, true, true, false>(cases, FloatConversion::UInt16ToFloat,
                                                              12, 96);
  return cases;
}

class ConvertBitsFloatKernels : public ::testing::TestWithParam<ConvertBitsFloatCase> {};

TEST_P(ConvertBitsFloatKernels, MatchesIndependentNumericalReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_convert_bits_float_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, ConvertBitsFloatKernels,
                         ::testing::ValuesIn(convert_bits_float_cases()),
                         [](const ::testing::TestParamInfo<ConvertBitsFloatCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
