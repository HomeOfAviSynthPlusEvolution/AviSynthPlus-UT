#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_FIELD_FILTER_UNDEF_AVS_UNUSED
#endif
#include "filters/field.h"
#ifdef AVSUT_FIELD_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_FIELD_FILTER_UNDEF_AVS_UNUSED
#endif

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_EDIT_FILTER_UNDEF_AVS_UNUSED
#endif
#include "filters/edit.h"
#ifdef AVSUT_EDIT_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_EDIT_FILTER_UNDEF_AVS_UNUSED
#endif
#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSequenceClip;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::video_frame_planes;
using avsut::test::VideoInfoSpec;
using avsut::test::write_frame_plane;

struct FieldFormatCase {
  int pixel_type;
  int width;
  int height;
  bool top_field_first;
  const char* name;
};

void PrintTo(const FieldFormatCase& test_case, std::ostream* stream) { *stream << test_case.name; }

std::vector<PVideoFrame> make_field_source_frames(AviSynthEnvironment& environment,
                                                  const VideoInfo& video_info, int frame_count) {
  std::vector<PVideoFrame> frames;
  frames.reserve(static_cast<std::size_t>(frame_count));
  for (int frame_index = 0; frame_index < frame_count; ++frame_index) {
    PVideoFrame frame = environment.get()->NewVideoFrame(video_info);
    for (const int plane : video_frame_planes(video_info)) {
      fill_plane_full_pitch(frame, static_cast<std::uint8_t>(0x80 + plane * 17 + frame_index),
                            plane);
      write_frame_plane<std::uint8_t>(frame, plane, [plane, frame_index](int x, int y) {
        return static_cast<std::uint8_t>(19 + plane * 31 + frame_index * 67 + y * 23 + x * 5);
      });
    }
    frames.push_back(frame);
  }
  return frames;
}

void expect_separated_field(const PVideoFrame& output, const PVideoFrame& source, bool top_field,
                            const char* format_name, int output_index) {
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    ASSERT_EQ(output->GetRowSize(plane), source->GetRowSize(plane))
        << "format=" << format_name << " plane=" << plane;
    ASSERT_EQ(output->GetHeight(plane), source->GetHeight(plane) / 2)
        << "format=" << format_name << " plane=" << plane;
    for (int y = 0; y < output->GetHeight(plane); ++y) {
      const int source_y = 2 * y + (top_field ? 0 : 1);
      const auto* source_row = source->GetReadPtr(plane) + source_y * source->GetPitch(plane);
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < output->GetRowSize(plane); ++x) {
        EXPECT_EQ(output_row[x], source_row[x])
            << "format=" << format_name << " output_frame=" << output_index << " plane=" << plane
            << " x=" << x << " y=" << y;
      }
    }
  }
}

void expect_double_weave_fields_frame(const PVideoFrame& output,
                                      const std::vector<PVideoFrame>& source_frames,
                                      int first_field, bool first_on_even,
                                      const char* format_name) {
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int output_height = output->GetHeight(plane);
    ASSERT_EQ(output->GetRowSize(plane), source_frames[0]->GetRowSize(plane))
        << "format=" << format_name << " plane=" << plane;
    ASSERT_EQ(output_height, source_frames[0]->GetHeight(plane))
        << "format=" << format_name << " plane=" << plane;
    for (int y = 0; y < output_height; ++y) {
      const bool use_first_field = ((y & 1) == 0) == first_on_even;
      const int field_index = use_first_field ? first_field : first_field + 1;
      const int source_frame_index = field_index / 2;
      const int source_y = 2 * (y / 2) + (field_index & 1);
      const auto* source_row =
          source_frames[static_cast<std::size_t>(source_frame_index)]->GetReadPtr(plane) +
          source_y * source_frames[static_cast<std::size_t>(source_frame_index)]->GetPitch(plane);
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < output->GetRowSize(plane); ++x) {
        EXPECT_EQ(output_row[x], source_row[x])
            << "format=" << format_name << " first_field=" << first_field << " plane=" << plane
            << " x=" << x << " y=" << y;
      }
    }
  }
}

void expect_double_weave_frames_frame(const PVideoFrame& output,
                                      const std::vector<PVideoFrame>& source_frames,
                                      int first_frame, bool first_on_even,
                                      const char* format_name) {
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    ASSERT_EQ(output->GetRowSize(plane), source_frames[0]->GetRowSize(plane))
        << "format=" << format_name << " plane=" << plane;
    ASSERT_EQ(output->GetHeight(plane), source_frames[0]->GetHeight(plane))
        << "format=" << format_name << " plane=" << plane;
    for (int y = 0; y < output->GetHeight(plane); ++y) {
      const int source_frame_index =
          (((y & 1) == 0) == first_on_even) ? first_frame : first_frame + 1;
      const auto* source_row =
          source_frames[static_cast<std::size_t>(source_frame_index)]->GetReadPtr(plane) +
          y * source_frames[static_cast<std::size_t>(source_frame_index)]->GetPitch(plane);
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < output->GetRowSize(plane); ++x) {
        EXPECT_EQ(output_row[x], source_row[x])
            << "format=" << format_name << " first_frame=" << first_frame << " plane=" << plane
            << " x=" << x << " y=" << y;
      }
    }
  }
}

void expect_frame_equal(const PVideoFrame& output, const PVideoFrame& expected,
                        const char* format_name) {
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    ASSERT_EQ(output->GetRowSize(plane), expected->GetRowSize(plane))
        << "format=" << format_name << " plane=" << plane;
    ASSERT_EQ(output->GetHeight(plane), expected->GetHeight(plane))
        << "format=" << format_name << " plane=" << plane;
    for (int y = 0; y < output->GetHeight(plane); ++y) {
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      const auto* expected_row = expected->GetReadPtr(plane) + y * expected->GetPitch(plane);
      for (int x = 0; x < output->GetRowSize(plane); ++x) {
        EXPECT_EQ(output_row[x], expected_row[x])
            << "format=" << format_name << " plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
}

class SeparateFieldsTest : public ::testing::TestWithParam<FieldFormatCase> {};

TEST_P(SeparateFieldsTest, SplitsRowsAndPublishesFieldMetadata) {
  const auto& test_case = GetParam();
  AviSynthEnvironment environment;
  const auto source_vi = make_video_info(
      VideoInfoSpec{test_case.width, test_case.height, test_case.pixel_type, 2, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);
  const PClip parity_source(new AssumeParity(source, test_case.top_field_first));

  SeparateFields filter(parity_source, environment.get());
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, source_vi.width);
  EXPECT_EQ(output_vi.height, source_vi.height / 2);
  EXPECT_EQ(output_vi.num_frames, source_vi.num_frames * 2);
  EXPECT_EQ(output_vi.fps_numerator, 50U);
  EXPECT_EQ(output_vi.fps_denominator, 1U);
  EXPECT_TRUE(output_vi.IsFieldBased());
  EXPECT_EQ(output_vi.IsTFF(), test_case.top_field_first);
  EXPECT_EQ(output_vi.IsBFF(), !test_case.top_field_first);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < output_vi.num_frames; ++n) {
    EXPECT_EQ(filter.GetParity(n), test_case.top_field_first ^ ((n & 1) != 0))
        << "format=" << test_case.name << " output_frame=" << n;
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_separated_field(output, source_frames[static_cast<std::size_t>(n / 2)],
                           test_case.top_field_first ^ ((n & 1) != 0), test_case.name, n);
    EXPECT_NE(output->CheckMemory(), 1) << "format=" << test_case.name << " output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 0, 1, 1}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "format=" << test_case.name << " source_frame=" << i;
  }
}

INSTANTIATE_TEST_SUITE_P(
    FormatAndParity, SeparateFieldsTest,
    ::testing::Values(FieldFormatCase{VideoInfo::CS_YV12, 6, 8, true, "Yv12_Tff_Width6_Height8"},
                      FieldFormatCase{VideoInfo::CS_YV12, 6, 8, false, "Yv12_Bff_Width6_Height8"},
                      FieldFormatCase{VideoInfo::CS_YV24, 5, 6, true, "Yv24_Tff_Width5_Height6"},
                      FieldFormatCase{VideoInfo::CS_YV24, 5, 6, false, "Yv24_Bff_Width5_Height6"}),
    [](const ::testing::TestParamInfo<FieldFormatCase>& info) { return info.param.name; });

TEST(FieldFilter, OverlapsAdjacentFieldsThroughDoubleWeaveFields) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 6;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);
  const PClip tff_source(new AssumeParity(source, true));
  const PClip field_clip(new SeparateFields(tff_source, environment.get()));

  DoubleWeaveFields filter(field_clip);
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, width);
  EXPECT_EQ(output_vi.height, height);
  EXPECT_EQ(output_vi.num_frames, 4);
  EXPECT_EQ(output_vi.fps_numerator, 50U);
  EXPECT_EQ(output_vi.fps_denominator, 1U);
  EXPECT_FALSE(output_vi.IsFieldBased());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < 3; ++n) {
    EXPECT_EQ(filter.GetParity(n), (n & 1) == 0) << "output_frame=" << n;
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_double_weave_fields_frame(output, source_frames, n, (n & 1) == 0, "Yv24");
    EXPECT_NE(output->CheckMemory(), 1) << "output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 0, 0, 1, 1, 1}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(FieldFilter, DoubleWeaveFieldsServesLastAdvertisedFrame) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 6;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);
  const PClip field_clip(new SeparateFields(new AssumeParity(source, true), environment.get()));

  DoubleWeaveFields filter(field_clip);
  PVideoFrame output;
  ASSERT_NO_THROW(output = filter.GetFrame(filter.GetVideoInfo().num_frames - 1, environment.get()))
      << "DoubleWeaveFields must serve its last advertised frame without requesting a past-end "
         "field";
  ASSERT_NE(output, nullptr);
  EXPECT_NE(output->CheckMemory(), 1);

  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(FieldFilter, ReconstructsFramesThroughPublicWeaveComposition) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 6;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);
  const PClip tff_source(new AssumeParity(source, true));
  const PClip field_clip(new SeparateFields(tff_source, environment.get()));
  const PClip double_weave(new DoubleWeaveFields(field_clip));

  SelectEvery filter(double_weave, 2, 0, environment.get());
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, width);
  EXPECT_EQ(output_vi.height, height);
  EXPECT_EQ(output_vi.num_frames, 2);
  EXPECT_EQ(output_vi.fps_numerator, 25U);
  EXPECT_EQ(output_vi.fps_denominator, 1U);
  EXPECT_FALSE(output_vi.IsFieldBased());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < output_vi.num_frames; ++n) {
    EXPECT_TRUE(filter.GetParity(n)) << "output_frame=" << n;
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_frame_equal(output, source_frames[static_cast<std::size_t>(n)], "Yv24");
    EXPECT_NE(output->CheckMemory(), 1) << "output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 0, 1, 1}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(FieldFilter, DoublesFrameRateAndInterleavesAdjacentFrames) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 6;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);

  DoubleWeaveFrames filter(source);
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, width);
  EXPECT_EQ(output_vi.height, height);
  EXPECT_EQ(output_vi.num_frames, 4);
  EXPECT_EQ(output_vi.fps_numerator, 50U);
  EXPECT_EQ(output_vi.fps_denominator, 1U);
  EXPECT_FALSE(output_vi.IsFieldBased());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  EXPECT_FALSE(filter.GetParity(0));
  const PVideoFrame frame0 = filter.GetFrame(0, environment.get());
  expect_frame_equal(frame0, source_frames[0], "Yv24");
  EXPECT_NE(frame0->CheckMemory(), 1);

  EXPECT_TRUE(filter.GetParity(1));
  const PVideoFrame frame1 = filter.GetFrame(1, environment.get());
  expect_double_weave_frames_frame(frame1, source_frames, 0, true, "Yv24");
  EXPECT_NE(frame1->CheckMemory(), 1);

  EXPECT_FALSE(filter.GetParity(2));
  const PVideoFrame frame2 = filter.GetFrame(2, environment.get());
  expect_frame_equal(frame2, source_frames[1], "Yv24");
  EXPECT_NE(frame2->CheckMemory(), 1);

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 0, 1, 1}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(FieldFilter, DoubleWeaveFramesServesLastAdvertisedFrame) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 6;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);

  DoubleWeaveFrames filter(source);
  PVideoFrame output;
  ASSERT_NO_THROW(output = filter.GetFrame(filter.GetVideoInfo().num_frames - 1, environment.get()))
      << "DoubleWeaveFrames must serve its last advertised frame without requesting a past-end "
         "source frame";
  ASSERT_NE(output, nullptr);
  EXPECT_NE(output->CheckMemory(), 1);

  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(FieldFilter, SelectsPatternedFramesAndUpdatesRate) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 6;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 6, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);

  SelectEvery filter(source, 2, 1, environment.get());
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, width);
  EXPECT_EQ(output_vi.height, height);
  EXPECT_EQ(output_vi.num_frames, 3);
  EXPECT_EQ(output_vi.fps_numerator, 25U);
  EXPECT_EQ(output_vi.fps_denominator, 2U);
  EXPECT_FALSE(output_vi.IsFieldBased());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < output_vi.num_frames; ++n) {
    EXPECT_FALSE(filter.GetParity(n)) << "output_frame=" << n;
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_frame_equal(output, source_frames[static_cast<std::size_t>(n * 2 + 1)], "Yv24");
    EXPECT_NE(output->CheckMemory(), 1) << "output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{1, 3, 5}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(EditFrameFilter, FreezesRangeToSelectedSourceFrame) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 4;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 5, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);

  // Freeze frames 1..3 onto source frame 4; outside the range keep original index.
  FreezeFrame filter(1, 3, 4, source);
  EXPECT_EQ(filter.GetVideoInfo().num_frames, 5);
  EXPECT_EQ(filter.GetVideoInfo().width, width);
  EXPECT_EQ(filter.GetVideoInfo().height, height);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < 5; ++n) {
    const int expected_source = (n >= 1 && n <= 3) ? 4 : n;
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_frame_equal(output, source_frames[static_cast<std::size_t>(expected_source)], "Yv24");
    EXPECT_NE(output->CheckMemory(), 1) << "output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 4, 4, 4, 4}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(EditFrameFilter, DeletesSelectedFrameAndShiftsLaterRequests) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 4;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 4, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);

  // Delete source frame 1: output 0->0, 1->2, 2->3.
  DeleteFrame filter(1, source);
  EXPECT_EQ(filter.GetVideoInfo().num_frames, 3);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < 3; ++n) {
    const int expected_source = n + (n >= 1 ? 1 : 0);
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_frame_equal(output, source_frames[static_cast<std::size_t>(expected_source)], "Yv24");
    EXPECT_NE(output->CheckMemory(), 1) << "output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 2, 3}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

TEST(EditFrameFilter, DuplicatesSelectedFrameAndShiftsLaterRequests) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 4;
  const auto source_vi =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 3, 25, 1});
  auto source_frames = make_field_source_frames(environment, source_vi, source_vi.num_frames);
  std::vector<FrameSnapshot> source_snapshots;
  for (const auto& frame : source_frames) {
    source_snapshots.push_back(FrameSnapshot::capture(frame, source_vi));
  }
  auto* source_impl = new FrameSequenceClip(source_vi, source_frames);
  const PClip source(source_impl);

  // Duplicate source frame 1: output 0->0, 1->1, 2->1, 3->2.
  DuplicateFrame filter(1, source);
  EXPECT_EQ(filter.GetVideoInfo().num_frames, 4);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  for (int n = 0; n < 4; ++n) {
    const int expected_source = n - (n > 1 ? 1 : 0);
    const PVideoFrame output = filter.GetFrame(n, environment.get());
    expect_frame_equal(output, source_frames[static_cast<std::size_t>(expected_source)], "Yv24");
    EXPECT_NE(output->CheckMemory(), 1) << "output_frame=" << n;
  }

  EXPECT_EQ(source_impl->frame_requests(), (std::vector<int>{0, 1, 1, 2}));
  for (std::size_t i = 0; i < source_frames.size(); ++i) {
    EXPECT_EQ(FrameSnapshot::capture(source_frames[i], source_vi), source_snapshots[i])
        << "source_frame=" << i;
  }
}

}  // namespace
