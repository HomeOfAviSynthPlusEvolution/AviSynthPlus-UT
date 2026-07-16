#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_LAYER_FILTER_UNDEF_AVS_UNUSED
#endif
#include "filters/layer.h"
#ifdef AVSUT_LAYER_FILTER_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_LAYER_FILTER_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSequenceClip;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::read_frame_plane_active;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;
using avsut::test::video_frame_planes;
using avsut::test::write_frame_plane;

struct Rgb32Pixel {
  std::uint8_t blue;
  std::uint8_t green;
  std::uint8_t red;
  std::uint8_t alpha;
};

struct Rgb64Pixel {
  std::uint16_t blue;
  std::uint16_t green;
  std::uint16_t red;
  std::uint16_t alpha;
};

void write_rgb32(PVideoFrame& frame, const std::vector<Rgb32Pixel>& pixels) {
  const int pitch = frame->GetPitch();
  const int width = frame->GetRowSize() / 4;
  ASSERT_EQ(pixels.size(), static_cast<std::size_t>(width));
  for (int y = 0; y < frame->GetHeight(); ++y) {
    auto* row = frame->GetWritePtr() + y * pitch;
    for (int x = 0; x < width; ++x) {
      const auto& pixel = pixels[static_cast<std::size_t>(x)];
      row[4 * x + 0] = pixel.blue;
      row[4 * x + 1] = pixel.green;
      row[4 * x + 2] = pixel.red;
      row[4 * x + 3] = pixel.alpha;
    }
  }
}

void write_bgr64(PVideoFrame& frame, const std::vector<Rgb64Pixel>& pixels) {
  const int pitch = frame->GetPitch();
  const int width = frame->GetRowSize() / static_cast<int>(sizeof(Rgb64Pixel));
  ASSERT_EQ(pixels.size(), static_cast<std::size_t>(width));
  for (int y = 0; y < frame->GetHeight(); ++y) {
    auto* row = reinterpret_cast<Rgb64Pixel*>(frame->GetWritePtr() + y * pitch);
    for (int x = 0; x < width; ++x) {
      row[x] = pixels[static_cast<std::size_t>(x)];
    }
  }
}

std::uint8_t rec601_luma(const Rgb32Pixel& pixel) {
  return static_cast<std::uint8_t>(
      (3736 * pixel.blue + 19234 * pixel.green + 9798 * pixel.red + 16384) >> 15);
}

TEST(MaskFilter, WritesRec601AlphaAndUsesLastMaskFrame) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int height = 2;
  const auto vi_two_frames =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 2, 25, 1});
  const auto vi_one_frame =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});

  PVideoFrame source_frame0 = environment.get()->NewVideoFrame(vi_two_frames);
  PVideoFrame source_frame1 = environment.get()->NewVideoFrame(vi_two_frames);
  fill_plane_full_pitch(source_frame0, 0xa1, DEFAULT_PLANE);
  fill_plane_full_pitch(source_frame1, 0xa2, DEFAULT_PLANE);
  write_rgb32(source_frame0, {{1, 2, 3, 4},
                              {10, 20, 30, 40},
                              {50, 60, 70, 80},
                              {90, 100, 110, 120},
                              {130, 140, 150, 160},
                              {170, 180, 190, 200},
                              {220, 230, 240, 250}});
  write_rgb32(source_frame1, {{9, 8, 7, 6},
                              {19, 29, 39, 49},
                              {59, 69, 79, 89},
                              {99, 109, 119, 129},
                              {139, 149, 159, 169},
                              {179, 189, 199, 209},
                              {229, 239, 249, 255}});

  PVideoFrame mask_frame = environment.get()->NewVideoFrame(vi_one_frame);
  fill_plane_full_pitch(mask_frame, 0xb3, DEFAULT_PLANE);
  const std::vector<Rgb32Pixel> mask_pixels{
      {40, 120, 200, 0},  {37, 115, 193, 1}, {43, 125, 207, 2}, {0, 0, 0, 3},
      {255, 255, 255, 4}, {80, 160, 32, 5},  {200, 100, 40, 6}};
  write_rgb32(mask_frame, mask_pixels);

  const auto source_before = FrameSnapshot::capture(source_frame1, vi_two_frames);
  const auto mask_before = FrameSnapshot::capture(mask_frame, vi_one_frame);
  auto* source_clip = new FrameSequenceClip(vi_two_frames, {source_frame0, source_frame1});
  auto* mask_clip = new StaticFrameClip(vi_one_frame, mask_frame);
  const PClip source(source_clip);
  const PClip mask(mask_clip);

  Mask filter(source, mask, environment.get());
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      EXPECT_EQ(row[4 * x + 3], rec601_luma(mask_pixels[static_cast<std::size_t>(x)]))
          << "x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(mask_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source_frame1, vi_two_frames), source_before);
  EXPECT_EQ(FrameSnapshot::capture(mask_frame, vi_one_frame), mask_before);
}

TEST(ColorKeyMaskFilter, ClearsAlphaOnlyForInclusiveRgbToleranceMatches) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xc4, DEFAULT_PLANE);
  const std::vector<Rgb32Pixel> pixels{{40, 120, 200, 17}, {37, 115, 193, 31}, {36, 120, 200, 45},
                                       {40, 125, 200, 59}, {40, 126, 200, 73}, {40, 120, 207, 87},
                                       {40, 120, 208, 101}};
  write_rgb32(source, pixels);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  ColorKeyMask filter(clip, 0xc87828, 3, 5, 7, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      const auto& pixel = pixels[static_cast<std::size_t>(x)];
      const bool matches = std::abs(static_cast<int>(pixel.blue) - 40) <= 3 &&
                           std::abs(static_cast<int>(pixel.green) - 120) <= 5 &&
                           std::abs(static_cast<int>(pixel.red) - 200) <= 7;
      EXPECT_EQ(row[4 * x + 0], pixel.blue) << "blue x=" << x << " y=" << y;
      EXPECT_EQ(row[4 * x + 1], pixel.green) << "green x=" << x << " y=" << y;
      EXPECT_EQ(row[4 * x + 2], pixel.red) << "red x=" << x << " y=" << y;
      EXPECT_EQ(row[4 * x + 3], matches ? 0 : pixel.alpha) << "alpha x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(ResetMaskFilter, WritesPackedAlphaFromMaskValue) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xc9, DEFAULT_PLANE);
  for (int y = 0; y < height; ++y) {
    auto* row = source->GetWritePtr() + y * source->GetPitch();
    for (int x = 0; x < width; ++x) {
      row[4 * x + 0] = static_cast<std::uint8_t>(11 + x * 13 + y * 7);
      row[4 * x + 1] = static_cast<std::uint8_t>(23 + x * 17 + y * 5);
      row[4 * x + 2] = static_cast<std::uint8_t>(37 + x * 19 + y * 3);
      row[4 * x + 3] = static_cast<std::uint8_t>(191 - x * 11 - y * 9);
    }
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  ResetMask filter(clip, AVSValue(37), AVSValue(), environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* source_row = source->GetReadPtr() + y * source->GetPitch();
    const auto* output_row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      for (int component = 0; component < 3; ++component) {
        EXPECT_EQ(output_row[4 * x + component], source_row[4 * x + component])
            << "component=" << component << " x=" << x << " y=" << y;
      }
      EXPECT_EQ(output_row[4 * x + 3], 37) << "alpha x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(ResetMaskFilter, UsesOpacityForPlanarYuvaAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 6;
  constexpr int height = 4;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YUVA420, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa1, PLANAR_Y);
  fill_plane_full_pitch(source, 0xb2, PLANAR_U);
  fill_plane_full_pitch(source, 0xc3, PLANAR_V);
  fill_plane_full_pitch(source, 0xd4, PLANAR_A);
  write_frame_plane<std::uint8_t>(source, PLANAR_Y,
                                  [](int x, int y) { return 17 + x * 9 + y * 13; });
  write_frame_plane<std::uint8_t>(source, PLANAR_U,
                                  [](int x, int y) { return 61 + x * 7 + y * 11; });
  write_frame_plane<std::uint8_t>(source, PLANAR_V,
                                  [](int x, int y) { return 193 - x * 5 - y * 17; });
  write_frame_plane<std::uint8_t>(source, PLANAR_A,
                                  [](int x, int y) { return 23 + x * 3 + y * 19; });
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  ResetMask filter(clip, AVSValue(17), AVSValue(0.5F), environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    EXPECT_EQ(read_frame_plane_active<std::uint8_t>(output, plane),
              read_frame_plane_active<std::uint8_t>(source, plane))
        << "plane=" << plane;
  }
  for (int y = 0; y < output->GetHeight(PLANAR_A); ++y) {
    const auto* row = output->GetReadPtr(PLANAR_A) + y * output->GetPitch(PLANAR_A);
    for (int x = 0; x < output->GetRowSize(PLANAR_A); ++x) {
      EXPECT_EQ(row[x], 128) << "alpha x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(ShowChannelFilter, ExtractsPackedRedToYuvaAndPreservesAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xe5, DEFAULT_PLANE);
  for (int raw_y = 0; raw_y < height; ++raw_y) {
    auto* row = source->GetWritePtr() + raw_y * source->GetPitch();
    for (int x = 0; x < width; ++x) {
      row[4 * x + 0] = static_cast<std::uint8_t>(7 + x * 11 + raw_y * 17);
      row[4 * x + 1] = static_cast<std::uint8_t>(29 + x * 13 + raw_y * 19);
      row[4 * x + 2] = static_cast<std::uint8_t>(53 + x * 17 + raw_y * 23);
      row[4 * x + 3] = static_cast<std::uint8_t>(101 + x * 7 + raw_y * 5);
    }
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  ShowChannel filter(clip, "yuva444", 2, environment.get());
  EXPECT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_YUVA444);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const int source_y = height - 1 - y;
    const auto* source_row = source->GetReadPtr() + source_y * source->GetPitch();
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    const auto* output_a = output->GetReadPtr(PLANAR_A) + y * output->GetPitch(PLANAR_A);
    for (int x = 0; x < width; ++x) {
      EXPECT_EQ(output_y[x], source_row[4 * x + 2]) << "Y x=" << x << " y=" << y;
      EXPECT_EQ(output_u[x], 128) << "U x=" << x << " y=" << y;
      EXPECT_EQ(output_v[x], 128) << "V x=" << x << " y=" << y;
      EXPECT_EQ(output_a[x], source_row[4 * x + 3]) << "A x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(ShowChannelFilter, RejectsInvalidChannelAndSubsampledOutputBeforeFrameRequest) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0x61, PLANAR_Y);
  fill_plane_full_pitch(source, 0x72, PLANAR_U);
  fill_plane_full_pitch(source, 0x83, PLANAR_V);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  EXPECT_THROW(
      { ShowChannel filter(clip, "rgb", 0, environment.get()); }, AvisynthError);
  EXPECT_THROW(
      { ShowChannel filter(clip, "yuv420", 4, environment.get()); }, AvisynthError);
  EXPECT_TRUE(source_clip->frame_requests().empty());
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(MergeRgbFilter, AssemblesPlanarRgbapFromPlanarChannelSources) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_RGBP8, 1, 25, 1});
  const auto alpha_vi = make_video_info(
      VideoInfoSpec{width, height, VideoInfo::CS_RGBAP8, 1, 25, 1});

  PVideoFrame blue = environment.get()->NewVideoFrame(vi);
  PVideoFrame green = environment.get()->NewVideoFrame(vi);
  PVideoFrame red = environment.get()->NewVideoFrame(vi);
  PVideoFrame alpha = environment.get()->NewVideoFrame(alpha_vi);
  for (const int plane : {PLANAR_G, PLANAR_B, PLANAR_R}) {
    fill_plane_full_pitch(blue, static_cast<std::uint8_t>(0x11 + plane), plane);
    fill_plane_full_pitch(green, static_cast<std::uint8_t>(0x21 + plane), plane);
    fill_plane_full_pitch(red, static_cast<std::uint8_t>(0x31 + plane), plane);
  }
  for (const int plane : {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A}) {
    fill_plane_full_pitch(alpha, static_cast<std::uint8_t>(0x41 + plane), plane);
  }
  write_frame_plane<std::uint8_t>(blue, PLANAR_B,
                                  [](int x, int y) { return 11 + x * 7 + y * 13; });
  write_frame_plane<std::uint8_t>(green, PLANAR_G,
                                  [](int x, int y) { return 37 + x * 11 + y * 17; });
  write_frame_plane<std::uint8_t>(red, PLANAR_R,
                                  [](int x, int y) { return 71 + x * 13 + y * 19; });
  write_frame_plane<std::uint8_t>(alpha, PLANAR_A,
                                  [](int x, int y) { return 101 + x * 5 + y * 23; });

  const auto blue_before = FrameSnapshot::capture(blue, vi);
  const auto green_before = FrameSnapshot::capture(green, vi);
  const auto red_before = FrameSnapshot::capture(red, vi);
  const auto alpha_before = FrameSnapshot::capture(alpha, alpha_vi);
  auto* blue_clip_impl = new StaticFrameClip(vi, blue);
  auto* green_clip_impl = new StaticFrameClip(vi, green);
  auto* red_clip_impl = new StaticFrameClip(vi, red);
  auto* alpha_clip_impl = new StaticFrameClip(alpha_vi, alpha);
  const PClip blue_clip(blue_clip_impl);
  const PClip green_clip(green_clip_impl);
  const PClip red_clip(red_clip_impl);
  const PClip alpha_clip(alpha_clip_impl);

  MergeRGB filter(red_clip, blue_clip, green_clip, red_clip, alpha_clip, "rgbap",
                  environment.get());
  EXPECT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_RGBAP8);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  EXPECT_EQ(read_frame_plane_active<std::uint8_t>(output, PLANAR_G),
            read_frame_plane_active<std::uint8_t>(green, PLANAR_G));
  EXPECT_EQ(read_frame_plane_active<std::uint8_t>(output, PLANAR_B),
            read_frame_plane_active<std::uint8_t>(blue, PLANAR_B));
  EXPECT_EQ(read_frame_plane_active<std::uint8_t>(output, PLANAR_R),
            read_frame_plane_active<std::uint8_t>(red, PLANAR_R));
  EXPECT_EQ(read_frame_plane_active<std::uint8_t>(output, PLANAR_A),
            read_frame_plane_active<std::uint8_t>(alpha, PLANAR_A));
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(blue_clip_impl->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(green_clip_impl->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(red_clip_impl->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(alpha_clip_impl->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(blue, vi), blue_before);
  EXPECT_EQ(FrameSnapshot::capture(green, vi), green_before);
  EXPECT_EQ(FrameSnapshot::capture(red, vi), red_before);
  EXPECT_EQ(FrameSnapshot::capture(alpha, alpha_vi), alpha_before);
}

TEST(MergeRgbFilter, RejectsMismatchedChannelsBeforeFrameRequest) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_RGBP8, 1, 25, 1});
  const auto wide_vi = make_video_info(
      VideoInfoSpec{width + 1, height, VideoInfo::CS_RGBP8, 1, 25, 1});

  auto make_source = [&](const VideoInfo& source_vi, std::uint8_t value) {
    PVideoFrame frame = environment.get()->NewVideoFrame(source_vi);
    for (const int plane : {PLANAR_G, PLANAR_B, PLANAR_R}) {
      fill_plane_full_pitch(frame, static_cast<std::uint8_t>(value + plane), plane);
    }
    return frame;
  };
  PVideoFrame blue = make_source(vi, 0x21);
  PVideoFrame green = make_source(vi, 0x31);
  PVideoFrame wide_green = make_source(wide_vi, 0x39);
  PVideoFrame red = make_source(vi, 0x41);
  PVideoFrame alpha = make_source(vi, 0x51);
  const auto blue_before = FrameSnapshot::capture(blue, vi);
  const auto green_before = FrameSnapshot::capture(green, vi);
  const auto wide_green_before = FrameSnapshot::capture(wide_green, wide_vi);
  const auto red_before = FrameSnapshot::capture(red, vi);
  const auto alpha_before = FrameSnapshot::capture(alpha, vi);
  auto* blue_clip_impl = new StaticFrameClip(vi, blue);
  auto* green_clip_impl = new StaticFrameClip(vi, green);
  auto* wide_green_clip_impl = new StaticFrameClip(wide_vi, wide_green);
  auto* red_clip_impl = new StaticFrameClip(vi, red);
  auto* alpha_clip_impl = new StaticFrameClip(vi, alpha);
  const PClip blue_clip(blue_clip_impl);
  const PClip green_clip(green_clip_impl);
  const PClip wide_green_clip(wide_green_clip_impl);
  const PClip red_clip(red_clip_impl);
  const PClip alpha_clip(alpha_clip_impl);

  EXPECT_THROW(
      {
        MergeRGB filter(red_clip, blue_clip, wide_green_clip, red_clip, PClip(), "rgb",
                        environment.get());
      },
      AvisynthError);
  EXPECT_THROW(
      {
        MergeRGB filter(red_clip, blue_clip, green_clip, red_clip, alpha_clip, "rgbap",
                        environment.get());
      },
      AvisynthError);
  EXPECT_TRUE(blue_clip_impl->frame_requests().empty());
  EXPECT_TRUE(green_clip_impl->frame_requests().empty());
  EXPECT_TRUE(wide_green_clip_impl->frame_requests().empty());
  EXPECT_TRUE(red_clip_impl->frame_requests().empty());
  EXPECT_TRUE(alpha_clip_impl->frame_requests().empty());
  EXPECT_EQ(FrameSnapshot::capture(blue, vi), blue_before);
  EXPECT_EQ(FrameSnapshot::capture(green, vi), green_before);
  EXPECT_EQ(FrameSnapshot::capture(wide_green, wide_vi), wide_green_before);
  EXPECT_EQ(FrameSnapshot::capture(red, vi), red_before);
  EXPECT_EQ(FrameSnapshot::capture(alpha, vi), alpha_before);
}

template <std::size_t N>
void write_plane_values(PVideoFrame& frame, int plane, const std::array<std::uint8_t, N>& values) {
  const int pitch = frame->GetPitch(plane);
  const int width = frame->GetRowSize(plane);
  const int height = frame->GetHeight(plane);
  for (int y = 0; y < height; ++y) {
    auto* row = frame->GetWritePtr(plane) + y * pitch;
    for (int x = 0; x < width; ++x) {
      row[x] = values[static_cast<std::size_t>(x) % values.size()];
    }
  }
}

TEST(InvertFilter, InvertsSelectedYuvPlanesAndCopiesUnselectedPlane) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd5, PLANAR_Y);
  fill_plane_full_pitch(source, 0xe6, PLANAR_U);
  fill_plane_full_pitch(source, 0xf7, PLANAR_V);
  constexpr std::array<std::uint8_t, 7> y_values{0, 1, 16, 127, 128, 254, 255};
  constexpr std::array<std::uint8_t, 7> u_values{0, 1, 64, 127, 128, 254, 255};
  constexpr std::array<std::uint8_t, 7> v_values{3, 19, 47, 89, 137, 201, 251};
  write_plane_values(source, PLANAR_Y, y_values);
  write_plane_values(source, PLANAR_U, u_values);
  write_plane_values(source, PLANAR_V, v_values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Invert filter(clip, "YU", environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    for (int x = 0; x < width; ++x) {
      const auto y_value = y_values[static_cast<std::size_t>(x)];
      const auto u_value = u_values[static_cast<std::size_t>(x)];
      EXPECT_EQ(output_y[x], static_cast<std::uint8_t>(255 - y_value)) << "Y x=" << x << " y=" << y;
      EXPECT_EQ(output_u[x],
                static_cast<std::uint8_t>(std::min(255, 256 - static_cast<int>(u_value))))
          << "U x=" << x << " y=" << y;
      EXPECT_EQ(output_v[x], v_values[static_cast<std::size_t>(x)]) << "V x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

std::uint8_t subtract_reference(std::uint8_t first, std::uint8_t second, int bias) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(first) - static_cast<int>(second) + bias, 0, 255));
}

void write_yuv_frame(PVideoFrame& frame, const std::array<std::uint8_t, 7>& y_values,
                     const std::array<std::uint8_t, 7>& u_values,
                     const std::array<std::uint8_t, 7>& v_values) {
  fill_plane_full_pitch(frame, 0x11, PLANAR_Y);
  fill_plane_full_pitch(frame, 0x22, PLANAR_U);
  fill_plane_full_pitch(frame, 0x33, PLANAR_V);
  write_plane_values(frame, PLANAR_Y, y_values);
  write_plane_values(frame, PLANAR_U, u_values);
  write_plane_values(frame, PLANAR_V, v_values);
}

TEST(SubtractFilter, AppliesLumaAndChromaCenteredDifferencesAcrossFrameSequences) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  const std::array<std::uint8_t, 7> first_y{0, 10, 100, 126, 200, 250, 255};
  const std::array<std::uint8_t, 7> first_u{0, 20, 100, 128, 200, 250, 255};
  const std::array<std::uint8_t, 7> first_v{255, 200, 128, 100, 20, 10, 0};
  const std::array<std::uint8_t, 7> second_y{255, 0, 90, 126, 0, 10, 255};
  const std::array<std::uint8_t, 7> second_u{255, 0, 90, 128, 0, 10, 255};
  const std::array<std::uint8_t, 7> second_v{0, 10, 128, 100, 20, 200, 255};
  PVideoFrame first_frame0 = environment.get()->NewVideoFrame(vi);
  PVideoFrame first_frame1 = environment.get()->NewVideoFrame(vi);
  PVideoFrame second_frame0 = environment.get()->NewVideoFrame(vi);
  PVideoFrame second_frame1 = environment.get()->NewVideoFrame(vi);
  write_yuv_frame(first_frame0, first_y, first_u, first_v);
  write_yuv_frame(first_frame1, first_y, first_u, first_v);
  write_yuv_frame(second_frame0, second_y, second_u, second_v);
  write_yuv_frame(second_frame1, second_y, second_u, second_v);
  const auto first_before = FrameSnapshot::capture(first_frame1, vi);
  const auto second_before = FrameSnapshot::capture(second_frame1, vi);
  auto* first_clip = new FrameSequenceClip(vi, {first_frame0, first_frame1});
  auto* second_clip = new FrameSequenceClip(vi, {second_frame0, second_frame1});
  const PClip first(first_clip);
  const PClip second(second_clip);

  Subtract filter(first, second, environment.get());
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    for (int x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(x);
      EXPECT_EQ(output_y[x], subtract_reference(first_y[index], second_y[index], 126))
          << "Y x=" << x << " y=" << y;
      EXPECT_EQ(output_u[x], subtract_reference(first_u[index], second_u[index], 128))
          << "U x=" << x << " y=" << y;
      EXPECT_EQ(output_v[x], subtract_reference(first_v[index], second_v[index], 128))
          << "V x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(first_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(second_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(FrameSnapshot::capture(first_frame1, vi), first_before);
  EXPECT_EQ(FrameSnapshot::capture(second_frame1, vi), second_before);
}

std::uint8_t layer_add_reference(std::uint8_t base, std::uint8_t overlay,
                                 std::uint8_t overlay_alpha) {
  constexpr std::uint32_t max_value = 255;
  constexpr std::uint32_t half = 127;
  constexpr std::uint32_t opacity = 128;
  const auto effective_alpha =
      (static_cast<std::uint32_t>(overlay_alpha) * opacity + half) / max_value;
  const auto numerator = static_cast<std::uint32_t>(base) * (max_value - effective_alpha) +
                         static_cast<std::uint32_t>(overlay) * effective_alpha + half;
  return static_cast<std::uint8_t>(numerator / max_value);
}

std::uint8_t layer_rgb32_lighten_darken_reference(std::uint8_t base, std::uint8_t overlay,
                                                 std::uint8_t overlay_alpha, int base_luma,
                                                 int overlay_luma, bool lighten) {
  constexpr int level = 129;  // opacity=0.5 with an alpha-bearing 8-bit input
  constexpr int rounder = 128;
  constexpr int threshold = 5;
  const int alpha = (static_cast<int>(overlay_alpha) * level + 1) >> 8;
  const bool selected = lighten ? overlay_luma > base_luma + threshold
                                : overlay_luma < base_luma - threshold;
  const int effective_alpha = selected ? alpha : 0;
  return static_cast<std::uint8_t>(static_cast<int>(base) +
                                   (((static_cast<int>(overlay) - base) * effective_alpha + rounder) >> 8));
}

int layer_rgb32_luma(const Rgb32Pixel& pixel) {
  return (3736 * pixel.blue + 19234 * pixel.green + 9798 * pixel.red) >> 15;
}

struct LayerYuvFormatCase {
  int pixel_type;
  int width;
  int height;
  const char* name;
};

void PrintTo(const LayerYuvFormatCase& test_case, std::ostream* stream) { *stream << test_case.name; }

std::vector<PVideoFrame> make_layer_yuv_frames(AviSynthEnvironment& environment,
                                               const VideoInfo& vi, std::uint8_t base) {
  std::vector<PVideoFrame> frames;
  for (int frame_index = 0; frame_index < vi.num_frames; ++frame_index) {
    PVideoFrame frame = environment.get()->NewVideoFrame(vi);
    for (const int plane : video_frame_planes(vi)) {
      fill_plane_full_pitch(frame,
                            static_cast<std::uint8_t>(base + plane * 17 + frame_index * 23),
                            plane);
      write_frame_plane<std::uint8_t>(frame, plane, [base, plane, frame_index](int x, int y) {
        return static_cast<std::uint8_t>(base + plane * 17 + frame_index * 23 + x * 9 + y * 15);
      });
    }
    frames.push_back(frame);
  }
  return frames;
}

std::uint8_t layer_masked_u8(std::uint8_t base, std::uint8_t overlay, std::uint8_t mask) {
  constexpr int max_value = 255;
  constexpr int half = 127;
  constexpr int opacity = 128;
  const int effective_alpha = (static_cast<int>(mask) * opacity + half) / max_value;
  return static_cast<std::uint8_t>(
      (static_cast<int>(base) * (max_value - effective_alpha) +
       static_cast<int>(overlay) * effective_alpha + half) /
      max_value);
}

std::uint8_t layer_alpha_for_chroma(const PVideoFrame& overlay, int x, int y) {
  const int pitch = overlay->GetPitch(PLANAR_A);
  const auto* row0 = overlay->GetReadPtr(PLANAR_A) + (2 * y) * pitch;
  const auto* row1 = overlay->GetReadPtr(PLANAR_A) + (2 * y + 1) * pitch;
  const int sum = static_cast<int>(row0[2 * x]) + static_cast<int>(row0[2 * x + 1]) +
                  static_cast<int>(row1[2 * x]) + static_cast<int>(row1[2 * x + 1]);
  return static_cast<std::uint8_t>((sum + 2) >> 2);
}

class LayerYuvFormatTest : public ::testing::TestWithParam<LayerYuvFormatCase> {};

TEST_P(LayerYuvFormatTest, BlendsSubsampledPlanesAndPreservesBaseAlpha) {
  const auto& test_case = GetParam();
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{test_case.width, test_case.height,
                                                test_case.pixel_type, 2, 25, 1});
  auto base_frames = make_layer_yuv_frames(environment, vi, 19);
  auto overlay_frames = make_layer_yuv_frames(environment, vi, 137);
  const auto base_before = FrameSnapshot::capture(base_frames[1], vi);
  const auto overlay_before = FrameSnapshot::capture(overlay_frames[1], vi);
  auto* base_clip = new FrameSequenceClip(vi, base_frames);
  auto* overlay_clip = new FrameSequenceClip(vi, overlay_frames);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "Add", -1, 0, 0, 0, true, 0.5f, PLACEMENT_MPEG1,
               environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int width = output->GetRowSize(plane);
    const int height = output->GetHeight(plane);
    for (int y = 0; y < height; ++y) {
      const auto* base_row = base_frames[1]->GetReadPtr(plane) + y * base_frames[1]->GetPitch(plane);
      const auto* overlay_row =
          overlay_frames[1]->GetReadPtr(plane) + y * overlay_frames[1]->GetPitch(plane);
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < width; ++x) {
        std::uint8_t expected{};
        if (!vi.IsYUVA()) {
          expected = static_cast<std::uint8_t>((static_cast<int>(base_row[x]) + overlay_row[x] + 1) / 2);
        } else {
          const std::uint8_t mask = plane == PLANAR_Y
                                        ? overlay_frames[1]->GetReadPtr(PLANAR_A)[y * overlay_frames[1]->GetPitch(PLANAR_A) + x]
                                        : layer_alpha_for_chroma(overlay_frames[1], x, y);
          expected = layer_masked_u8(base_row[x], overlay_row[x], mask);
        }
        EXPECT_EQ(output_row[x], expected) << "format=" << test_case.name << " plane=" << plane
                                           << " x=" << x << " y=" << y;
      }
    }
  }
  if (vi.IsYUVA()) {
    EXPECT_EQ(read_frame_plane_active<std::uint8_t>(output, PLANAR_A),
              read_frame_plane_active<std::uint8_t>(base_frames[1], PLANAR_A));
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(FrameSnapshot::capture(base_frames[1], vi), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frames[1], vi), overlay_before);
}

INSTANTIATE_TEST_SUITE_P(
    FormatCases, LayerYuvFormatTest,
    ::testing::Values(LayerYuvFormatCase{VideoInfo::CS_YV12, 8, 4,
                                         "Yv12_Width8_Height4_AddAlphaFree"},
                      LayerYuvFormatCase{VideoInfo::CS_YV16, 8, 5,
                                         "Yv16_Width8_Height5_AddAlphaFree"},
                      LayerYuvFormatCase{VideoInfo::CS_YUVA420, 8, 4,
                                         "Yuva420_Width8_Height4_AddAlphaMask"}),
    [](const ::testing::TestParamInfo<LayerYuvFormatCase>& info) { return info.param.name; });

TEST(LayerFilter, AveragesYuvPlanesThroughFastMode) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_YV24, 2, 25, 1});
  auto base_frames = make_layer_yuv_frames(environment, vi, 23);
  auto overlay_frames = make_layer_yuv_frames(environment, vi, 149);
  const auto base_before = FrameSnapshot::capture(base_frames[1], vi);
  const auto overlay_before = FrameSnapshot::capture(overlay_frames[1], vi);
  auto* base_clip = new FrameSequenceClip(vi, base_frames);
  auto* overlay_clip = new FrameSequenceClip(vi, overlay_frames);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "Fast", -1, 0, 0, 0, true, 0.5f, PLACEMENT_MPEG1,
               environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    const int plane_width = output->GetRowSize(plane);
    const int plane_height = output->GetHeight(plane);
    for (int y = 0; y < plane_height; ++y) {
      const auto* base_row = base_frames[1]->GetReadPtr(plane) + y * base_frames[1]->GetPitch(plane);
      const auto* overlay_row =
          overlay_frames[1]->GetReadPtr(plane) + y * overlay_frames[1]->GetPitch(plane);
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < plane_width; ++x) {
        EXPECT_EQ(output_row[x], static_cast<std::uint8_t>((base_row[x] + overlay_row[x] + 1) / 2))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(FrameSnapshot::capture(base_frames[1], vi), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frames[1], vi), overlay_before);
}

struct LayerMulovrCase {
  int pixel_type;
  bool has_alpha;
  const char* name;
};

void PrintTo(const LayerMulovrCase& test_case, std::ostream* stream) { *stream << test_case.name; }

std::uint8_t layer_mulovr_u8_reference(std::uint8_t base, std::uint8_t overlay_y,
                                       std::uint8_t overlay_alpha, bool has_alpha, bool chroma) {
  constexpr int max_value = 255;
  constexpr int half = 127;
  constexpr int opacity_i = 128;
  const int alpha_eff = has_alpha
                            ? (static_cast<int>(overlay_alpha) * opacity_i + half) / max_value
                            : opacity_i;
  const int darken_factor =
      (alpha_eff * (max_value - static_cast<int>(overlay_y)) + half) / max_value;
  const int inv_keep = max_value - darken_factor;
  const int target = chroma ? 127 * darken_factor : 0;
  return static_cast<std::uint8_t>(
      (static_cast<int>(base) * inv_keep + target + half) / max_value);
}

class LayerMulovrTest : public ::testing::TestWithParam<LayerMulovrCase> {};

TEST_P(LayerMulovrTest, UsesOverlayLumaForYuvPlanesAndPreservesBaseAlpha) {
  const auto& test_case = GetParam();
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 2;
  const auto vi = make_video_info(
      VideoInfoSpec{width, height, test_case.pixel_type, 2, 25, 1});
  const auto overlay_vi = make_video_info(
      VideoInfoSpec{width, height, test_case.pixel_type, 1, 25, 1});

  constexpr std::array<std::uint8_t, 10> base_y{0, 32, 128, 200, 255,
                                                 17, 63, 127, 201, 240};
  constexpr std::array<std::uint8_t, 10> base_u{0, 64, 128, 192, 255,
                                                 15, 71, 127, 183, 239};
  constexpr std::array<std::uint8_t, 10> base_v{255, 192, 128, 64, 0,
                                                 240, 177, 113, 49, 7};
  constexpr std::array<std::uint8_t, 10> base_a{17, 31, 47, 63, 79,
                                                 95, 111, 127, 143, 159};
  constexpr std::array<std::uint8_t, 10> overlay_y{0, 64, 128, 192, 255,
                                                    7, 71, 135, 199, 247};
  constexpr std::array<std::uint8_t, 10> overlay_u{255, 1, 17, 233, 127,
                                                    3, 249, 89, 201, 45};
  constexpr std::array<std::uint8_t, 10> overlay_v{3, 250, 80, 10, 200,
                                                    251, 6, 176, 91, 220};
  constexpr std::array<std::uint8_t, 10> overlay_a{0, 64, 128, 192, 255,
                                                    255, 192, 128, 64, 0};

  auto write_values = [&](PVideoFrame& frame, bool overlay) {
    for (const int plane : video_frame_planes(vi)) {
      fill_plane_full_pitch(frame, static_cast<std::uint8_t>(0x40 + plane), plane);
    }
    const auto& y_values = overlay ? overlay_y : base_y;
    const auto& u_values = overlay ? overlay_u : base_u;
    const auto& v_values = overlay ? overlay_v : base_v;
    const auto& a_values = overlay ? overlay_a : base_a;
    write_frame_plane<std::uint8_t>(frame, PLANAR_Y, [&](int x, int y) {
      return y_values[static_cast<std::size_t>(y * width + x)];
    });
    write_frame_plane<std::uint8_t>(frame, PLANAR_U, [&](int x, int y) {
      return u_values[static_cast<std::size_t>(y * width + x)];
    });
    write_frame_plane<std::uint8_t>(frame, PLANAR_V, [&](int x, int y) {
      return v_values[static_cast<std::size_t>(y * width + x)];
    });
    if (test_case.has_alpha) {
      write_frame_plane<std::uint8_t>(frame, PLANAR_A, [&](int x, int y) {
        return a_values[static_cast<std::size_t>(y * width + x)];
      });
    }
  };

  PVideoFrame base_frame0 = environment.get()->NewVideoFrame(vi);
  PVideoFrame base_frame1 = environment.get()->NewVideoFrame(vi);
  PVideoFrame overlay_frame = environment.get()->NewVideoFrame(overlay_vi);
  write_values(base_frame0, false);
  write_values(base_frame1, false);
  write_values(overlay_frame, true);
  const auto base_before = FrameSnapshot::capture(base_frame1, vi);
  const auto overlay_before = FrameSnapshot::capture(overlay_frame, overlay_vi);
  auto* base_clip = new FrameSequenceClip(vi, {base_frame0, base_frame1});
  auto* overlay_clip = new StaticFrameClip(overlay_vi, overlay_frame);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "mulovr", -1, 0, 0, 0, true, 0.5f, PLACEMENT_MPEG1,
               environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* output_y = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* output_u = output->GetReadPtr(PLANAR_U) + y * output->GetPitch(PLANAR_U);
    const auto* output_v = output->GetReadPtr(PLANAR_V) + y * output->GetPitch(PLANAR_V);
    const auto* output_a = test_case.has_alpha
                               ? output->GetReadPtr(PLANAR_A) + y * output->GetPitch(PLANAR_A)
                               : nullptr;
    for (int x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y * width + x);
      const auto mask = test_case.has_alpha ? overlay_a[index] : 255;
      EXPECT_EQ(output_y[x], layer_mulovr_u8_reference(base_y[index], overlay_y[index], mask,
                                                       test_case.has_alpha, false))
          << "format=" << test_case.name << " plane=Y x=" << x << " y=" << y;
      EXPECT_EQ(output_u[x], layer_mulovr_u8_reference(base_u[index], overlay_y[index], mask,
                                                       test_case.has_alpha, true))
          << "format=" << test_case.name << " plane=U x=" << x << " y=" << y;
      EXPECT_EQ(output_v[x], layer_mulovr_u8_reference(base_v[index], overlay_y[index], mask,
                                                       test_case.has_alpha, true))
          << "format=" << test_case.name << " plane=V x=" << x << " y=" << y;
      if (test_case.has_alpha) {
        EXPECT_EQ(output_a[x], base_a[index])
            << "format=" << test_case.name << " plane=A x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(base_frame1, vi), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frame, overlay_vi), overlay_before);
}

INSTANTIATE_TEST_SUITE_P(
    FormatCases, LayerMulovrTest,
    ::testing::Values(LayerMulovrCase{VideoInfo::CS_YV24, false, "Yv24_OverlayLuma"},
                      LayerMulovrCase{VideoInfo::CS_YUVA444, true, "Yuva444_OverlayLumaAlpha"}),
    [](const ::testing::TestParamInfo<LayerMulovrCase>& info) { return info.param.name; });

std::uint16_t layer_mul_u16(std::uint16_t base, std::uint16_t overlay,
                            std::uint16_t mask, std::uint16_t alpha_target) {
  constexpr std::uint64_t max_value = 65535;
  constexpr std::uint64_t half = 32767;
  constexpr std::uint64_t opacity = 32768;
  const auto effective_alpha = (static_cast<std::uint64_t>(mask) * opacity + half) / max_value;
  const auto product = (static_cast<std::uint64_t>(base) * alpha_target) >> 16;
  return static_cast<std::uint16_t>(
      (static_cast<std::uint64_t>(base) * (max_value - effective_alpha) +
       product * effective_alpha + half) /
      max_value);
}

TEST(LayerFilter, BlendsPlanarRgbap16UsingMulAndOverlayAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 6;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_RGBAP16, 2, 25, 1});
  std::vector<PVideoFrame> base_frames;
  std::vector<PVideoFrame> overlay_frames;
  for (int frame_index = 0; frame_index < 2; ++frame_index) {
    PVideoFrame base = environment.get()->NewVideoFrame(vi);
    PVideoFrame overlay = environment.get()->NewVideoFrame(vi);
    for (const int plane : video_frame_planes(vi)) {
      fill_plane_full_pitch(base, 0x91, plane);
      fill_plane_full_pitch(overlay, 0x42, plane);
      write_frame_plane<std::uint16_t>(base, plane, [plane, frame_index](int x, int y) {
        return static_cast<std::uint16_t>(3000 + plane * 1700 + frame_index * 900 + x * 700 + y * 1100);
      });
      write_frame_plane<std::uint16_t>(overlay, plane, [plane, frame_index](int x, int y) {
        return static_cast<std::uint16_t>(39000 + plane * 2300 + frame_index * 500 + x * 900 + y * 1300);
      });
    }
    base_frames.push_back(base);
    overlay_frames.push_back(overlay);
  }
  const auto base_before = FrameSnapshot::capture(base_frames[1], vi);
  const auto overlay_before = FrameSnapshot::capture(overlay_frames[1], vi);
  auto* base_clip = new FrameSequenceClip(vi, base_frames);
  auto* overlay_clip = new FrameSequenceClip(vi, overlay_frames);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "Mul", -1, 0, 0, 0, true, 0.5f, PLACEMENT_MPEG2,
               environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (const int plane : video_frame_planes(vi)) {
    const int width_samples = output->GetRowSize(plane) / static_cast<int>(sizeof(std::uint16_t));
    const int plane_height = output->GetHeight(plane);
    const auto* mask_row = overlay_frames[1]->GetReadPtr(PLANAR_A);
    for (int y = 0; y < plane_height; ++y) {
      const auto* base_row = reinterpret_cast<const std::uint16_t*>(
          base_frames[1]->GetReadPtr(plane) + y * base_frames[1]->GetPitch(plane));
      const auto* overlay_row = reinterpret_cast<const std::uint16_t*>(
          overlay_frames[1]->GetReadPtr(plane) + y * overlay_frames[1]->GetPitch(plane));
      const auto* output_row = reinterpret_cast<const std::uint16_t*>(
          output->GetReadPtr(plane) + y * output->GetPitch(plane));
      const auto* alpha_row = reinterpret_cast<const std::uint16_t*>(
          mask_row + y * overlay_frames[1]->GetPitch(PLANAR_A));
      for (int x = 0; x < width_samples; ++x) {
        EXPECT_EQ(output_row[x],
                  layer_mul_u16(base_row[x], overlay_row[x], alpha_row[x], overlay_row[x]))
            << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(FrameSnapshot::capture(base_frames[1], vi), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frames[1], vi), overlay_before);
}

std::uint16_t layer_blend_u16(std::uint16_t base, std::uint16_t overlay,
                              std::uint16_t mask) {
  constexpr std::uint64_t max_value = 65535;
  constexpr std::uint64_t half = 32767;
  constexpr std::uint64_t opacity = 32768;
  const auto effective_alpha = (static_cast<std::uint64_t>(mask) * opacity + half) / max_value;
  return static_cast<std::uint16_t>(
      (static_cast<std::uint64_t>(base) * (max_value - effective_alpha) +
       static_cast<std::uint64_t>(overlay) * effective_alpha + half) /
      max_value);
}

TEST(LayerFilter, BlendsBgr64PackedChannelsUsingOverlayAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR64, 2, 25, 1});
  const std::vector<Rgb64Pixel> base_pixels{{0, 1000, 2000, 3000},
                                             {9000, 12000, 15000, 18000},
                                             {23000, 26000, 29000, 32000},
                                             {37000, 40000, 43000, 46000},
                                             {52000, 55000, 58000, 61000}};
  const std::vector<Rgb64Pixel> overlay_pixels{{65535, 50000, 40000, 0},
                                                {1000, 3000, 7000, 9000},
                                                {12000, 18000, 24000, 30000},
                                                {33000, 39000, 45000, 51000},
                                                {60000, 62000, 64000, 65535}};
  std::vector<PVideoFrame> base_frames;
  std::vector<PVideoFrame> overlay_frames;
  for (int frame_index = 0; frame_index < 2; ++frame_index) {
    PVideoFrame base = environment.get()->NewVideoFrame(vi);
    PVideoFrame overlay = environment.get()->NewVideoFrame(vi);
    fill_plane_full_pitch(base, 0xa1, DEFAULT_PLANE);
    fill_plane_full_pitch(overlay, 0xb2, DEFAULT_PLANE);
    write_bgr64(base, base_pixels);
    write_bgr64(overlay, overlay_pixels);
    base_frames.push_back(base);
    overlay_frames.push_back(overlay);
  }
  const auto base_before = FrameSnapshot::capture(base_frames[1], vi);
  const auto overlay_before = FrameSnapshot::capture(overlay_frames[1], vi);
  auto* base_clip = new FrameSequenceClip(vi, base_frames);
  auto* overlay_clip = new FrameSequenceClip(vi, overlay_frames);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "Add", -1, 0, 0, 0, true, 0.5f, PLACEMENT_MPEG1,
               environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* output_row = reinterpret_cast<const Rgb64Pixel*>(
        output->GetReadPtr() + y * output->GetPitch());
    for (int x = 0; x < width; ++x) {
      const auto& base_pixel = base_pixels[static_cast<std::size_t>(x)];
      const auto& overlay_pixel = overlay_pixels[static_cast<std::size_t>(x)];
      EXPECT_EQ(output_row[x].blue,
                layer_blend_u16(base_pixel.blue, overlay_pixel.blue, overlay_pixel.alpha))
          << "blue x=" << x << " y=" << y;
      EXPECT_EQ(output_row[x].green,
                layer_blend_u16(base_pixel.green, overlay_pixel.green, overlay_pixel.alpha))
          << "green x=" << x << " y=" << y;
      EXPECT_EQ(output_row[x].red,
                layer_blend_u16(base_pixel.red, overlay_pixel.red, overlay_pixel.alpha))
          << "red x=" << x << " y=" << y;
      EXPECT_EQ(output_row[x].alpha,
                layer_blend_u16(base_pixel.alpha, overlay_pixel.alpha, overlay_pixel.alpha))
          << "alpha x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(FrameSnapshot::capture(base_frames[1], vi), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frames[1], vi), overlay_before);
}

TEST(LayerFilter, BlendsRgb32ChannelsUsingOverlayAlphaAndExplicitOpacity) {
  AviSynthEnvironment environment;
  constexpr int width = 7;
  constexpr int height = 2;
  const auto vi_two_frames =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 2, 25, 1});
  const auto vi_one_frame =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});

  PVideoFrame base_frame0 = environment.get()->NewVideoFrame(vi_two_frames);
  PVideoFrame base_frame1 = environment.get()->NewVideoFrame(vi_two_frames);
  fill_plane_full_pitch(base_frame0, 0x81, DEFAULT_PLANE);
  fill_plane_full_pitch(base_frame1, 0x82, DEFAULT_PLANE);
  const std::vector<Rgb32Pixel> base_pixels{
      {0, 10, 20, 30},    {32, 64, 96, 128},    {255, 240, 200, 160}, {17, 33, 49, 65},
      {80, 96, 112, 128}, {144, 160, 176, 192}, {220, 230, 240, 250}};
  write_rgb32(base_frame0, base_pixels);
  write_rgb32(base_frame1, base_pixels);

  PVideoFrame overlay_frame = environment.get()->NewVideoFrame(vi_one_frame);
  fill_plane_full_pitch(overlay_frame, 0x93, DEFAULT_PLANE);
  const std::vector<Rgb32Pixel> overlay_pixels{
      {255, 200, 100, 0},  {1, 3, 5, 1},        {9, 27, 81, 64},   {32, 96, 160, 127},
      {64, 128, 192, 128}, {200, 100, 40, 254}, {0, 128, 255, 255}};
  write_rgb32(overlay_frame, overlay_pixels);

  const auto base_before = FrameSnapshot::capture(base_frame1, vi_two_frames);
  const auto overlay_before = FrameSnapshot::capture(overlay_frame, vi_one_frame);
  auto* base_clip = new FrameSequenceClip(vi_two_frames, {base_frame0, base_frame1});
  auto* overlay_clip = new StaticFrameClip(vi_one_frame, overlay_frame);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "Add", -1, 0, 0, 0, true, 0.5f, PLACEMENT_MPEG1,
               environment.get());
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      const auto& base_pixel = base_pixels[static_cast<std::size_t>(x)];
      const auto& overlay_pixel = overlay_pixels[static_cast<std::size_t>(x)];
      EXPECT_EQ(row[4 * x + 0],
                layer_add_reference(base_pixel.blue, overlay_pixel.blue, overlay_pixel.alpha))
          << "blue x=" << x << " y=" << y;
      EXPECT_EQ(row[4 * x + 1],
                layer_add_reference(base_pixel.green, overlay_pixel.green, overlay_pixel.alpha))
          << "green x=" << x << " y=" << y;
      EXPECT_EQ(row[4 * x + 2],
                layer_add_reference(base_pixel.red, overlay_pixel.red, overlay_pixel.alpha))
          << "red x=" << x << " y=" << y;
      EXPECT_EQ(row[4 * x + 3],
                layer_add_reference(base_pixel.alpha, overlay_pixel.alpha, overlay_pixel.alpha))
          << "alpha x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(base_frame1, vi_two_frames), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frame, vi_one_frame), overlay_before);
}

std::uint8_t layer_rgb32_subtract_without_chroma_reference(const Rgb32Pixel& overlay,
                                                           std::uint8_t base_value) {
  constexpr int level = 129;  // opacity=0.5 with an alpha-bearing 8-bit input
  constexpr int rounder = 128;
  const int alpha = (static_cast<int>(overlay.alpha) * level + 1) >> 8;
  const int inverse_overlay_luma =
      (3736 * (255 - overlay.blue) + 19234 * (255 - overlay.green) +
       9798 * (255 - overlay.red)) >>
      15;
  return static_cast<std::uint8_t>(
      static_cast<int>(base_value) +
      (((inverse_overlay_luma - static_cast<int>(base_value)) * alpha + rounder) >> 8));
}

TEST(LayerFilter, AppliesPackedRgbSubtractWithoutChromaUsingInvertedOverlayLuma) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 2;
  const auto vi_two_frames =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 2, 25, 1});
  const auto vi_one_frame =
      make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});
  const std::vector<Rgb32Pixel> base_pixels{
      {0, 20, 40, 60}, {32, 64, 96, 128}, {255, 240, 200, 160},
      {17, 83, 149, 65}, {220, 230, 240, 250}};
  const std::vector<Rgb32Pixel> overlay_pixels{
      {255, 200, 100, 0}, {1, 3, 5, 1}, {9, 27, 81, 64},
      {32, 96, 160, 128}, {0, 128, 255, 255}};

  PVideoFrame base_frame0 = environment.get()->NewVideoFrame(vi_two_frames);
  PVideoFrame base_frame1 = environment.get()->NewVideoFrame(vi_two_frames);
  PVideoFrame overlay_frame = environment.get()->NewVideoFrame(vi_one_frame);
  fill_plane_full_pitch(base_frame0, 0x81, DEFAULT_PLANE);
  fill_plane_full_pitch(base_frame1, 0x82, DEFAULT_PLANE);
  fill_plane_full_pitch(overlay_frame, 0x93, DEFAULT_PLANE);
  write_rgb32(base_frame0, base_pixels);
  write_rgb32(base_frame1, base_pixels);
  write_rgb32(overlay_frame, overlay_pixels);
  const auto base_before = FrameSnapshot::capture(base_frame1, vi_two_frames);
  const auto overlay_before = FrameSnapshot::capture(overlay_frame, vi_one_frame);
  auto* base_clip = new FrameSequenceClip(vi_two_frames, {base_frame0, base_frame1});
  auto* overlay_clip = new StaticFrameClip(vi_one_frame, overlay_frame);
  const PClip base(base_clip);
  const PClip overlay(overlay_clip);

  Layer filter(base, overlay, nullptr, "Subtract", -1, 0, 0, 0, false, 0.5f,
               PLACEMENT_MPEG1, environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(1, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* output_row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      const auto& base_pixel = base_pixels[static_cast<std::size_t>(x)];
      const auto& overlay_pixel = overlay_pixels[static_cast<std::size_t>(x)];
      const std::array<std::uint8_t, 4> expected{
          layer_rgb32_subtract_without_chroma_reference(overlay_pixel, base_pixel.blue),
          layer_rgb32_subtract_without_chroma_reference(overlay_pixel, base_pixel.green),
          layer_rgb32_subtract_without_chroma_reference(overlay_pixel, base_pixel.red),
          layer_rgb32_subtract_without_chroma_reference(overlay_pixel, base_pixel.alpha)};
      for (int component = 0; component < 4; ++component) {
        EXPECT_EQ(output_row[x * 4 + component], expected[static_cast<std::size_t>(component)])
            << "component=" << component << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
  EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(base_frame1, vi_two_frames), base_before);
  EXPECT_EQ(FrameSnapshot::capture(overlay_frame, vi_one_frame), overlay_before);
}

TEST(LayerFilter, SelectsStrictLumaThresholdForLightenAndDarken) {
  constexpr int width = 7;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 2, 25, 1});
  const std::vector<Rgb32Pixel> base_pixels{
      {50, 50, 50, 17},  {200, 200, 200, 31}, {100, 100, 100, 45},
      {100, 100, 100, 59}, {100, 100, 100, 73}, {100, 100, 100, 87},
      {30, 80, 140, 101}};
  const std::vector<Rgb32Pixel> overlay_pixels{
      {200, 200, 200, 255}, {50, 50, 50, 255}, {100, 100, 100, 255},
      {105, 105, 105, 255}, {106, 106, 106, 255}, {94, 94, 94, 255},
      {220, 40, 60, 0}};

  for (const auto& operation : {std::pair{"Lighten", true}, std::pair{"Darken", false}}) {
    AviSynthEnvironment environment;
    std::vector<PVideoFrame> base_frames;
    std::vector<PVideoFrame> overlay_frames;
    for (int frame_index = 0; frame_index < 2; ++frame_index) {
      PVideoFrame base = environment.get()->NewVideoFrame(vi);
      PVideoFrame overlay = environment.get()->NewVideoFrame(vi);
      fill_plane_full_pitch(base, static_cast<std::uint8_t>(0xa1 + frame_index), DEFAULT_PLANE);
      fill_plane_full_pitch(overlay, static_cast<std::uint8_t>(0xb2 + frame_index), DEFAULT_PLANE);
      write_rgb32(base, base_pixels);
      write_rgb32(overlay, overlay_pixels);
      base_frames.push_back(base);
      overlay_frames.push_back(overlay);
    }
    const auto base_before = FrameSnapshot::capture(base_frames[1], vi);
    const auto overlay_before = FrameSnapshot::capture(overlay_frames[1], vi);
    auto* base_clip = new FrameSequenceClip(vi, base_frames);
    auto* overlay_clip = new FrameSequenceClip(vi, overlay_frames);
    const PClip base(base_clip);
    const PClip overlay(overlay_clip);

    Layer filter(base, overlay, nullptr, operation.first, -1, 0, 0, 5, true, 0.5f,
                 PLACEMENT_MPEG1, environment.get());
    EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
    const PVideoFrame output = filter.GetFrame(1, environment.get());

    for (int y = 0; y < height; ++y) {
      const auto* output_row = output->GetReadPtr() + y * output->GetPitch();
      for (int x = 0; x < width; ++x) {
        const auto& base_pixel = base_pixels[static_cast<std::size_t>(x)];
        const auto& overlay_pixel = overlay_pixels[static_cast<std::size_t>(x)];
        const int base_luma = layer_rgb32_luma(base_pixel);
        const int overlay_luma = layer_rgb32_luma(overlay_pixel);
        const std::array<std::uint8_t, 4> expected{
            layer_rgb32_lighten_darken_reference(base_pixel.blue, overlay_pixel.blue,
                                                 overlay_pixel.alpha, base_luma, overlay_luma,
                                                 operation.second),
            layer_rgb32_lighten_darken_reference(base_pixel.green, overlay_pixel.green,
                                                 overlay_pixel.alpha, base_luma, overlay_luma,
                                                 operation.second),
            layer_rgb32_lighten_darken_reference(base_pixel.red, overlay_pixel.red,
                                                 overlay_pixel.alpha, base_luma, overlay_luma,
                                                 operation.second),
            layer_rgb32_lighten_darken_reference(base_pixel.alpha, overlay_pixel.alpha,
                                                 overlay_pixel.alpha, base_luma, overlay_luma,
                                                 operation.second)};
        for (int component = 0; component < 4; ++component) {
          EXPECT_EQ(output_row[x * 4 + component], expected[static_cast<std::size_t>(component)])
              << "operation=" << operation.first << " component=" << component << " x=" << x
              << " y=" << y;
        }
      }
    }
    EXPECT_NE(output->CheckMemory(), 1);
    EXPECT_EQ(base_clip->frame_requests(), std::vector<int>{1});
    EXPECT_EQ(overlay_clip->frame_requests(), std::vector<int>{1});
    EXPECT_EQ(FrameSnapshot::capture(base_frames[1], vi), base_before);
    EXPECT_EQ(FrameSnapshot::capture(overlay_frames[1], vi), overlay_before);
  }
}

}  // namespace
