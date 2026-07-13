#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_RESIZE_FILTER_UNDEF_AVS_UNUSED
#endif
#include "filters/resize.h"
#include "filters/resample.h"
#ifdef AVSUT_RESIZE_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_RESIZE_FILTER_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
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
    fill_plane_full_pitch(frame, static_cast<std::uint8_t>(0xa0 + plane_index), plane);
    const int pitch = frame->GetPitch(plane);
    const int width = frame->GetRowSize(plane);
    const int height = frame->GetHeight(plane);
    for (int y = 0; y < height; ++y) {
      auto* row = frame->GetWritePtr(plane) + y * pitch;
      for (int x = 0; x < width; ++x) {
        row[x] = static_cast<std::uint8_t>(bases[plane_index] + x * 19 + y * 37);
      }
    }
  }
}

std::uint8_t vertical_reduce_reference(const PVideoFrame& source, int plane, int x, int y,
                                       int output_height) {
  const int pitch = source->GetPitch(plane);
  const auto* first = source->GetReadPtr(plane) + (2 * y) * pitch;
  if (y + 1 == output_height) {
    const auto* second = first + pitch;
    return static_cast<std::uint8_t>((first[x] + 3 * second[x] + 2) / 4);
  }
  const auto* second = first + pitch;
  const auto* third = second + pitch;
  return static_cast<std::uint8_t>((first[x] + 2 * second[x] + third[x] + 2) / 4);
}

std::uint8_t horizontal_reduce_reference(const PVideoFrame& source, int plane, int x, int y,
                                         int output_width) {
  const int pitch = source->GetPitch(plane);
  const auto* row = source->GetReadPtr(plane) + y * pitch;
  if (x + 1 == output_width) {
    return static_cast<std::uint8_t>((row[2 * x] + row[2 * x + 1] + 1) / 2);
  }
  return static_cast<std::uint8_t>((row[2 * x] + 2 * row[2 * x + 1] + row[2 * x + 2] + 2) / 4);
}

TEST(ReduceBy2Filter, ReducesYv24VerticallyWithFinalRowWeight) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int source_height = 6;
  const auto vi =
      make_video_info(VideoInfoSpec{width, source_height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  VerticalReduceBy2 filter(clip, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetHeight(), source_height / 2);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    ASSERT_EQ(output->GetRowSize(plane), width);
    for (int y = 0; y < output->GetHeight(plane); ++y) {
      const auto* row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < width; ++x) {
        EXPECT_EQ(row[x], vertical_reduce_reference(source, plane, x, y, output->GetHeight(plane)))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(ReduceBy2Filter, ReducesYv24HorizontallyWithFinalColumnWeight) {
  AviSynthEnvironment environment;
  constexpr int source_width = 8;
  constexpr int height = 3;
  const auto vi =
      make_video_info(VideoInfoSpec{source_width, height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  HorizontalReduceBy2 filter(clip, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), source_width / 2);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    ASSERT_EQ(output->GetRowSize(plane), source_width / 2);
    for (int y = 0; y < height; ++y) {
      const auto* row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < source_width / 2; ++x) {
        EXPECT_EQ(row[x], horizontal_reduce_reference(source, plane, x, y, source_width / 2))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

std::uint8_t point_resize_reference(const PVideoFrame& source, int plane, int source_width,
                                    int source_height, int target_width, int target_height, int x,
                                    int y) {
  const int source_x =
      std::clamp(static_cast<int>(std::floor(static_cast<double>(x) * source_width / target_width)),
                 0, source_width - 1);
  const int source_y = std::clamp(
      static_cast<int>(std::floor(static_cast<double>(y) * source_height / target_height)), 0,
      source_height - 1);
  const auto* row = source->GetReadPtr(plane) + source_y * source->GetPitch(plane);
  return row[source_x];
}

TEST(FilteredResizeFilter, PointResizeHorizontalUsesNearestSourceCoordinates) {
  AviSynthEnvironment environment;
  constexpr int source_width = 7;
  constexpr int source_height = 4;
  constexpr int target_width = 4;
  const auto vi =
      make_video_info(VideoInfoSpec{source_width, source_height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);
  PointFilter point_filter;

  FilteredResizeH filter(clip, 0.0, static_cast<double>(source_width), target_width, &point_filter,
                         false, -1, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), target_width);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    for (int y = 0; y < source_height; ++y) {
      const auto* row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < target_width; ++x) {
        EXPECT_EQ(row[x], point_resize_reference(source, plane, source_width, source_height,
                                                 target_width, source_height, x, y))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(FilteredResizeFilter, PointResizeVerticalUsesNearestSourceCoordinates) {
  AviSynthEnvironment environment;
  constexpr int source_width = 5;
  constexpr int source_height = 7;
  constexpr int target_height = 4;
  const auto vi =
      make_video_info(VideoInfoSpec{source_width, source_height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yv24_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);
  PointFilter point_filter;

  FilteredResizeV filter(clip, 0.0, static_cast<double>(source_height), target_height,
                         &point_filter, false, -1, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetHeight(), target_height);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    for (int y = 0; y < target_height; ++y) {
      const auto* row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < source_width; ++x) {
        EXPECT_EQ(row[x], point_resize_reference(source, plane, source_width, source_height,
                                                 source_width, target_height, x, y))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
