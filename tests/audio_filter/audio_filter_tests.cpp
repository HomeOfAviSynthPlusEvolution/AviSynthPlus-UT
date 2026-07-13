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

#include <cstdint>
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

}  // namespace
