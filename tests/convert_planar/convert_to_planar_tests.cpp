#include <gtest/gtest.h>

#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVSUT_DEFINED_AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#endif
#include "convert/convert_planar.h"
#ifdef AVSUT_DEFINED_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_DEFINED_AVS_UNUSED
#endif

#include "support/avisynth_environment.h"
#include "support/video_filter_test_support.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace avsut::test {
namespace {

constexpr int kSourceWidth = 8;
constexpr int kSourceHeight = 4;
constexpr int kYv12Width = 8;
constexpr int kYv12Height = 6;

VideoInfo yv16_video_info() {
  return make_video_info(VideoInfoSpec{kSourceWidth, kSourceHeight, VideoInfo::CS_YV16, 1, 25, 1});
}

void fill_yv16_source(PVideoFrame& frame) {
  fill_plane_full_pitch(frame, 0xe1, PLANAR_Y);
  fill_plane_full_pitch(frame, 0xd2, PLANAR_U);
  fill_plane_full_pitch(frame, 0xc3, PLANAR_V);
  write_frame_plane<std::uint8_t>(frame, PLANAR_Y,
                                  [](int x, int y) { return 7 + x * 13 + y * 19; });
  write_frame_plane<std::uint8_t>(frame, PLANAR_U,
                                  [](int x, int y) { return 31 + x * 29 + y * 37; });
  write_frame_plane<std::uint8_t>(frame, PLANAR_V,
                                  [](int x, int y) { return 83 + x * 17 + y * 23; });
}

VideoInfo yv12_video_info() {
  return make_video_info(VideoInfoSpec{kYv12Width, kYv12Height, VideoInfo::CS_YV12, 1, 25, 1});
}

void fill_yv12_source(PVideoFrame& frame) {
  fill_plane_full_pitch(frame, 0xb1, PLANAR_Y);
  fill_plane_full_pitch(frame, 0xa2, PLANAR_U);
  fill_plane_full_pitch(frame, 0x93, PLANAR_V);
  write_frame_plane<std::uint8_t>(frame, PLANAR_Y,
                                  [](int x, int y) { return 11 + x * 17 + y * 23; });
  write_frame_plane<std::uint8_t>(frame, PLANAR_U,
                                  [](int x, int y) { return 37 + x * 31 + y * 19; });
  write_frame_plane<std::uint8_t>(frame, PLANAR_V,
                                  [](int x, int y) { return 71 + x * 13 + y * 29; });
}

void fill_y8_source(PVideoFrame& frame) {
  fill_plane_full_pitch(frame, 0x6f, PLANAR_Y);
  write_frame_plane<std::uint8_t>(frame, PLANAR_Y,
                                  [](int x, int y) { return 9 + x * 23 + y * 31; });
}

TEST(ConvertToPlanarGeneric, UpsamplesYv16ChromaWithPointFilterAndCopiesProperties) {
  AviSynthEnvironment environment;
  const auto source_vi = yv16_video_info();
  PVideoFrame source_frame = environment.get()->NewVideoFrame(source_vi);
  fill_yv16_source(source_frame);
  set_frame_property_int(environment.get(), source_frame, "_ChromaLocation", AVS_CHROMA_LEFT);
  set_frame_property_int(environment.get(), source_frame, "_ColorRange", AVS_COLORRANGE_FULL);
  set_frame_property_int(environment.get(), source_frame, "_FieldBased", AVS_FIELD_TOP);
  const auto source_before = FrameSnapshot::capture(source_frame, source_vi);
  auto* source_clip_impl = new StaticFrameClip(source_vi, source_frame);
  const PClip source(source_clip_impl);

  const AVSValue point_resampler("point");
  const AVSValue no_parameter;
  ConvertToPlanarGeneric filter(source, VideoInfo::CS_YV24, false, AVS_CHROMA_LEFT, point_resampler,
                                no_parameter, no_parameter, no_parameter, AVS_CHROMA_UNUSED,
                                environment.get());

  ASSERT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_YV24);
  ASSERT_EQ(filter.GetVideoInfo().width, kSourceWidth);
  ASSERT_EQ(filter.GetVideoInfo().height, kSourceHeight);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  const PVideoFrame output = filter.GetFrame(0, environment.get());
  ASSERT_EQ(output->GetRowSize(PLANAR_Y), kSourceWidth);
  ASSERT_EQ(output->GetRowSize(PLANAR_U), kSourceWidth);
  ASSERT_EQ(output->GetRowSize(PLANAR_V), kSourceWidth);
  ASSERT_EQ(output->GetHeight(PLANAR_U), kSourceHeight);

  for (int y = 0; y < kSourceHeight; ++y) {
    const auto* source_y =
        source_frame->GetReadPtr(PLANAR_Y) + y * source_frame->GetPitch(PLANAR_Y);
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    for (int x = 0; x < kSourceWidth; ++x) {
      EXPECT_EQ(output_y[x], source_y[x]) << "plane=Y row=" << y << " column=" << x;
    }

    const auto* source_u =
        source_frame->GetReadPtr(PLANAR_U) + y * source_frame->GetPitch(PLANAR_U);
    const auto* source_v =
        source_frame->GetReadPtr(PLANAR_V) + y * source_frame->GetPitch(PLANAR_V);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    for (int x = 0; x < kSourceWidth; ++x) {
      EXPECT_EQ(output_u[x], source_u[x / 2]) << "plane=U row=" << y << " column=" << x;
      EXPECT_EQ(output_v[x], source_v[x / 2]) << "plane=V row=" << y << " column=" << x;
    }
  }

  EXPECT_FALSE(get_frame_property_int(environment.get(), output, "_ChromaLocation").has_value());
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_ColorRange"),
            std::optional<int>{AVS_COLORRANGE_FULL});
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_FieldBased"),
            std::optional<int>{AVS_FIELD_TOP});
  const std::vector<int> expected_requests{0, 0, 0};
  EXPECT_EQ(source_clip_impl->frame_requests(), expected_requests);
  EXPECT_NE(source_frame->CheckMemory(), 1);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(FrameSnapshot::capture(source_frame, source_vi), source_before);
}

TEST(ConvertToPlanarGeneric, UpsamplesYv12ChromaAcrossBothAxesWithTopLeftPlacement) {
  AviSynthEnvironment environment;
  const auto source_vi = yv12_video_info();
  PVideoFrame source_frame = environment.get()->NewVideoFrame(source_vi);
  fill_yv12_source(source_frame);
  set_frame_property_int(environment.get(), source_frame, "_ChromaLocation", AVS_CHROMA_TOP_LEFT);
  set_frame_property_int(environment.get(), source_frame, "_ColorRange", AVS_COLORRANGE_FULL);
  set_frame_property_int(environment.get(), source_frame, "_FieldBased", AVS_FIELD_TOP);
  const auto source_before = FrameSnapshot::capture(source_frame, source_vi);
  auto* source_clip_impl = new StaticFrameClip(source_vi, source_frame);
  const PClip source(source_clip_impl);

  const AVSValue point_resampler("point");
  const AVSValue no_parameter;
  ConvertToPlanarGeneric filter(source, VideoInfo::CS_YV24, false, AVS_CHROMA_TOP_LEFT,
                                point_resampler, no_parameter, no_parameter, no_parameter,
                                AVS_CHROMA_UNUSED, environment.get());

  ASSERT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_YV24);
  ASSERT_EQ(filter.GetVideoInfo().width, kYv12Width);
  ASSERT_EQ(filter.GetVideoInfo().height, kYv12Height);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  const PVideoFrame output = filter.GetFrame(0, environment.get());
  ASSERT_EQ(output->GetRowSize(PLANAR_Y), kYv12Width);
  ASSERT_EQ(output->GetRowSize(PLANAR_U), kYv12Width);
  ASSERT_EQ(output->GetRowSize(PLANAR_V), kYv12Width);
  ASSERT_EQ(output->GetHeight(PLANAR_U), kYv12Height);
  ASSERT_EQ(output->GetHeight(PLANAR_V), kYv12Height);

  for (int y = 0; y < kYv12Height; ++y) {
    const auto* source_y =
        source_frame->GetReadPtr(PLANAR_Y) + y * source_frame->GetPitch(PLANAR_Y);
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* source_u =
        source_frame->GetReadPtr(PLANAR_U) + (y / 2) * source_frame->GetPitch(PLANAR_U);
    const auto* source_v =
        source_frame->GetReadPtr(PLANAR_V) + (y / 2) * source_frame->GetPitch(PLANAR_V);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    for (int x = 0; x < kYv12Width; ++x) {
      EXPECT_EQ(output_y[x], source_y[x]) << "plane=Y row=" << y << " column=" << x;
      EXPECT_EQ(output_u[x], source_u[x / 2]) << "plane=U row=" << y << " column=" << x;
      EXPECT_EQ(output_v[x], source_v[x / 2]) << "plane=V row=" << y << " column=" << x;
    }
  }

  EXPECT_FALSE(get_frame_property_int(environment.get(), output, "_ChromaLocation").has_value());
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_ColorRange"),
            std::optional<int>{AVS_COLORRANGE_FULL});
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_FieldBased"),
            std::optional<int>{AVS_FIELD_TOP});
  const std::vector<int> expected_requests{0, 0, 0};
  EXPECT_EQ(source_clip_impl->frame_requests(), expected_requests);
  EXPECT_NE(source_frame->CheckMemory(), 1);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(FrameSnapshot::capture(source_frame, source_vi), source_before);
}

TEST(ConvertToPlanarGeneric, ConvertsYv12ToYv16AndSetsOutputChromaPlacement) {
  AviSynthEnvironment environment;
  const auto source_vi = yv12_video_info();
  PVideoFrame source_frame = environment.get()->NewVideoFrame(source_vi);
  fill_yv12_source(source_frame);
  set_frame_property_int(environment.get(), source_frame, "_ChromaLocation", AVS_CHROMA_TOP_LEFT);
  set_frame_property_int(environment.get(), source_frame, "_ColorRange", AVS_COLORRANGE_FULL);
  set_frame_property_int(environment.get(), source_frame, "_FieldBased", AVS_FIELD_TOP);
  const auto source_before = FrameSnapshot::capture(source_frame, source_vi);
  auto* source_clip_impl = new StaticFrameClip(source_vi, source_frame);
  const PClip source(source_clip_impl);

  const AVSValue point_resampler("point");
  const AVSValue no_parameter;
  ConvertToPlanarGeneric filter(source, VideoInfo::CS_YV16, false, AVS_CHROMA_TOP_LEFT,
                                point_resampler, no_parameter, no_parameter, no_parameter,
                                AVS_CHROMA_CENTER, environment.get());

  ASSERT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_YV16);
  ASSERT_EQ(filter.GetVideoInfo().width, kYv12Width);
  ASSERT_EQ(filter.GetVideoInfo().height, kYv12Height);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  const PVideoFrame output = filter.GetFrame(0, environment.get());
  ASSERT_EQ(output->GetRowSize(PLANAR_Y), kYv12Width);
  ASSERT_EQ(output->GetRowSize(PLANAR_U), kYv12Width / 2);
  ASSERT_EQ(output->GetRowSize(PLANAR_V), kYv12Width / 2);
  ASSERT_EQ(output->GetHeight(PLANAR_U), kYv12Height);
  ASSERT_EQ(output->GetHeight(PLANAR_V), kYv12Height);

  for (int y = 0; y < kYv12Height; ++y) {
    const auto* source_y =
        source_frame->GetReadPtr(PLANAR_Y) + y * source_frame->GetPitch(PLANAR_Y);
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* source_u =
        source_frame->GetReadPtr(PLANAR_U) + (y / 2) * source_frame->GetPitch(PLANAR_U);
    const auto* source_v =
        source_frame->GetReadPtr(PLANAR_V) + (y / 2) * source_frame->GetPitch(PLANAR_V);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    for (int x = 0; x < kYv12Width; ++x) {
      EXPECT_EQ(output_y[x], source_y[x]) << "plane=Y row=" << y << " column=" << x;
    }
    for (int x = 0; x < kYv12Width / 2; ++x) {
      EXPECT_EQ(output_u[x], source_u[x]) << "plane=U row=" << y << " column=" << x;
      EXPECT_EQ(output_v[x], source_v[x]) << "plane=V row=" << y << " column=" << x;
    }
  }

  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_ChromaLocation"),
            std::optional<int>{AVS_CHROMA_CENTER});
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_ColorRange"),
            std::optional<int>{AVS_COLORRANGE_FULL});
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_FieldBased"),
            std::optional<int>{AVS_FIELD_TOP});
  const std::vector<int> expected_requests{0, 0, 0};
  EXPECT_EQ(source_clip_impl->frame_requests(), expected_requests);
  EXPECT_NE(source_frame->CheckMemory(), 1);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(FrameSnapshot::capture(source_frame, source_vi), source_before);
}

TEST(ConvertToPlanarGeneric, ExpandsY8ToYuva420WithNeutralChromaAndOpaqueAlpha) {
  AviSynthEnvironment environment;
  const auto source_vi =
      make_video_info(VideoInfoSpec{kYv12Width, kYv12Height, VideoInfo::CS_Y8, 1, 25, 1});
  PVideoFrame source_frame = environment.get()->NewVideoFrame(source_vi);
  fill_y8_source(source_frame);
  set_frame_property_int(environment.get(), source_frame, "_ColorRange", AVS_COLORRANGE_FULL);
  set_frame_property_int(environment.get(), source_frame, "_FieldBased", AVS_FIELD_TOP);
  const auto source_before = FrameSnapshot::capture(source_frame, source_vi);
  auto* source_clip_impl = new StaticFrameClip(source_vi, source_frame);
  const PClip source(source_clip_impl);

  const AVSValue point_resampler("point");
  const AVSValue no_parameter;
  ConvertToPlanarGeneric filter(source, VideoInfo::CS_YUVA420, false, AVS_CHROMA_UNUSED,
                                point_resampler, no_parameter, no_parameter, no_parameter,
                                AVS_CHROMA_LEFT, environment.get());

  ASSERT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_YUVA420);
  ASSERT_EQ(filter.GetVideoInfo().width, kYv12Width);
  ASSERT_EQ(filter.GetVideoInfo().height, kYv12Height);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  const PVideoFrame output = filter.GetFrame(0, environment.get());
  ASSERT_EQ(output->GetRowSize(PLANAR_Y), kYv12Width);
  ASSERT_EQ(output->GetRowSize(PLANAR_U), kYv12Width / 2);
  ASSERT_EQ(output->GetRowSize(PLANAR_V), kYv12Width / 2);
  ASSERT_EQ(output->GetRowSize(PLANAR_A), kYv12Width);
  ASSERT_EQ(output->GetHeight(PLANAR_U), kYv12Height / 2);
  ASSERT_EQ(output->GetHeight(PLANAR_V), kYv12Height / 2);
  ASSERT_EQ(output->GetHeight(PLANAR_A), kYv12Height);

  for (int y = 0; y < kYv12Height; ++y) {
    const auto* source_y =
        source_frame->GetReadPtr(PLANAR_Y) + y * source_frame->GetPitch(PLANAR_Y);
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* output_a = output->GetReadPtr(PLANAR_A) + y * output->GetPitch(PLANAR_A);
    for (int x = 0; x < kYv12Width; ++x) {
      EXPECT_EQ(output_y[x], source_y[x]) << "plane=Y row=" << y << " column=" << x;
      EXPECT_EQ(output_a[x], 255) << "plane=A row=" << y << " column=" << x;
    }
  }
  for (int y = 0; y < kYv12Height / 2; ++y) {
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    for (int x = 0; x < kYv12Width / 2; ++x) {
      EXPECT_EQ(output_u[x], 128) << "plane=U row=" << y << " column=" << x;
      EXPECT_EQ(output_v[x], 128) << "plane=V row=" << y << " column=" << x;
    }
  }

  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_ChromaLocation"),
            std::optional<int>{AVS_CHROMA_LEFT});
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_ColorRange"),
            std::optional<int>{AVS_COLORRANGE_FULL});
  EXPECT_EQ(get_frame_property_int(environment.get(), output, "_FieldBased"),
            std::optional<int>{AVS_FIELD_TOP});
  const std::vector<int> expected_requests{0};
  EXPECT_EQ(source_clip_impl->frame_requests(), expected_requests);
  EXPECT_NE(source_frame->CheckMemory(), 1);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(FrameSnapshot::capture(source_frame, source_vi), source_before);
}

}  // namespace
}  // namespace avsut::test
