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
#include <vector>

namespace {

using avsut::test::AviSynthEnvironment;
using avsut::test::fill_plane_full_pitch;
using avsut::test::FrameSequenceClip;
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;

struct Rgb32Pixel {
  std::uint8_t blue;
  std::uint8_t green;
  std::uint8_t red;
  std::uint8_t alpha;
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

}  // namespace
