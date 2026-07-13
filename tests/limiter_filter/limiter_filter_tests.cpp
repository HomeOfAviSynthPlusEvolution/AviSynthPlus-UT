#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_LIMITER_UNDEF_AVS_UNUSED
#endif
#include "filters/limiter.h"
#ifdef AVSUT_LIMITER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_LIMITER_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;

template <typename Pixel>
void write_values(PVideoFrame& frame, int plane, int width, int height,
                  const std::array<Pixel, 8>& values) {
  const int pitch = frame->GetPitch(plane);
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<Pixel*>(frame->GetWritePtr(plane) + y * pitch);
    for (int x = 0; x < width; ++x) {
      row[x] = values[static_cast<std::size_t>(x) % values.size()];
    }
  }
}

template <typename Pixel>
void expect_clamped_plane(const PVideoFrame& source, const PVideoFrame& output, int plane,
                          int width, int height, Pixel min_value, Pixel max_value) {
  const int source_pitch = source->GetPitch(plane);
  const int output_pitch = output->GetPitch(plane);
  const auto* source_base = source->GetReadPtr(plane);
  const auto* output_base = output->GetReadPtr(plane);
  for (int y = 0; y < height; ++y) {
    const auto* source_row = reinterpret_cast<const Pixel*>(source_base + y * source_pitch);
    const auto* output_row = reinterpret_cast<const Pixel*>(output_base + y * output_pitch);
    for (int x = 0; x < width; ++x) {
      const Pixel expected = std::clamp(source_row[x], min_value, max_value);
      EXPECT_EQ(output_row[x], expected) << "plane=" << plane << " x=" << x << " y=" << y;
    }
  }
}

TEST(Limiter, ClampsEightBitLumaAndChromaForYuv444) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa1, PLANAR_Y);
  fill_plane_full_pitch(source, 0xb2, PLANAR_U);
  fill_plane_full_pitch(source, 0xc3, PLANAR_V);
  const std::array<std::uint8_t, 8> luma_values{0, 15, 16, 64, 128, 235, 236, 255};
  const std::array<std::uint8_t, 8> chroma_values{0, 15, 16, 64, 128, 240, 241, 255};
  write_values<std::uint8_t>(source, PLANAR_Y, vi.width, vi.height, luma_values);
  write_values<std::uint8_t>(source, PLANAR_U, vi.width, vi.height, chroma_values);
  write_values<std::uint8_t>(source, PLANAR_V, vi.width, vi.height, chroma_values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Limiter filter(clip, 16.0f, 235.0f, 16.0f, 240.0f, 0, false, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  expect_clamped_plane<std::uint8_t>(source, output, PLANAR_Y, vi.width, vi.height, 16, 235);
  expect_clamped_plane<std::uint8_t>(source, output, PLANAR_U, vi.width, vi.height, 16, 240);
  expect_clamped_plane<std::uint8_t>(source, output, PLANAR_V, vi.width, vi.height, 16, 240);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(Limiter, ScalesLimitsForTenBitYuv444WhenParamScaleIsEnabled) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_YUV444P10, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd4, PLANAR_Y);
  fill_plane_full_pitch(source, 0xe5, PLANAR_U);
  fill_plane_full_pitch(source, 0xf6, PLANAR_V);
  const std::array<std::uint16_t, 8> luma_values{0, 63, 64, 256, 512, 940, 941, 1023};
  const std::array<std::uint16_t, 8> chroma_values{0, 63, 64, 256, 512, 960, 961, 1023};
  write_values<std::uint16_t>(source, PLANAR_Y, vi.width, vi.height, luma_values);
  write_values<std::uint16_t>(source, PLANAR_U, vi.width, vi.height, chroma_values);
  write_values<std::uint16_t>(source, PLANAR_V, vi.width, vi.height, chroma_values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Limiter filter(clip, 16.0f, 235.0f, 16.0f, 240.0f, 0, true, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  expect_clamped_plane<std::uint16_t>(source, output, PLANAR_Y, vi.width, vi.height, 64, 940);
  expect_clamped_plane<std::uint16_t>(source, output, PLANAR_U, vi.width, vi.height, 64, 960);
  expect_clamped_plane<std::uint16_t>(source, output, PLANAR_V, vi.width, vi.height, 64, 960);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
