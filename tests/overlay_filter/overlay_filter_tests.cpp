#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_OVERLAY_FILTER_UNDEF_AVS_UNUSED
#endif
#include "filters/overlay/overlay.h"
#ifdef AVSUT_OVERLAY_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_OVERLAY_FILTER_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <ostream>
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

struct OverlayFormatCase {
  int pixel_type;
  int width;
  int height;
  const char* mode;
  const char* name;
};

void PrintTo(const OverlayFormatCase& test_case, std::ostream* stream) { *stream << test_case.name; }

std::vector<PVideoFrame> make_yuv_frames(AviSynthEnvironment& environment, const VideoInfo& vi,
                                         std::uint8_t base) {
  std::vector<PVideoFrame> frames;
  for (int frame_index = 0; frame_index < vi.num_frames; ++frame_index) {
    PVideoFrame frame = environment.get()->NewVideoFrame(vi);
    for (const int plane : video_frame_planes(vi)) {
      fill_plane_full_pitch(frame,
                            static_cast<std::uint8_t>(base + plane * 23 + frame_index * 17),
                            plane);
      write_frame_plane<std::uint8_t>(frame, plane, [base, plane, frame_index](int x, int y) {
        return static_cast<std::uint8_t>(base + plane * 23 + frame_index * 17 + x * 11 + y * 7);
      });
    }
    frames.push_back(frame);
  }
  return frames;
}

std::vector<PVideoFrame> make_full_mask_frames(AviSynthEnvironment& environment, int width,
                                               int height) {
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  std::vector<PVideoFrame> frames;
  for (int frame_index = 0; frame_index < 2; ++frame_index) {
    PVideoFrame frame = environment.get()->NewVideoFrame(vi);
    for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
      fill_plane_full_pitch(frame, 0xff, plane);
    }
    frames.push_back(frame);
  }
  return frames;
}

std::vector<FrameSnapshot> snapshot_frames(const std::vector<PVideoFrame>& frames,
                                            const VideoInfo& vi) {
  std::vector<FrameSnapshot> snapshots;
  for (const auto& frame : frames) {
    snapshots.push_back(FrameSnapshot::capture(frame, vi));
  }
  return snapshots;
}

std::array<AVSValue, 14> make_overlay_args(const PClip& base, const PClip& overlay,
                                           const PClip& mask, const char* mode) {
  std::array<AVSValue, 14> args{};
  args[0] = base;
  args[1] = overlay;
  args[2] = 0;
  args[3] = 0;
  if (mask) {
    args[4] = mask;
  }
  args[5] = 0.5f;
  args[6] = mode;
  args[7] = true;
  args[9] = true;
  args[10] = false;
  args[11] = true;
  args[12] = "";
  args[13] = "mpeg2";
  return args;
}

PVideoFrame render_overlay(const OverlayFormatCase& test_case, AviSynthEnvironment& environment,
                           const VideoInfo& vi, const PClip& base, const PClip& overlay,
                           const PClip& mask) {
  const auto args = make_overlay_args(base, overlay, mask, test_case.mode);
  Overlay filter(base, AVSValue(args.data(), static_cast<int>(args.size())), environment.get());
  EXPECT_EQ(filter.GetVideoInfo().pixel_type, vi.pixel_type);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  return filter.GetFrame(1, environment.get());
}

void expect_active_planes_equal(const PVideoFrame& expected, const PVideoFrame& actual,
                                const VideoInfo& vi, const char* case_name) {
  for (const int plane : video_frame_planes(vi)) {
    ASSERT_EQ(expected->GetRowSize(plane), actual->GetRowSize(plane)) << "case=" << case_name;
    ASSERT_EQ(expected->GetHeight(plane), actual->GetHeight(plane)) << "case=" << case_name;
    for (int y = 0; y < expected->GetHeight(plane); ++y) {
      const auto* expected_row = expected->GetReadPtr(plane) + y * expected->GetPitch(plane);
      const auto* actual_row = actual->GetReadPtr(plane) + y * actual->GetPitch(plane);
      for (int x = 0; x < expected->GetRowSize(plane); ++x) {
        EXPECT_EQ(actual_row[x], expected_row[x]) << "case=" << case_name << " plane=" << plane
                                                  << " x=" << x << " y=" << y;
      }
    }
  }
}

class OverlayFilterFormatTest : public ::testing::TestWithParam<OverlayFormatCase> {};

TEST_P(OverlayFilterFormatTest, FullScaleMaskMatchesOmittedMaskAcrossYuvFormats) {
  const auto& test_case = GetParam();
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{test_case.width, test_case.height,
                                                test_case.pixel_type, 2, 25, 1});

  auto unmasked_base_frames = make_yuv_frames(environment, vi, 29);
  auto unmasked_overlay_frames = make_yuv_frames(environment, vi, 151);
  if (vi.IsYUVA()) {
    for (auto& frame : unmasked_overlay_frames) {
      write_frame_plane<std::uint8_t>(frame, PLANAR_A, [](int, int) { return 255; });
    }
  }
  const auto unmasked_base_before = snapshot_frames(unmasked_base_frames, vi);
  const auto unmasked_overlay_before = snapshot_frames(unmasked_overlay_frames, vi);
  auto* unmasked_base_impl = new FrameSequenceClip(vi, unmasked_base_frames);
  auto* unmasked_overlay_impl = new FrameSequenceClip(vi, unmasked_overlay_frames);
  const PClip unmasked_base(unmasked_base_impl);
  const PClip unmasked_overlay(unmasked_overlay_impl);
  const PVideoFrame unmasked =
      render_overlay(test_case, environment, vi, unmasked_base, unmasked_overlay, PClip());

  auto masked_base_frames = make_yuv_frames(environment, vi, 29);
  auto masked_overlay_frames = make_yuv_frames(environment, vi, 151);
  if (vi.IsYUVA()) {
    for (auto& frame : masked_overlay_frames) {
      write_frame_plane<std::uint8_t>(frame, PLANAR_A, [](int, int) { return 255; });
    }
  }
  const auto full_mask_frames = make_full_mask_frames(environment, test_case.width, test_case.height);
  const auto masked_base_before = snapshot_frames(masked_base_frames, vi);
  const auto masked_overlay_before = snapshot_frames(masked_overlay_frames, vi);
  const auto full_mask_vi = make_video_info(
      VideoInfoSpec{test_case.width, test_case.height, VideoInfo::CS_YV24, 2, 25, 1});
  const auto full_mask_before = snapshot_frames(full_mask_frames, full_mask_vi);
  auto* masked_base_impl = new FrameSequenceClip(vi, masked_base_frames);
  auto* masked_overlay_impl = new FrameSequenceClip(vi, masked_overlay_frames);
  auto* full_mask_impl = new FrameSequenceClip(full_mask_vi, full_mask_frames);
  const PClip masked_base(masked_base_impl);
  const PClip masked_overlay(masked_overlay_impl);
  const PClip full_mask(full_mask_impl);
  const PVideoFrame masked =
      render_overlay(test_case, environment, vi, masked_base, masked_overlay, full_mask);

  expect_active_planes_equal(unmasked, masked, vi, test_case.name);
  EXPECT_NE(unmasked->CheckMemory(), 1);
  EXPECT_NE(masked->CheckMemory(), 1);
  EXPECT_EQ(unmasked_base_impl->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(unmasked_overlay_impl->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(masked_base_impl->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(masked_overlay_impl->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(full_mask_impl->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(snapshot_frames(unmasked_base_frames, vi), unmasked_base_before);
  EXPECT_EQ(snapshot_frames(unmasked_overlay_frames, vi), unmasked_overlay_before);
  EXPECT_EQ(snapshot_frames(masked_base_frames, vi), masked_base_before);
  EXPECT_EQ(snapshot_frames(masked_overlay_frames, vi), masked_overlay_before);
  EXPECT_EQ(snapshot_frames(full_mask_frames, full_mask_vi), full_mask_before);
}

INSTANTIATE_TEST_SUITE_P(
    FormatCases, OverlayFilterFormatTest,
    ::testing::Values(OverlayFormatCase{VideoInfo::CS_YV12, 8, 4, "Blend",
                                        "Yv12_Width8_Height4_Blend_Use444"},
                      OverlayFormatCase{VideoInfo::CS_YV16, 8, 5, "Blend",
                                        "Yv16_Width8_Height5_Blend_Use444"},
                      OverlayFormatCase{VideoInfo::CS_YUVA420, 8, 4, "Blend",
                                        "Yuva420_Width8_Height4_Blend_Use444"}),
    [](const ::testing::TestParamInfo<OverlayFormatCase>& info) { return info.param.name; });

}  // namespace
