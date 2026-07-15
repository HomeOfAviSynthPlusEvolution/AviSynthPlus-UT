#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_LEVELS_UNDEF_AVS_UNUSED
#endif
#include "filters/levels.h"
#ifdef AVSUT_LEVELS_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_LEVELS_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;
using avsut::test::write_frame_plane;

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

TEST(Levels, MapsEightBitLumaWithClamping) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_Y8, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa7, PLANAR_Y);
  write_values<std::uint8_t>(source, PLANAR_Y, vi.width, vi.height,
                             std::array<std::uint8_t, 8>{0, 16, 32, 64, 128, 200, 235, 255});
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Levels filter(clip, 16.0f, 1.0, 235.0f, 0.0f, 255.0f, false, false, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), vi.width);
  ASSERT_EQ(output->GetHeight(PLANAR_Y), vi.height);
  for (int y = 0; y < vi.height; ++y) {
    const auto* output_row = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* source_row = source->GetReadPtr(PLANAR_Y) + y * source->GetPitch(PLANAR_Y);
    for (int x = 0; x < vi.width; ++x) {
      const int input = source_row[x];
      const int expected = std::clamp(static_cast<int>((input - 16) * 255.0 / 219.0 + 0.5), 0, 255);
      EXPECT_EQ(output_row[x], expected) << "x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(Levels, MapsLumaAndChromaIndependentlyForYuv444) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0x91, PLANAR_Y);
  fill_plane_full_pitch(source, 0x82, PLANAR_U);
  fill_plane_full_pitch(source, 0x73, PLANAR_V);
  const std::array<std::uint8_t, 8> values{0, 16, 64, 96, 128, 192, 240, 255};
  write_values<std::uint8_t>(source, PLANAR_Y, vi.width, vi.height, values);
  write_values<std::uint8_t>(source, PLANAR_U, vi.width, vi.height, values);
  write_values<std::uint8_t>(source, PLANAR_V, vi.width, vi.height, values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Levels filter(clip, 16.0f, 1.0, 235.0f, 0.0f, 255.0f, true, false, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int pitch = output->GetPitch(plane);
    const int source_pitch = source->GetPitch(plane);
    const auto* output_base = output->GetReadPtr(plane);
    const auto* source_base = source->GetReadPtr(plane);
    for (int y = 0; y < vi.height; ++y) {
      const auto* output_row = output_base + y * pitch;
      const auto* source_row = source_base + y * source_pitch;
      for (int x = 0; x < vi.width; ++x) {
        const int input = source_row[x];
        const int expected =
            plane == PLANAR_Y
                ? std::clamp(static_cast<int>((input - 16) * 255.0 / 219.0 + 0.5), 16, 235)
                : std::clamp(static_cast<int>((input - 128) * 255.0 / 219.0 + 128.5), 16, 240);
        EXPECT_EQ(output_row[x], expected) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(Levels, MapsSixteenBitYuv420WithIndependentLumaAndChromaRanges) {
  AviSynthEnvironment environment;
  constexpr int width = 8;
  constexpr int height = 6;
  const auto vi = make_video_info(
      VideoInfoSpec{width, height, VideoInfo::CS_YUV420P16, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    fill_plane_full_pitch(source, static_cast<std::uint8_t>(0x70 + plane * 0x13), plane);
    write_frame_plane<std::uint16_t>(source, plane, [](int x, int y) {
      return 2048 + x * 8192 + y * 4096;
    });
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Levels filter(clip, 8192.0f, 1.0, 57344.0f, 4096.0f, 61440.0f, false, false,
                environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  const auto expected_luma = [](std::uint16_t value) {
    const double normalized = std::clamp((static_cast<double>(value) - 8192.0) / 49152.0,
                                         0.0, 1.0);
    const double mapped = normalized * 57344.0 + 4096.0;
    return static_cast<std::uint16_t>(static_cast<int>(mapped + 0.5));
  };
  const auto expected_chroma = [](std::uint16_t value) {
    const double mapped = (static_cast<double>(value) - 32768.0) * 57344.0 / 49152.0 + 32768.0;
    return static_cast<std::uint16_t>(std::clamp(static_cast<int>(mapped + 0.5), 0, 65535));
  };
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int plane_width = output->GetRowSize(plane) / static_cast<int>(sizeof(std::uint16_t));
    const int plane_height = output->GetHeight(plane);
    const int source_pitch = source->GetPitch(plane);
    const int output_pitch = output->GetPitch(plane);
    for (int y = 0; y < plane_height; ++y) {
      const auto* source_row = reinterpret_cast<const std::uint16_t*>(
          source->GetReadPtr(plane) + y * source_pitch);
      const auto* output_row = reinterpret_cast<const std::uint16_t*>(
          output->GetReadPtr(plane) + y * output_pitch);
      for (int x = 0; x < plane_width; ++x) {
        const auto expected = plane == PLANAR_Y ? expected_luma(source_row[x])
                                                : expected_chroma(source_row[x]);
        EXPECT_EQ(output_row[x], expected) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_EQ(output->GetRowSize(PLANAR_U), width / 2 * static_cast<int>(sizeof(std::uint16_t)));
  EXPECT_EQ(output->GetHeight(PLANAR_U), height / 2);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(Levels, AppliesGammaToFloatYuv444AndPreservesChromaCenter) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 2;
  const auto vi = make_video_info(
      VideoInfoSpec{width, height, VideoInfo::CS_YUV444PS, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  const std::array<float, 5> luma{0.0F, 0.04F, 0.25F, 0.64F, 1.0F};
  const std::array<float, 5> chroma{-0.6F, -0.25F, 0.0F, 0.25F, 0.6F};
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    fill_plane_full_pitch(source, static_cast<std::uint8_t>(0x90 + plane * 0x11), plane);
    write_frame_plane<float>(source, plane, [plane, &luma, &chroma](int x, int) {
      return plane == PLANAR_Y ? luma[static_cast<std::size_t>(x)]
                                : chroma[static_cast<std::size_t>(x)];
    });
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Levels filter(clip, 0.0f, 2.0, 1.0f, 0.0f, 1.0f, false, false, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int pitch = output->GetPitch(plane);
    const auto* output_row = reinterpret_cast<const float*>(output->GetReadPtr(plane));
    for (int x = 0; x < width; ++x) {
      const float input = plane == PLANAR_Y ? luma[static_cast<std::size_t>(x)]
                                            : chroma[static_cast<std::size_t>(x)];
      const float expected = plane == PLANAR_Y
                                 ? std::sqrt(std::clamp(input, 0.0F, 1.0F))
                                 : std::clamp(input, -0.5F, 0.5F);
      EXPECT_NEAR(output_row[x], expected, 1.0e-5F) << "plane=" << plane << " x=" << x;
    }
    EXPECT_EQ(pitch >= output->GetRowSize(plane), true);
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
