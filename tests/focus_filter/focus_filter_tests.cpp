#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_FOCUS_UNDEF_AVS_UNUSED
#endif
#include "filters/focus.h"
#ifdef AVSUT_FOCUS_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_FOCUS_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSequenceClip;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;

void fill_yuy2_pattern(PVideoFrame& frame) {
  const int pitch = frame->GetPitch();
  const int row_size = frame->GetRowSize();
  for (int y = 0; y < frame->GetHeight(); ++y) {
    auto* row = frame->GetWritePtr() + y * pitch;
    for (int x = 0; x < row_size; ++x) {
      const int group = x / 4;
      if ((x & 1) == 0) {
        row[x] = static_cast<std::uint8_t>(32 + group * 17 + y * 9 + (x & 2) * 3);
      } else if ((x & 2) == 0) {
        row[x] = static_cast<std::uint8_t>(96 + group * 5 + y * 7);
      } else {
        row[x] = static_cast<std::uint8_t>(160 + group * 3 + y * 11);
      }
    }
  }
}

bool close_sample(int first, int second, unsigned threshold) {
  return std::abs(first - second) <= static_cast<int>(threshold);
}

std::vector<std::uint8_t> spatial_reference(const PVideoFrame& source, int radius,
                                            unsigned luma_threshold, unsigned chroma_threshold) {
  const int row_size = source->GetRowSize();
  const int height = source->GetHeight();
  const int pitch = source->GetPitch();
  const int diameter = radius * 2 + 1;
  const int edge = (diameter + 1) & -4;
  std::vector<std::uint8_t> expected(static_cast<std::size_t>(row_size) * height);
  for (int y = 0; y < height; ++y) {
    const auto* source_row = source->GetReadPtr() + y * pitch;
    auto* expected_row = expected.data() + static_cast<std::size_t>(y) * row_size;
    std::copy_n(source_row, row_size, expected_row);
    for (int x = edge; x < row_size - edge; x += 2) {
      const int reference_group = x | 3;
      const int current_y = source_row[x];
      const int current_u = source_row[reference_group - 2];
      const int current_v = source_row[reference_group];
      int count = 0;
      int sum_y = 0;
      int sum_u = 0;
      int sum_v = 0;
      for (int h = 0; h < diameter; ++h) {
        const int source_y = std::clamp(y + h - (diameter >> 1), 0, height - 1);
        const auto* candidate_row = source->GetReadPtr() + source_y * pitch;
        for (int w = -diameter + 1; w < diameter; w += 2) {
          const int candidate_group = (x + w) | 3;
          if (close_sample(candidate_row[x + w], current_y, luma_threshold) &&
              close_sample(candidate_row[candidate_group - 2], current_u, chroma_threshold) &&
              close_sample(candidate_row[candidate_group], current_v, chroma_threshold)) {
            ++count;
            sum_y += candidate_row[x + w];
            sum_u += candidate_row[candidate_group - 2];
            sum_v += candidate_row[candidate_group];
          }
        }
      }
      if (count == 0) {
        throw std::logic_error("spatial soften reference had no matching sample");
      }
      expected_row[x] = static_cast<std::uint8_t>((sum_y + (count >> 1)) / count);
      if ((x & 3) == 0) {
        expected_row[x + 1] = static_cast<std::uint8_t>((sum_u + (count >> 1)) / count);
        expected_row[x + 3] = static_cast<std::uint8_t>((sum_v + (count >> 1)) / count);
      }
    }
  }
  return expected;
}

TEST(SpatialSoftenFilter, AppliesJointYuvThresholdWithReplicatedEdges) {
  AviSynthEnvironment environment;
  constexpr int width = 8;
  constexpr int height = 3;
  constexpr int radius = 1;
  constexpr unsigned luma_threshold = 5;
  constexpr unsigned chroma_threshold = 7;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YUY2, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_yuy2_pattern(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  const auto expected = spatial_reference(source, radius, luma_threshold, chroma_threshold);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  SpatialSoften filter(clip, radius, luma_threshold, chroma_threshold, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(), vi.RowSize());
  for (int y = 0; y < height; ++y) {
    const auto* output_row = output->GetReadPtr() + y * output->GetPitch();
    const auto* expected_row = expected.data() + static_cast<std::size_t>(y) * vi.RowSize();
    for (int x = 0; x < vi.RowSize(); ++x) {
      EXPECT_EQ(output_row[x], expected_row[x]) << "x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

std::vector<PVideoFrame> make_temporal_frames(AviSynthEnvironment& environment, const VideoInfo& vi,
                                              unsigned luma_threshold, unsigned chroma_threshold) {
  std::vector<PVideoFrame> frames;
  for (int frame_index = 0; frame_index < 3; ++frame_index) {
    PVideoFrame frame = environment.get()->NewVideoFrame(vi);
    for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
      fill_plane_full_pitch(frame, static_cast<std::uint8_t>(0xe0 + plane * 7 + frame_index),
                            plane);
      const unsigned threshold = plane == PLANAR_Y ? luma_threshold : chroma_threshold;
      const int width = frame->GetRowSize(plane);
      const int height = frame->GetHeight(plane);
      const int pitch = frame->GetPitch(plane);
      for (int y = 0; y < height; ++y) {
        auto* row = frame->GetWritePtr(plane) + y * pitch;
        for (int x = 0; x < width; ++x) {
          const int center = 64 + plane * 41 + x * 3 + y * 5;
          int delta = 0;
          if (frame_index == 0) {
            delta = ((x + plane) % 3 == 0) ? static_cast<int>(threshold)
                                           : static_cast<int>(threshold) + 1;
          } else if (frame_index == 2) {
            delta = ((x + plane) % 4 == 0) ? -static_cast<int>(threshold)
                                           : -static_cast<int>(threshold) - 2;
          }
          row[x] = static_cast<std::uint8_t>(center + delta);
        }
      }
    }
    frames.push_back(frame);
  }
  return frames;
}

std::uint8_t temporal_reference_sample(std::uint8_t current, std::uint8_t previous,
                                       std::uint8_t next, unsigned threshold) {
  int sum = current;
  sum += close_sample(current, previous, threshold) ? previous : current;
  sum += close_sample(current, next, threshold) ? next : current;
  constexpr int divisor = 32768 / 3;
  return static_cast<std::uint8_t>((sum * divisor + 16384) >> 15);
}

TEST(TemporalSoftenFilter, AveragesThresholdMatchesAcrossFrameSequence) {
  AviSynthEnvironment environment;
  constexpr int width = 17;
  constexpr int height = 3;
  constexpr unsigned luma_threshold = 10;
  constexpr unsigned chroma_threshold = 7;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 3, 25, 1});
  auto frames = make_temporal_frames(environment, vi, luma_threshold, chroma_threshold);
  std::vector<FrameSnapshot> snapshots;
  for (const auto& frame : frames) {
    snapshots.push_back(FrameSnapshot::capture(frame, vi));
  }
  auto* source_clip = new FrameSequenceClip(vi, frames);
  const PClip clip(source_clip);

  TemporalSoften filter(clip, 1, luma_threshold, chroma_threshold, 0, environment.get());
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const unsigned threshold = plane == PLANAR_Y ? luma_threshold : chroma_threshold;
    const int width_bytes = output->GetRowSize(plane);
    const int height_plane = output->GetHeight(plane);
    const int output_pitch = output->GetPitch(plane);
    const int previous_pitch = frames[0]->GetPitch(plane);
    const int current_pitch = frames[1]->GetPitch(plane);
    const int next_pitch = frames[2]->GetPitch(plane);
    for (int y = 0; y < height_plane; ++y) {
      const auto* output_row = output->GetReadPtr(plane) + y * output_pitch;
      const auto* previous_row = frames[0]->GetReadPtr(plane) + y * previous_pitch;
      const auto* current_row = frames[1]->GetReadPtr(plane) + y * current_pitch;
      const auto* next_row = frames[2]->GetReadPtr(plane) + y * next_pitch;
      for (int x = 0; x < width_bytes; ++x) {
        EXPECT_EQ(output_row[x], temporal_reference_sample(current_row[x], previous_row[x],
                                                           next_row[x], threshold))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>({0, 1, 2}));
  ASSERT_EQ(source_clip->cache_hint_requests().size(), 1U);
  const avsut::test::CacheHintRequest expected_hint{CACHE_WINDOW, 3};
  EXPECT_EQ(source_clip->cache_hint_requests()[0], expected_hint);
  for (std::size_t i = 0; i < frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(frames[i], vi), snapshots[i]);
  }
}

}  // namespace
