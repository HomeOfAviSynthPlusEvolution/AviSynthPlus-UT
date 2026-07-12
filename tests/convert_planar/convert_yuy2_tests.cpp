#include <gtest/gtest.h>

#include "convert_yuy2_test_helpers.h"

namespace avsut::test {
namespace {

TEST(ConvertYuy2ToY8, ExtractsLumaBytesFromPackedRows) {
  if (!CpuFeatures::detect().supports(IsaRequirement::Sse2)) {
    GTEST_SKIP() << "host does not support sse2";
  }
  run_yuy2_to_y8_case(make_yuy2_conversion_case(
      "Yuy2ToY8", 32, 5, 80, 48, 32, "a4d7587a8bc4d848"));
}

TEST(ConvertYuy2ToYv16, SplitsPackedChannelsIntoPlanarRows) {
  if (!CpuFeatures::detect().supports(IsaRequirement::Sse2)) {
    GTEST_SKIP() << "host does not support sse2";
  }
  run_yuy2_to_yv16_case(make_yuy2_conversion_case(
      "Yuy2ToYv16", 32, 5, 80, 48, 32, "a4d7587a8bc4d848",
      "cad35c647b9798e8", "52f89684a5e68efa"));
}

TEST(ConvertYv16ToYuy2, InterleavesPlanarChannelsIntoPackedRows) {
  if (!CpuFeatures::detect().supports(IsaRequirement::Sse2)) {
    GTEST_SKIP() << "host does not support sse2";
  }
  run_yv16_to_yuy2_case(make_yuy2_conversion_case(
      "Yv16ToYuy2", 32, 5, 80, 48, 32, {}, {}, {},
      "990a205340cb835c"));
}

}  // namespace
}  // namespace avsut::test
