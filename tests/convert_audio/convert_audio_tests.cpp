#include <gtest/gtest.h>

#include "convert_audio_test_helpers.h"

#include "support/cpu_features.h"

#include <array>
#include <vector>

namespace avsut::test {
namespace {

template <typename Function>
void add_integer_variants(std::vector<AudioIntegerCase>& cases, AudioFormat source,
                          AudioFormat destination, std::size_t count, const char* expected_hash,
                          Function c_function, Function sse2_function,
                          Function avx2_function = nullptr) {
  cases.push_back(make_audio_integer_case(
      source, destination, count,
      Variant<AudioConvertFunction>{"c", c_function, IsaRequirement::Scalar}, expected_hash));
  cases.push_back(make_audio_integer_case(
      source, destination, count,
      Variant<AudioConvertFunction>{"sse2", sse2_function, IsaRequirement::Sse2}, expected_hash));
  if (avx2_function != nullptr) {
    cases.push_back(make_audio_integer_case(
        source, destination, count,
        Variant<AudioConvertFunction>{"avx2", avx2_function, IsaRequirement::Avx2}, expected_hash));
  }
}

std::vector<AudioIntegerCase> audio_integer_cases() {
  constexpr std::array<std::size_t, 6> counts{7, 8, 9, 15, 16, 17};
  constexpr std::array<const char*, 6> s32_to_s16{"72636ca0711e509d", "bc0055e922cca5c1",
                                                  "dddf915c0a598053", "7f85c2db07160a03",
                                                  "8583483f9c7f777d", "236d8ef6157ac922"};
  constexpr std::array<const char*, 6> s16_to_s32{"7f585b802d4eefde", "a709f5d101dab569",
                                                  "8493cdc5eac29749", "f58c1429ba6987ee",
                                                  "e75c50f543851658", "5fb431b11e75eca8"};
  constexpr std::array<const char*, 6> s32_to_u8{"2fa42ce33b9b9690", "26631036a0122595",
                                                 "5089246ee560804b", "94e50bd25b3fd8ca",
                                                 "4e7769dc192a2c6d", "20fa999e5e2c8324"};
  constexpr std::array<const char*, 6> u8_to_s32{"1365282cea4e33d3", "f53ba3e2b796b5a5",
                                                 "9e6d204fccb2fb77", "1e543f3519614fba",
                                                 "f2e1a143da816fa7", "5bd5391fc2326e74"};
  constexpr std::array<const char*, 6> s16_to_u8{"6d140c77ecf217a9", "825796fed5f9a30b",
                                                 "8fea9faa63e457e4", "f5cc9a5828084c2c",
                                                 "a9109668148bb410", "9b3d2c9927677728"};
  constexpr std::array<const char*, 6> u8_to_s16{"5f8296cbe98967a7", "b827603c85c112e3",
                                                 "256c7f8cff50d066", "65e83ea200c3bb22",
                                                 "e9f8dbb9828874e2", "730ae7733badca4d"};
  std::vector<AudioIntegerCase> cases;
  for (std::size_t index = 0; index < counts.size(); ++index) {
    const auto count = counts[index];
    add_integer_variants(cases, AudioFormat::S32, AudioFormat::S16, count, s32_to_s16[index],
                         convert32To16, convert32To16_SSE2, convert32To16_AVX2);
    add_integer_variants(cases, AudioFormat::S16, AudioFormat::S32, count, s16_to_s32[index],
                         convert16To32, convert16To32_SSE2, convert16To32_AVX2);
    add_integer_variants(cases, AudioFormat::S32, AudioFormat::U8, count, s32_to_u8[index],
                         convert32To8, convert32To8_SSE2);
    add_integer_variants(cases, AudioFormat::U8, AudioFormat::S32, count, u8_to_s32[index],
                         convert8To32, convert8To32_SSE2);
    add_integer_variants(cases, AudioFormat::S16, AudioFormat::U8, count, s16_to_u8[index],
                         convert16To8, convert16To8_SSE2);
    add_integer_variants(cases, AudioFormat::U8, AudioFormat::S16, count, u8_to_s16[index],
                         convert8To16, convert8To16_SSE2);
  }
  return cases;
}

std::vector<AudioIntegerCase> audio_packed_24_cases() {
  constexpr std::array<std::size_t, 3> counts{15, 16, 17};
  constexpr std::array<const char*, 3> s32_to_s24{"56c34a4a40d2b71f", "ce35878a8c7ba570",
                                                  "27a959747be110f8"};
  constexpr std::array<const char*, 3> s24_to_s32{"544236777f760a57", "1a6fdfcb7259f9d6",
                                                  "4c7abe1834a5cfd4"};
  constexpr std::array<const char*, 3> s24_to_s16{"b9a412f8dd275583", "a5f6058831cbef94",
                                                  "eb36bfad072941c8"};
  constexpr std::array<const char*, 3> s16_to_s24{"9d6f6a2f95056b94", "d615b9752abe3b7f",
                                                  "508d07839f9959a4"};
  constexpr std::array<const char*, 3> s24_to_u8{"be5a748b497f8a72", "fd804383de2f7954",
                                                 "7b8589ad51b4c7bc"};
  constexpr std::array<const char*, 3> u8_to_s24{"14ad13c337e3e265", "3affb442fd5a2bf7",
                                                 "a7b10fbd795a1245"};
  std::vector<AudioIntegerCase> cases;
  for (std::size_t index = 0; index < counts.size(); ++index) {
    const auto count = counts[index];
    cases.push_back(make_audio_integer_case(
        AudioFormat::S32, AudioFormat::S24, count,
        Variant<AudioConvertFunction>{"c", convert32To24, IsaRequirement::Scalar},
        s32_to_s24[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S32, AudioFormat::S24, count,
        Variant<AudioConvertFunction>{"ssse3", convert32To24_SSSE3, IsaRequirement::Ssse3},
        s32_to_s24[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S24, AudioFormat::S32, count,
        Variant<AudioConvertFunction>{"c", convert24To32, IsaRequirement::Scalar},
        s24_to_s32[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S24, AudioFormat::S32, count,
        Variant<AudioConvertFunction>{"ssse3", convert24To32_SSSE3, IsaRequirement::Ssse3},
        s24_to_s32[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S24, AudioFormat::S16, count,
        Variant<AudioConvertFunction>{"c", convert24To16, IsaRequirement::Scalar},
        s24_to_s16[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S24, AudioFormat::S16, count,
        Variant<AudioConvertFunction>{"ssse3", convert24To16_SSSE3, IsaRequirement::Ssse3},
        s24_to_s16[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S16, AudioFormat::S24, count,
        Variant<AudioConvertFunction>{"c", convert16To24, IsaRequirement::Scalar},
        s16_to_s24[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S16, AudioFormat::S24, count,
        Variant<AudioConvertFunction>{"ssse3", convert16To24_SSSE3, IsaRequirement::Ssse3},
        s16_to_s24[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S24, AudioFormat::U8, count,
        Variant<AudioConvertFunction>{"c", convert24To8, IsaRequirement::Scalar},
        s24_to_u8[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::S24, AudioFormat::U8, count,
        Variant<AudioConvertFunction>{"ssse3", convert24To8_SSSE3, IsaRequirement::Ssse3},
        s24_to_u8[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::U8, AudioFormat::S24, count,
        Variant<AudioConvertFunction>{"c", convert8To24, IsaRequirement::Scalar},
        u8_to_s24[index]));
    cases.push_back(make_audio_integer_case(
        AudioFormat::U8, AudioFormat::S24, count,
        Variant<AudioConvertFunction>{"ssse3", convert8To24_SSSE3, IsaRequirement::Ssse3},
        u8_to_s24[index]));
  }
  return cases;
}

class AudioIntegerKernels : public ::testing::TestWithParam<AudioIntegerCase> {};

TEST_P(AudioIntegerKernels, MatchesIndependentIntegerReference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_audio_integer_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Kernels, AudioIntegerKernels, ::testing::ValuesIn(audio_integer_cases()),
                         [](const ::testing::TestParamInfo<AudioIntegerCase>& info) {
                           return info.param.name;
                         });

class AudioPacked24Kernels : public ::testing::TestWithParam<AudioIntegerCase> {};

TEST_P(AudioPacked24Kernels, MatchesIndependentPacked24Reference) {
  const auto& test_case = GetParam();
  if (!variant_supported(test_case.variant, CpuFeatures::detect())) {
    GTEST_SKIP() << "host does not support " << test_case.variant.name;
  }
  run_audio_integer_case(test_case);
}

INSTANTIATE_TEST_SUITE_P(Packed24, AudioPacked24Kernels,
                         ::testing::ValuesIn(audio_packed_24_cases()),
                         [](const ::testing::TestParamInfo<AudioIntegerCase>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace avsut::test
