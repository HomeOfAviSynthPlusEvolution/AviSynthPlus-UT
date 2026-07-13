#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_TURN_UNDEF_AVS_UNUSED
#endif
#include "filters/turn.h"
#ifdef AVSUT_TURN_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_TURN_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;

constexpr int kTurnLeft = 0;
constexpr int kTurnRight = 1;
constexpr int kTurn180 = 2;

const char* direction_name(int direction) {
  switch (direction) {
    case kTurnLeft:
      return "Left";
    case kTurnRight:
      return "Right";
    case kTurn180:
      return "Half";
    default:
      return "Unknown";
  }
}

void fill_pattern(PVideoFrame& frame, int plane, int width, int height, std::uint8_t base) {
  const int pitch = frame->GetPitch(plane);
  for (int y = 0; y < height; ++y) {
    auto* row = frame->GetWritePtr(plane) + y * pitch;
    for (int x = 0; x < width; ++x) {
      row[x] = static_cast<std::uint8_t>(base + x * 17 + y * 31);
    }
  }
}

void expect_rotated_plane(const PVideoFrame& source, const PVideoFrame& output, int plane,
                          int direction, int width, int height) {
  const int destination_width = direction == kTurn180 ? width : height;
  const int destination_height = direction == kTurn180 ? height : width;
  ASSERT_EQ(output->GetRowSize(plane), destination_width);
  ASSERT_EQ(output->GetHeight(plane), destination_height);

  const int source_pitch = source->GetPitch(plane);
  const int output_pitch = output->GetPitch(plane);
  for (int destination_y = 0; destination_y < destination_height; ++destination_y) {
    const auto* output_row = output->GetReadPtr(plane) + destination_y * output_pitch;
    for (int destination_x = 0; destination_x < destination_width; ++destination_x) {
      int source_x = 0;
      int source_y = 0;
      if (direction == kTurnLeft) {
        source_x = width - 1 - destination_y;
        source_y = destination_x;
      } else if (direction == kTurnRight) {
        source_x = destination_y;
        source_y = height - 1 - destination_x;
      } else {
        source_x = width - 1 - destination_x;
        source_y = height - 1 - destination_y;
      }
      const auto* source_row = source->GetReadPtr(plane) + source_y * source_pitch;
      EXPECT_EQ(output_row[destination_x], source_row[source_x])
          << "direction=" << direction_name(direction) << " plane=" << plane
          << " x=" << destination_x << " y=" << destination_y;
    }
  }
}

class TurnFilterTest : public ::testing::TestWithParam<int> {};

TEST_P(TurnFilterTest, RotatesAllYuv444PlanesWithExpectedGeometry) {
  const int direction = GetParam();
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa1, PLANAR_Y);
  fill_plane_full_pitch(source, 0xb2, PLANAR_U);
  fill_plane_full_pitch(source, 0xc3, PLANAR_V);
  fill_pattern(source, PLANAR_Y, width, height, 3);
  fill_pattern(source, PLANAR_U, width, height, 67);
  fill_pattern(source, PLANAR_V, width, height, 131);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Turn filter(clip, direction, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    expect_rotated_plane(source, output, plane, direction, width, height);
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

INSTANTIATE_TEST_SUITE_P(Directions, TurnFilterTest,
                         ::testing::Values(kTurnLeft, kTurnRight, kTurn180),
                         [](const ::testing::TestParamInfo<int>& info) {
                           return direction_name(info.param);
                         });

}  // namespace
