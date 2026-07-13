#include <gtest/gtest.h>

#include "layer_test_helpers.h"
#include "layer_colorkey_test_helpers.h"
#include "layer_invert_test_helpers.h"
#include "layer_mask_test_helpers.h"
#include "layer_packed_blend_test_helpers.h"
#include "layer_planarrgb_add_test_helpers.h"
#include "layer_rgb32_add_test_helpers.h"
#include "layer_rgb32_fast_test_helpers.h"
#include "layer_rgb32_subtract_test_helpers.h"

#include "support/cpu_features.h"

#include <string>
#include <utility>
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

std::vector<LayerMaskCase> layer_mask_cases() {
  constexpr std::size_t width_pixels = 9;
  constexpr std::size_t height = 3;
  constexpr std::size_t source_pitch = 64;
  constexpr std::size_t alpha_pitch = 80;
  return {
      make_layer_mask_case(width_pixels, height, source_pitch, alpha_pitch,
                           Variant<LayerMaskFunction>{"sse2", mask_sse2, IsaRequirement::Sse2},
                           "1867ef60337953e9"),
      make_layer_mask_case(width_pixels, height, source_pitch, alpha_pitch,
                           Variant<LayerMaskFunction>{"avx2", mask_avx2, IsaRequirement::Avx2},
                           "1867ef60337953e9"),
  };
}

class LayerMaskKernels : public ::testing::TestWithParam<LayerMaskCase> {};

TEST_P(LayerMaskKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_mask_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerMaskKernels, ::testing::ValuesIn(layer_mask_cases()),
                         [](const ::testing::TestParamInfo<LayerMaskCase>& info) {
                           return info.param.name;
                         });

std::vector<LayerColorKeyMaskCase> layer_colorkey_cases() {
  constexpr std::size_t width_pixels = 13;
  constexpr std::size_t height = 3;
  constexpr std::size_t pitch = 64;
  constexpr int color = 0x00c87828;
  constexpr int tolerance_b = 3;
  constexpr int tolerance_g = 5;
  constexpr int tolerance_r = 7;
  return {
      make_layer_colorkey_case(
          width_pixels, height, pitch, color, tolerance_b, tolerance_g, tolerance_r,
          Variant<LayerColorKeyMaskFunction>{"sse2", colorkeymask_sse2, IsaRequirement::Sse2},
          "0c2bba8d6e79353b"),
      make_layer_colorkey_case(
          width_pixels, height, pitch, color, tolerance_b, tolerance_g, tolerance_r,
          Variant<LayerColorKeyMaskFunction>{"avx2", colorkeymask_avx2, IsaRequirement::Avx2},
          "0c2bba8d6e79353b"),
  };
}

class LayerColorKeyMaskKernels : public ::testing::TestWithParam<LayerColorKeyMaskCase> {};

TEST_P(LayerColorKeyMaskKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_colorkey_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerColorKeyMaskKernels,
                         ::testing::ValuesIn(layer_colorkey_cases()),
                         [](const ::testing::TestParamInfo<LayerColorKeyMaskCase>& info) {
                           return info.param.name;
                         });

std::vector<LayerRgb32FastCase> layer_rgb32_fast_cases() {
  constexpr std::size_t width_pixels = 11;
  constexpr std::size_t height = 3;
  constexpr std::size_t destination_pitch = 64;
  constexpr std::size_t overlay_pitch = 80;
  return {
      make_layer_rgb32_fast_case(
          width_pixels, height, destination_pitch, overlay_pitch,
          Variant<LayerRgb32FastFunction>{"sse2", layer_rgb32_fast_sse2, IsaRequirement::Sse2},
          "dbfeb368cbc16789"),
      make_layer_rgb32_fast_case(
          width_pixels, height, destination_pitch, overlay_pitch,
          Variant<LayerRgb32FastFunction>{"avx2", layer_rgb32_fast_avx2, IsaRequirement::Avx2},
          "dbfeb368cbc16789"),
  };
}

class LayerRgb32FastKernels : public ::testing::TestWithParam<LayerRgb32FastCase> {};

TEST_P(LayerRgb32FastKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_rgb32_fast_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerRgb32FastKernels,
                         ::testing::ValuesIn(layer_rgb32_fast_cases()),
                         [](const ::testing::TestParamInfo<LayerRgb32FastCase>& info) {
                           return info.param.name;
                         });

std::vector<LayerRgb32AddCase> layer_rgb32_add_cases() {
  constexpr std::size_t width_pixels = 7;
  constexpr std::size_t height = 3;
  constexpr std::size_t destination_pitch = 64;
  constexpr std::size_t overlay_pitch = 80;
  constexpr int full_level = 257;
  constexpr int partial_level = 173;
  return {
      make_layer_rgb32_add_case(
          width_pixels, height, destination_pitch, overlay_pitch, full_level, "Full257",
          Variant<LayerRgb32AddFunction>{"sse2", layer_rgb32_add_sse2<false>, IsaRequirement::Sse2},
          "c1653fdf75d25f9e"),
      make_layer_rgb32_add_case(
          width_pixels, height, destination_pitch, overlay_pitch, full_level, "Full257",
          Variant<LayerRgb32AddFunction>{"avx2", layer_rgb32_add_avx2<false>, IsaRequirement::Avx2},
          "c1653fdf75d25f9e"),
      make_layer_rgb32_add_case(
          width_pixels, height, destination_pitch, overlay_pitch, partial_level, "Partial173",
          Variant<LayerRgb32AddFunction>{"sse2", layer_rgb32_add_sse2<false>, IsaRequirement::Sse2},
          "8a5a44620cfb2a74"),
      make_layer_rgb32_add_case(
          width_pixels, height, destination_pitch, overlay_pitch, partial_level, "Partial173",
          Variant<LayerRgb32AddFunction>{"avx2", layer_rgb32_add_avx2<false>, IsaRequirement::Avx2},
          "8a5a44620cfb2a74"),
  };
}

class LayerRgb32AddKernels : public ::testing::TestWithParam<LayerRgb32AddCase> {};

TEST_P(LayerRgb32AddKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_rgb32_add_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerRgb32AddKernels,
                         ::testing::ValuesIn(layer_rgb32_add_cases()),
                         [](const ::testing::TestParamInfo<LayerRgb32AddCase>& info) {
                           return info.param.name;
                         });

std::vector<LayerRgb32SubtractCase> layer_rgb32_subtract_cases() {
  constexpr std::size_t width_pixels = 7;
  constexpr std::size_t height = 3;
  constexpr std::size_t destination_pitch = 64;
  constexpr std::size_t overlay_pitch = 80;
  constexpr int full_level = 257;
  constexpr int partial_level = 173;
  return {
      make_layer_rgb32_subtract_case(
          width_pixels, height, destination_pitch, overlay_pitch, full_level, "Full257",
          Variant<LayerRgb32SubtractFunction>{"sse2", layer_rgb32_subtract_sse2<false>,
                                              IsaRequirement::Sse2},
          "6ea3751be3e04826"),
      make_layer_rgb32_subtract_case(
          width_pixels, height, destination_pitch, overlay_pitch, full_level, "Full257",
          Variant<LayerRgb32SubtractFunction>{"avx2", layer_rgb32_subtract_avx2<false>,
                                              IsaRequirement::Avx2},
          "6ea3751be3e04826"),
      make_layer_rgb32_subtract_case(
          width_pixels, height, destination_pitch, overlay_pitch, partial_level, "Partial173",
          Variant<LayerRgb32SubtractFunction>{"sse2", layer_rgb32_subtract_sse2<false>,
                                              IsaRequirement::Sse2},
          "340ea8bf55efe193"),
      make_layer_rgb32_subtract_case(
          width_pixels, height, destination_pitch, overlay_pitch, partial_level, "Partial173",
          Variant<LayerRgb32SubtractFunction>{"avx2", layer_rgb32_subtract_avx2<false>,
                                              IsaRequirement::Avx2},
          "340ea8bf55efe193"),
  };
}

class LayerRgb32SubtractKernels : public ::testing::TestWithParam<LayerRgb32SubtractCase> {};

TEST_P(LayerRgb32SubtractKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_rgb32_subtract_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerRgb32SubtractKernels,
                         ::testing::ValuesIn(layer_rgb32_subtract_cases()),
                         [](const ::testing::TestParamInfo<LayerRgb32SubtractCase>& info) {
                           return info.param.name;
                         });

std::string layer_invert_expected_hash(int bits_per_pixel, bool chroma) {
  if (bits_per_pixel == 8) {
    return chroma ? "63dae183f4e233e3" : "3b5bf145fdaed0cb";
  }
  switch (bits_per_pixel) {
    case 10:
      return chroma ? "7c15f9f33b835a8a" : "5c304cf2053ce54b";
    case 12:
      return chroma ? "67303d0b4081f759" : "b533f26d2adcd798";
    case 14:
      return chroma ? "fef1da539fd5077b" : "bb93d2e087f3ffb1";
    case 16:
      return chroma ? "0c914f9bead77af9" : "744ac3dfbf30785f";
    default:
      return {};
  }
}

std::vector<LayerInvertCase> layer_invert_cases() {
  std::vector<LayerInvertCase> cases;
  constexpr std::size_t height = 3;
  constexpr std::size_t source_pitch = 96;
  constexpr std::size_t destination_pitch = 128;

  for (const bool chroma : {false, true}) {
    cases.push_back(make_layer_invert_case(
        LayerInvertElement::UInt8, chroma, 8, 64, height, source_pitch, destination_pitch,
        Variant<LayerInvertFuncPtr>{
            "sse2", chroma ? invert_plane_sse2_u8<true> : invert_plane_sse2_u8<false>,
            IsaRequirement::Sse2},
        layer_invert_expected_hash(8, chroma)));
    cases.push_back(make_layer_invert_case(
        LayerInvertElement::UInt8, chroma, 8, 64, height, source_pitch, destination_pitch,
        Variant<LayerInvertFuncPtr>{
            "avx2", chroma ? invert_plane_avx2_u8<true> : invert_plane_avx2_u8<false>,
            IsaRequirement::Avx2},
        layer_invert_expected_hash(8, chroma)));
  }

  for (const int bits_per_pixel : {10, 12, 14, 16}) {
    for (const bool chroma : {false, true}) {
      cases.push_back(make_layer_invert_case(
          LayerInvertElement::UInt16, chroma, bits_per_pixel, 32, height, source_pitch,
          destination_pitch,
          Variant<LayerInvertFuncPtr>{
              "sse2", chroma ? invert_plane_sse2_u16<true> : invert_plane_sse2_u16<false>,
              IsaRequirement::Sse2},
          layer_invert_expected_hash(bits_per_pixel, chroma)));
      cases.push_back(make_layer_invert_case(
          LayerInvertElement::UInt16, chroma, bits_per_pixel, 32, height, source_pitch,
          destination_pitch,
          Variant<LayerInvertFuncPtr>{
              "avx2", chroma ? invert_plane_avx2_u16<true> : invert_plane_avx2_u16<false>,
              IsaRequirement::Avx2},
          layer_invert_expected_hash(bits_per_pixel, chroma)));
    }
  }

  for (const bool chroma : {false, true}) {
    cases.push_back(make_layer_invert_case(
        LayerInvertElement::Float32, chroma, 32, 16, height, source_pitch, destination_pitch,
        Variant<LayerInvertFuncPtr>{
            "sse2", chroma ? invert_plane_sse2_f32<true> : invert_plane_sse2_f32<false>,
            IsaRequirement::Sse2}));
    cases.push_back(make_layer_invert_case(
        LayerInvertElement::Float32, chroma, 32, 16, height, source_pitch, destination_pitch,
        Variant<LayerInvertFuncPtr>{
            "avx2", chroma ? invert_plane_avx2_f32<true> : invert_plane_avx2_f32<false>,
            IsaRequirement::Avx2}));
  }
  return cases;
}

class LayerInvertKernels : public ::testing::TestWithParam<LayerInvertCase> {};

TEST_P(LayerInvertKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_invert_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerInvertKernels, ::testing::ValuesIn(layer_invert_cases()),
                         [](const ::testing::TestParamInfo<LayerInvertCase>& info) {
                           return info.param.name;
                         });

std::string layer_packed_blend_expected_hash(bool has_separate_mask, int opacity) {
  if (has_separate_mask) {
    return opacity == 255 ? "f8ed38a808275fff" : "43898c99879818e0";
  }
  return opacity == 255 ? "7340a277058b7fe7" : "f4637421ba6b129d";
}

std::vector<LayerPackedBlendCase> layer_packed_blend_cases() {
  std::vector<LayerPackedBlendCase> cases;
  constexpr std::size_t width = 9;
  constexpr std::size_t height = 3;
  constexpr std::size_t destination_pitch = 64;
  constexpr std::size_t overlay_pitch = 80;
  constexpr std::size_t mask_pitch = 32;
  for (const bool has_separate_mask : {false, true}) {
    for (const auto [opacity, opacity_name] :
         {std::pair{255, std::string{"Full255"}}, std::pair{173, std::string{"Partial173"}}}) {
      cases.push_back(make_layer_packed_blend_case(
          has_separate_mask, width, height, destination_pitch, overlay_pitch, mask_pitch, opacity,
          opacity_name, Variant<LayerPackedBlendFuncPtr>{"sse41", nullptr, IsaRequirement::Sse41},
          layer_packed_blend_expected_hash(has_separate_mask, opacity)));
      get_layer_packedrgb_blend_functions_sse41(has_separate_mask, 8,
                                                &cases.back().variant.function);
      cases.back().name = layer_packed_blend_case_name(cases.back());
      cases.push_back(make_layer_packed_blend_case(
          has_separate_mask, width, height, destination_pitch, overlay_pitch, mask_pitch, opacity,
          opacity_name, Variant<LayerPackedBlendFuncPtr>{"avx2", nullptr, IsaRequirement::Avx2},
          layer_packed_blend_expected_hash(has_separate_mask, opacity)));
      get_layer_packedrgb_blend_functions_avx2(has_separate_mask, 8,
                                               &cases.back().variant.function);
      cases.back().name = layer_packed_blend_case_name(cases.back());
    }
  }
  return cases;
}

class LayerPackedBlendKernels : public ::testing::TestWithParam<LayerPackedBlendCase> {};

TEST_P(LayerPackedBlendKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!test_case.variant.function) {
    GTEST_SKIP() << "upstream did not provide " << test_case.variant.name
                 << " packed RGB blend function";
  }
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_layer_packed_blend_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerPackedBlendKernels,
                         ::testing::ValuesIn(layer_packed_blend_cases()),
                         [](const ::testing::TestParamInfo<LayerPackedBlendCase>& info) {
                           return info.param.name;
                         });

std::string layer_planarrgb_add_expected_hash(bool blend_alpha, int bits_per_pixel,
                                              bool full_opacity) {
  if (bits_per_pixel == 8) {
    if (blend_alpha) {
      return full_opacity ? "0e7a603078865d27" : "75680622264aad12";
    }
    return full_opacity ? "65773d6eb085583d" : "72a225f6f71380e3";
  }
  if (blend_alpha) {
    return full_opacity ? "94fcfcd368ebb5f1" : "b54328941bb23a77";
  }
  return full_opacity ? "173587b424c0f452" : "0b6d809dfd24a08e";
}

std::vector<LayerPlanarRgbAddCase> layer_planarrgb_add_cases() {
  std::vector<LayerPlanarRgbAddCase> cases;
  for (const int bits_per_pixel : {8, 16}) {
    const auto max_value = (1 << bits_per_pixel) - 1;
    const auto width = bits_per_pixel == 8 ? std::size_t{37} : std::size_t{19};
    const auto destination_pitch = bits_per_pixel == 8 ? std::size_t{64} : std::size_t{96};
    const auto overlay_pitch = bits_per_pixel == 8 ? std::size_t{80} : std::size_t{112};
    const auto mask_pitch = bits_per_pixel == 8 ? std::size_t{64} : std::size_t{96};
    for (const bool blend_alpha : {false, true}) {
      for (const int opacity : {max_value, max_value * 3 / 5}) {
        const auto opacity_name = opacity == max_value ? "Max" + std::to_string(max_value)
                                                       : "Partial" + std::to_string(opacity);
        cases.push_back(make_layer_planarrgb_add_case(
            blend_alpha, bits_per_pixel, width, 3, destination_pitch, overlay_pitch, mask_pitch,
            opacity, opacity_name, "sse41", IsaRequirement::Sse41, false,
            layer_planarrgb_add_expected_hash(blend_alpha, bits_per_pixel, opacity == max_value)));
        cases.push_back(make_layer_planarrgb_add_case(
            blend_alpha, bits_per_pixel, width, 3, destination_pitch, overlay_pitch, mask_pitch,
            opacity, opacity_name, "avx2", IsaRequirement::Avx2, true,
            layer_planarrgb_add_expected_hash(blend_alpha, bits_per_pixel, opacity == max_value)));
      }
    }
  }
  return cases;
}

class LayerPlanarRgbAddKernels : public ::testing::TestWithParam<LayerPlanarRgbAddCase> {};

TEST_P(LayerPlanarRgbAddKernels, MatchesIndependentReference) {
  const auto& test_case = GetParam();
  if (!test_case.variant.function) {
    GTEST_SKIP() << "upstream did not provide " << test_case.variant.name
                 << " planar RGB add function";
  }
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  if (test_case.bits_per_pixel == 8) {
    run_layer_planarrgb_add_case<std::uint8_t>(test_case);
  } else {
    run_layer_planarrgb_add_case<std::uint16_t>(test_case);
  }
}

INSTANTIATE_TEST_SUITE_P(Kernels, LayerPlanarRgbAddKernels,
                         ::testing::ValuesIn(layer_planarrgb_add_cases()),
                         [](const ::testing::TestParamInfo<LayerPlanarRgbAddCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
