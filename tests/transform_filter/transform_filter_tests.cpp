#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_TRANSFORM_FILTER_UNDEF_AVS_UNUSED
#endif
#include "filters/transform.h"
#ifdef AVSUT_TRANSFORM_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_TRANSFORM_FILTER_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

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

void fill_yv24_pattern(PVideoFrame& frame) {
  constexpr std::array<std::uint8_t, 3> bases{3, 67, 131};
  for (std::size_t plane_index = 0; plane_index < bases.size(); ++plane_index) {
    const int plane = static_cast<int>(PLANAR_Y + plane_index);
    fill_plane_full_pitch(frame, static_cast<std::uint8_t>(0xb0 + plane_index), plane);
    const int pitch = frame->GetPitch(plane);
    const int width = frame->GetRowSize(plane);
    const int height = frame->GetHeight(plane);
    for (int y = 0; y < height; ++y) {
      auto* row = frame->GetWritePtr(plane) + y * pitch;
      for (int x = 0; x < width; ++x) {
        row[x] = static_cast<std::uint8_t>(bases[plane_index] + x * 17 + y * 31);
      }
    }
  }
}

TEST(FlipFilter, FlipsYv24VerticallyAcrossAllPlanes) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  FlipVertical filter(clip);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int pitch = source->GetPitch(plane);
    const int output_pitch = output->GetPitch(plane);
    for (int y = 0; y < height; ++y) {
      const auto* source_row = source->GetReadPtr(plane) + (height - 1 - y) * pitch;
      const auto* output_row = output->GetReadPtr(plane) + y * output_pitch;
      for (int x = 0; x < width; ++x) {
        EXPECT_EQ(output_row[x], source_row[x]) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(FlipFilter, FlipsYv24HorizontallyAcrossAllPlanes) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  FlipHorizontal filter(clip);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int pitch = source->GetPitch(plane);
    const int output_pitch = output->GetPitch(plane);
    for (int y = 0; y < height; ++y) {
      const auto* source_row = source->GetReadPtr(plane) + y * pitch;
      const auto* output_row = output->GetReadPtr(plane) + y * output_pitch;
      for (int x = 0; x < width; ++x) {
        EXPECT_EQ(output_row[x], source_row[width - 1 - x])
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(CropFilter, ReturnsRequestedYv24Subrectangle) {
  AviSynthEnvironment environment;
  constexpr int source_width = 8;
  constexpr int source_height = 6;
  constexpr int left = 2;
  constexpr int top = 2;
  constexpr int width = 4;
  constexpr int height = 2;
  const auto vi =
      make_video_info(VideoInfoSpec{source_width, source_height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Crop filter(left, top, width, height, true, clip, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), width);
  ASSERT_EQ(output->GetHeight(PLANAR_Y), height);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int source_pitch = source->GetPitch(plane);
    const int output_pitch = output->GetPitch(plane);
    for (int y = 0; y < height; ++y) {
      const auto* source_row = source->GetReadPtr(plane) + (top + y) * source_pitch + left;
      const auto* output_row = output->GetReadPtr(plane) + y * output_pitch;
      for (int x = 0; x < width; ++x) {
        EXPECT_EQ(output_row[x], source_row[x]) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(AddBordersFilter, AddsExplicitYuvColorAroundYv24Source) {
  AviSynthEnvironment environment;
  constexpr int source_width = 5;
  constexpr int source_height = 3;
  constexpr int left = 2;
  constexpr int top = 2;
  constexpr int right = 2;
  constexpr int bottom = 2;
  constexpr std::uint8_t border_y = 18;
  constexpr std::uint8_t border_u = 52;
  constexpr std::uint8_t border_v = 86;
  const auto vi =
      make_video_info(VideoInfoSpec{source_width, source_height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  const int yuv_color = (static_cast<int>(border_y) << 16) | (static_cast<int>(border_u) << 8) |
                        static_cast<int>(border_v);
  AddBorders filter(left, top, right, bottom, yuv_color, true, clip, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), source_width + left + right);
  ASSERT_EQ(output->GetHeight(PLANAR_Y), source_height + top + bottom);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const std::uint8_t border = plane == PLANAR_Y   ? border_y
                                : plane == PLANAR_U ? border_u
                                                    : border_v;
    const int output_width = output->GetRowSize(plane);
    const int output_height = output->GetHeight(plane);
    const int source_pitch = source->GetPitch(plane);
    const int output_pitch = output->GetPitch(plane);
    for (int y = 0; y < output_height; ++y) {
      const auto* output_row = output->GetReadPtr(plane) + y * output_pitch;
      for (int x = 0; x < output_width; ++x) {
        const bool inside =
            y >= top && y < top + source_height && x >= left && x < left + source_width;
        const auto expected =
            inside ? source->GetReadPtr(plane)[(y - top) * source_pitch + x - left] : border;
        EXPECT_EQ(output_row[x], expected) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
