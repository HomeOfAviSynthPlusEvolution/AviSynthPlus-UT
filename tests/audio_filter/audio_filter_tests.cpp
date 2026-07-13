#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_AUDIO_FILTER_UNDEF_AVS_UNUSED
#endif
#include "core/internal.h"
#include "core/audio.h"
#ifdef AVSUT_AUDIO_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_AUDIO_FILTER_UNDEF_AVS_UNUSED
#endif

#include "convert/convert_audio.h"

#include "support/audio_test_helpers.h"
#include "support/avisynth_environment.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

using avsut::test::AudioBoundsPolicy;
using avsut::test::AudioInfoSpec;
using avsut::test::AudioSequenceClip;
using avsut::test::AviSynthEnvironment;
using avsut::test::expect_audio_requests;
using avsut::test::expect_audio_source_unchanged;
using avsut::test::expect_exact_audio;
using avsut::test::expect_float_audio;
using avsut::test::GuardedAudioBuffer;
using avsut::test::make_audio_bytes;
using avsut::test::make_audio_video_info;

int amplify_int16_reference(std::int16_t sample, int factor) {
  const auto scaled = (static_cast<std::int64_t>(sample) * factor + 65536) >> 17;
  return static_cast<int>(std::clamp<std::int64_t>(scaled, std::numeric_limits<std::int16_t>::min(),
                                                   std::numeric_limits<std::int16_t>::max()));
}

std::int16_t mix_int16_reference(std::int16_t first, std::int16_t second, int first_factor,
                                 int second_factor) {
  const auto scaled = (static_cast<std::int64_t>(first) * first_factor +
                       static_cast<std::int64_t>(second) * second_factor + 65536) >>
                      17;
  return static_cast<std::int16_t>(std::clamp<std::int64_t>(
      scaled, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
}

TEST(ConvertAudioFilter, ConvertsSigned16ToFloatForRequestedInterleavedWindow) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_INT16, 4, 2});
  const std::vector<std::int16_t> samples{0, 32767, -32768, 16384, -16384, 1, 8192, -8192};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();

  ConvertAudio filter(source, SAMPLE_FLOAT);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(2), 64, 64, 1);
  filter.GetAudio(output.data(), 1, 2, environment.get());

  expect_float_audio(output, {-1.0F, 16384.0F / 32768.0F, -16384.0F / 32768.0F, 1.0F / 32768.0F});
  EXPECT_EQ(filter.GetVideoInfo().SampleType(), SAMPLE_FLOAT);
  EXPECT_EQ(filter.GetVideoInfo().AudioChannels(), 2);
  expect_audio_requests(*source_clip, {{1, 2}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), 0);
  EXPECT_TRUE(output.memory_intact());
}

TEST(ConvertAudioFilter, ConvertsFloatToSigned16WithEndpointClamping) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{44100, SAMPLE_FLOAT, 5, 1});
  const std::vector<float> samples{-1.25F, -1.0F, -0.5F, 0.5F, 1.25F};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();

  ConvertAudio filter(source, SAMPLE_INT16);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(5), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 5, environment.get());

  expect_exact_audio<std::int16_t>(output, {-32768, -32768, -16384, 16384, 32767});
  EXPECT_EQ(filter.GetVideoInfo().SampleType(), SAMPLE_INT16);
  expect_audio_requests(*source_clip, {{0, 5}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_TRUE(output.memory_intact());
}

TEST(ConvertToMonoFilter, AveragesAllFloatChannelsAndSetsMonoLayout) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_FLOAT, 3, 3});
  const std::vector<float> samples{0.3F, 0.0F, -0.3F, 1.0F, 0.5F, 0.0F, -0.5F, -0.25F, 0.25F};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();

  ConvertToMono filter(source);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(2), 64, 64, 1);
  filter.GetAudio(output.data(), 1, 2, environment.get());

  expect_float_audio(output, {0.5F, (-0.5F - 0.25F + 0.25F) / 3.0F});
  EXPECT_EQ(filter.GetVideoInfo().AudioChannels(), 1);
  EXPECT_TRUE(filter.GetVideoInfo().IsChannelMaskKnown());
  EXPECT_EQ(filter.GetVideoInfo().GetChannelMask(), AVS_CHANNEL_LAYOUT_MONO);
  expect_audio_requests(*source_clip, {{1, 2}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_SERIALIZED);
  EXPECT_TRUE(output.memory_intact());
}

TEST(GetChannelFilter, SelectsRequestedChannelsInOrderForSigned16) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_INT16, 3, 3});
  const std::vector<std::int16_t> samples{1, 2, 3, 4, 5, 6, 7, 8, 9};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();
  auto* selected_channels = new int[2]{2, 0};

  GetChannel filter(source, selected_channels, 2);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(3), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 3, environment.get());

  expect_exact_audio<std::int16_t>(output, {3, 1, 6, 4, 9, 7});
  EXPECT_EQ(filter.GetVideoInfo().AudioChannels(), 2);
  EXPECT_TRUE(filter.GetVideoInfo().IsChannelMaskKnown());
  EXPECT_EQ(filter.GetVideoInfo().GetChannelMask(), AVS_CHANNEL_LAYOUT_STEREO);
  expect_audio_requests(*source_clip, {{0, 3}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_SERIALIZED);
  EXPECT_TRUE(output.memory_intact());
}

TEST(MergeChannelsFilter, InterleavesMonoSourcesAfterConvertingTheSecondFormat) {
  AviSynthEnvironment environment;
  const auto int16_vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_INT16, 3, 1});
  const auto float_vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_FLOAT, 3, 1});
  const std::vector<std::int16_t> first_samples{100, -200, 300};
  const std::vector<float> second_samples{0.5F, -0.25F, 1.25F};
  auto* first_clip = new AudioSequenceClip(int16_vi, make_audio_bytes(first_samples));
  auto* second_clip = new AudioSequenceClip(float_vi, make_audio_bytes(second_samples));
  PClip first(first_clip);
  PClip second(second_clip);
  const auto first_before = first_clip->audio();
  const auto second_before = second_clip->audio();

  auto* children = new PClip[2];
  children[0] = first;
  children[1] = second;
  MergeChannels filter(first, 2, children, environment.get());
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(3), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 3, environment.get());

  expect_exact_audio<std::int16_t>(output, {100, 16384, -200, -8192, 300, 32767});
  EXPECT_EQ(filter.GetVideoInfo().AudioChannels(), 2);
  EXPECT_TRUE(filter.GetVideoInfo().IsChannelMaskKnown());
  EXPECT_EQ(filter.GetVideoInfo().GetChannelMask(), AVS_CHANNEL_LAYOUT_STEREO);
  expect_audio_requests(*first_clip, {{0, 3}});
  expect_audio_requests(*second_clip, {{0, 3}});
  expect_audio_source_unchanged(*first_clip, first_before);
  expect_audio_source_unchanged(*second_clip, second_before);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_SERIALIZED);
  EXPECT_TRUE(output.memory_intact());
}

TEST(DelayAudioFilter, ZeroFillsTheDelayedPrefixAndOffsetsTheChildRequest) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{4, SAMPLE_FLOAT, 4, 1});
  const std::vector<float> samples{0.1F, 0.2F, 0.3F, 0.4F};
  auto* source_clip =
      new AudioSequenceClip(vi, make_audio_bytes(samples), AudioBoundsPolicy::ZeroFill);
  PClip source(source_clip);
  const auto source_before = source_clip->audio();

  DelayAudio filter(0.5, source);
  ASSERT_EQ(filter.GetVideoInfo().num_audio_samples, 6);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(4), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 4, environment.get());

  expect_float_audio(output, {0.0F, 0.0F, 0.1F, 0.2F});
  expect_audio_requests(*source_clip, {{-2, 4}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_TRUE(output.memory_intact());
}

TEST(AmplifyFilter, AppliesPerChannelIntegerFactorsWithSaturation) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_INT16, 3, 2});
  const std::vector<std::int16_t> samples{20000, -20000, -32768, 32767, 1000, -1000};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();
  auto* volumes = new float[2]{2.0F, 0.5F};
  auto* factors = new int[2]{static_cast<int>(2.0F * 131072.0F + 0.5F),
                             static_cast<int>(0.5F * 131072.0F + 0.5F)};

  Amplify filter(source, volumes, factors);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(3), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 3, environment.get());

  const std::vector<std::int16_t> expected{
      static_cast<std::int16_t>(amplify_int16_reference(samples[0], factors[0])),
      static_cast<std::int16_t>(amplify_int16_reference(samples[1], factors[1])),
      static_cast<std::int16_t>(amplify_int16_reference(samples[2], factors[0])),
      static_cast<std::int16_t>(amplify_int16_reference(samples[3], factors[1])),
      static_cast<std::int16_t>(amplify_int16_reference(samples[4], factors[0])),
      static_cast<std::int16_t>(amplify_int16_reference(samples[5], factors[1]))};
  expect_exact_audio<std::int16_t>(output, expected);
  expect_audio_requests(*source_clip, {{0, 3}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_TRUE(output.memory_intact());
}

TEST(AmplifyFilter, AppliesPerChannelFloatFactorsWithoutClamping) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_FLOAT, 2, 2});
  const std::vector<float> samples{0.75F, -0.75F, -0.5F, 0.5F};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();
  auto* volumes = new float[2]{2.0F, 0.5F};
  auto* factors = new int[2]{static_cast<int>(2.0F * 131072.0F + 0.5F),
                             static_cast<int>(0.5F * 131072.0F + 0.5F)};

  Amplify filter(source, volumes, factors);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(2), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 2, environment.get());

  expect_float_audio(output, {1.5F, -0.375F, -1.0F, 0.25F});
  expect_audio_requests(*source_clip, {{0, 2}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_TRUE(output.memory_intact());
}

TEST(MixAudioFilter, MixesFloatTracksWithIndependentFactors) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_FLOAT, 3, 2});
  const std::vector<float> first_samples{0.8F, 0.2F, -0.4F, 0.6F, 0.0F, -0.8F};
  const std::vector<float> second_samples{-0.2F, 0.4F, 0.6F, -0.6F, 1.0F, 0.2F};
  auto* first_clip = new AudioSequenceClip(vi, make_audio_bytes(first_samples));
  auto* second_clip = new AudioSequenceClip(vi, make_audio_bytes(second_samples));
  PClip first(first_clip);
  PClip second(second_clip);
  const auto first_before = first_clip->audio();
  const auto second_before = second_clip->audio();

  MixAudio filter(first, second, 0.25, 0.75, environment.get());
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(3), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 3, environment.get());

  expect_float_audio(output, {0.05F, 0.35F, 0.35F, -0.3F, 0.75F, -0.05F});
  expect_audio_requests(*first_clip, {{0, 3}});
  expect_audio_requests(*second_clip, {{0, 3}});
  expect_audio_source_unchanged(*first_clip, first_before);
  expect_audio_source_unchanged(*second_clip, second_before);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_SERIALIZED);
  EXPECT_TRUE(output.memory_intact());
}

TEST(MixAudioFilter, SaturatesSigned16Results) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_INT16, 2, 1});
  const std::vector<std::int16_t> first_samples{30000, -30000};
  const std::vector<std::int16_t> second_samples{30000, -30000};
  auto* first_clip = new AudioSequenceClip(vi, make_audio_bytes(first_samples));
  auto* second_clip = new AudioSequenceClip(vi, make_audio_bytes(second_samples));
  PClip first(first_clip);
  PClip second(second_clip);

  MixAudio filter(first, second, 0.75, 0.75, environment.get());
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(2), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 2, environment.get());

  const int factor = static_cast<int>(0.75 * 131072.0 + 0.5);
  expect_exact_audio<std::int16_t>(output, {mix_int16_reference(30000, 30000, factor, factor),
                                            mix_int16_reference(-30000, -30000, factor, factor)});
  expect_audio_requests(*first_clip, {{0, 2}});
  expect_audio_requests(*second_clip, {{0, 2}});
  EXPECT_TRUE(output.memory_intact());
}

TEST(NormalizeFilter, ScansFloatStreamBeforeNormalizingRequestedWindow) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_FLOAT, 4, 1});
  const std::vector<float> samples{0.25F, -0.5F, 0.125F, 0.0F};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();

  Normalize filter(source, 1.0F, false);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(2), 64, 64, 1);
  filter.GetAudio(output.data(), 1, 2, environment.get());

  expect_float_audio(output, {-1.0F, 0.25F});
  expect_audio_requests(*source_clip, {{0, 4}, {1, 2}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_SERIALIZED);
  EXPECT_TRUE(output.memory_intact());
}

TEST(NormalizeFilter, ScansSigned16PeaksAndSaturatesOutput) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{48000, SAMPLE_INT16, 3, 1});
  const std::vector<std::int16_t> samples{-16384, 8192, 4096};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  const auto source_before = source_clip->audio();

  Normalize filter(source, 1.0F, false);
  GuardedAudioBuffer output(filter.GetVideoInfo().BytesFromAudioSamples(3), 64, 64, 1);
  filter.GetAudio(output.data(), 0, 3, environment.get());

  expect_exact_audio<std::int16_t>(output, {-32768, 16384, 8192});
  expect_audio_requests(*source_clip, {{0, 3}, {0, 3}});
  expect_audio_source_unchanged(*source_clip, source_before);
  EXPECT_TRUE(output.memory_intact());
}

TEST(EnsureVBRMP3SyncFilter, ReplaysSkippedAndRewoundAudioBeforeServingOutput) {
  AviSynthEnvironment environment;
  const auto vi = make_audio_video_info(AudioInfoSpec{44100, SAMPLE_FLOAT, 10, 1});
  const std::vector<float> samples{0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F, 0.9F};
  auto* source_clip = new AudioSequenceClip(vi, make_audio_bytes(samples));
  PClip source(source_clip);
  EnsureVBRMP3Sync filter(source);
  GuardedAudioBuffer first(vi.BytesFromAudioSamples(4), 64, 64, 1);
  GuardedAudioBuffer skipped(vi.BytesFromAudioSamples(2), 64, 64, 1);
  GuardedAudioBuffer rewound(vi.BytesFromAudioSamples(2), 64, 64, 1);

  filter.GetAudio(first.data(), 0, 4, environment.get());
  filter.GetAudio(skipped.data(), 8, 2, environment.get());
  filter.GetAudio(rewound.data(), 2, 2, environment.get());

  expect_float_audio(first, {0.0F, 0.1F, 0.2F, 0.3F});
  expect_float_audio(skipped, {0.8F, 0.9F});
  expect_float_audio(rewound, {0.2F, 0.3F});
  expect_audio_requests(*source_clip, {{0, 4}, {4, 4}, {8, 2}, {0, 2}, {2, 2}});
  EXPECT_EQ(filter.SetCacheHints(CACHE_GETCHILD_AUDIO_MODE, 0), CACHE_AUDIO);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GETCHILD_AUDIO_SIZE, 0), 1024 * 1024);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_SERIALIZED);
  EXPECT_TRUE(first.memory_intact());
  EXPECT_TRUE(skipped.memory_intact());
  EXPECT_TRUE(rewound.memory_intact());
}

}  // namespace
