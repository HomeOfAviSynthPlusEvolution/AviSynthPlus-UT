#include <gtest/gtest.h>

#include "layer_test_helpers.h"

#include "support/cpu_features.h"

#include <string>
#include <vector>

namespace avsut::test {
namespace {

std::string layer_expected_hash(MaskMode mode, int bits_per_pixel, bool full_opacity) {
  if (bits_per_pixel == 8) {
    if (full_opacity) {
      switch (mode) {
        case MASK444:
          return "14dbbcc50651f5bb";
        case MASK420:
          return "7712bdab1d1469cb";
        case MASK422:
          return "babb2b1e8c3e743f";
        case MASK420_MPEG2:
          return "9ea76060583478b9";
        case MASK422_MPEG2:
          return "421f8079ac704fbe";
        case MASK420_TOPLEFT:
          return "dc1e7036ac538b83";
        case MASK422_TOPLEFT:
          return "6ce91f1354061b87";
        default:
          break;
      }
    } else {
      switch (mode) {
        case MASK444:
          return "3e803ad138c2bd56";
        case MASK420:
          return "2012bddf9fac9abb";
        case MASK422:
          return "5ef2fcdf3375f0a3";
        case MASK420_MPEG2:
          return "53333e3e85b5b2a6";
        case MASK422_MPEG2:
          return "902a84966e047bab";
        case MASK420_TOPLEFT:
          return "64baadc5d6e8d46e";
        case MASK422_TOPLEFT:
          return "20599fe4adfa7a58";
        default:
          break;
      }
    }
  } else if (full_opacity) {
    switch (mode) {
      case MASK444:
        return "74812dbb85566db1";
      case MASK420:
        return "bc9ce1f2cb6e988a";
      case MASK422:
        return "8437830a12e92639";
      case MASK420_MPEG2:
        return "b1560b0d4785b73f";
      case MASK422_MPEG2:
        return "e380ef45516a95b6";
      case MASK420_TOPLEFT:
        return "41f13dbf16bd6b5a";
      case MASK422_TOPLEFT:
        return "3080c53ada902cf8";
      default:
        break;
    }
  } else {
    switch (mode) {
      case MASK444:
        return "f13eb0bc0b2bf106";
      case MASK420:
        return "2ecff2c240d70de8";
      case MASK422:
        return "4d6583d3c52ba529";
      case MASK420_MPEG2:
        return "7ecd08024daf47d2";
      case MASK422_MPEG2:
        return "9170ad6c1c643eb4";
      case MASK420_TOPLEFT:
        return "4ce86c41e85d4a69";
      case MASK422_TOPLEFT:
        return "602b491cbf1736aa";
      default:
        break;
    }
  }
  return {};
}

std::vector<LayerYuvAddCase> layer_yuv_add_cases() {
  std::vector<LayerYuvAddCase> cases;
  for (const int bits_per_pixel : {8, 16}) {
    const auto max_value = (1 << bits_per_pixel) - 1;
    const auto width = bits_per_pixel == 8 ? std::size_t{17} : std::size_t{9};
    const auto max_name = "Max" + std::to_string(max_value);
    const auto partial = max_value * 3 / 5;
    const auto partial_name = "Partial" + std::to_string(partial);
    for (const auto opacity : {max_value, partial}) {
      const auto& opacity_name = opacity == max_value ? max_name : partial_name;
      for (const auto placement : {PLACEMENT_MPEG1, PLACEMENT_MPEG2, PLACEMENT_TOPLEFT}) {
        const auto mask420_mode = layer_mask_mode(true, 420, placement);
        const auto mask422_mode = layer_mask_mode(true, 422, placement);
        cases.push_back(make_layer_yuv_add_case(
            true, 420, placement, bits_per_pixel, width, 5, opacity, opacity_name,
            Variant<LayerYuvAddFuncPtr>{"sse41",
                                        layer_sse41_function(true, 420, placement, bits_per_pixel),
                                        IsaRequirement::Sse41},
            layer_expected_hash(mask420_mode, bits_per_pixel, opacity == max_value)));
        cases.push_back(make_layer_yuv_add_case(
            true, 420, placement, bits_per_pixel, width, 5, opacity, opacity_name,
            Variant<LayerYuvAddFuncPtr>{"avx2",
                                        layer_avx2_function(true, 420, placement, bits_per_pixel),
                                        IsaRequirement::Avx2},
            layer_expected_hash(mask420_mode, bits_per_pixel, opacity == max_value)));
        cases.push_back(make_layer_yuv_add_case(
            true, 422, placement, bits_per_pixel, width, 5, opacity, opacity_name,
            Variant<LayerYuvAddFuncPtr>{"sse41",
                                        layer_sse41_function(true, 422, placement, bits_per_pixel),
                                        IsaRequirement::Sse41},
            layer_expected_hash(mask422_mode, bits_per_pixel, opacity == max_value)));
        cases.push_back(make_layer_yuv_add_case(
            true, 422, placement, bits_per_pixel, width, 5, opacity, opacity_name,
            Variant<LayerYuvAddFuncPtr>{"avx2",
                                        layer_avx2_function(true, 422, placement, bits_per_pixel),
                                        IsaRequirement::Avx2},
            layer_expected_hash(mask422_mode, bits_per_pixel, opacity == max_value)));
      }
      cases.push_back(make_layer_yuv_add_case(
          false, 420, PLACEMENT_MPEG2, bits_per_pixel, width, 5, opacity, opacity_name,
          Variant<LayerYuvAddFuncPtr>{
              "sse41", layer_sse41_function(false, 420, PLACEMENT_MPEG2, bits_per_pixel),
              IsaRequirement::Sse41},
          layer_expected_hash(MASK444, bits_per_pixel, opacity == max_value)));
      cases.push_back(make_layer_yuv_add_case(
          false, 420, PLACEMENT_MPEG2, bits_per_pixel, width, 5, opacity, opacity_name,
          Variant<LayerYuvAddFuncPtr>{
              "avx2", layer_avx2_function(false, 420, PLACEMENT_MPEG2, bits_per_pixel),
              IsaRequirement::Avx2},
          layer_expected_hash(MASK444, bits_per_pixel, opacity == max_value)));
    }
  }
  return cases;
}

class LayerYuvAddKernels : public ::testing::TestWithParam<LayerYuvAddCase> {};

TEST_P(LayerYuvAddKernels, MatchesPlacementAwareReference) {
  const auto& test_case = GetParam();
  if (!test_case.variant.function) {
    GTEST_SKIP() << "upstream did not provide " << test_case.variant.name
                 << " Layer YUV add function";
  }
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  if (test_case.bits_per_pixel == 8) {
    run_layer_yuv_add_case<std::uint8_t>(test_case);
  } else {
    run_layer_yuv_add_case<std::uint16_t>(test_case);
  }
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerYuvAddKernels, ::testing::ValuesIn(layer_yuv_add_cases()),
                         [](const ::testing::TestParamInfo<LayerYuvAddCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
