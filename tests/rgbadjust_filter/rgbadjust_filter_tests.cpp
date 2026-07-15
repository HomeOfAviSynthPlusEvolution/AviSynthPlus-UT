#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_RGBADJUST_UNDEF_AVS_UNUSED
#endif
#include "filters/levels.h"
#ifdef AVSUT_RGBADJUST_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_RGBADJUST_UNDEF_AVS_UNUSED
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
using avsut::test::read_frame_plane_active;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;
using avsut::test::write_frame_plane;

std::uint8_t adjust_channel(std::uint8_t value, double scale, double bias) {
  const double normalized =
      std::clamp((bias + static_cast<double>(value) * scale) / 255.0, 0.0, 1.0);
  return static_cast<std::uint8_t>(std::pow(normalized, 1.0) * 255.0 + 0.5);
}

template <typename Pixel>
Pixel adjust_sample(Pixel value, double scale, double bias, double gamma,
                    double maximum_value) {
  const double normalized =
      std::clamp((bias + static_cast<double>(value) * scale) / maximum_value, 0.0, 1.0);
  return static_cast<Pixel>(std::pow(normalized, 1.0 / gamma) * maximum_value + 0.5);
}

float adjust_float(float value, double scale, double bias, double gamma) {
  return static_cast<float>(std::pow(std::clamp(bias + static_cast<double>(value) * scale, 0.0,
                                                 1.0),
                                     1.0 / gamma));
}

TEST(RGBAdjustFilter, AppliesPackedChannelScaleBiasAndAlphaMapping) {
  AviSynthEnvironment environment;
  constexpr int width = 8;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR32, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd1, DEFAULT_PLANE);
  const std::array<std::uint8_t, 8> blue{0, 5, 20, 64, 128, 200, 250, 255};
  const std::array<std::uint8_t, 8> green{3, 17, 31, 63, 127, 191, 239, 255};
  const std::array<std::uint8_t, 8> red{1, 16, 32, 80, 128, 180, 240, 255};
  const std::array<std::uint8_t, 8> alpha{0, 15, 64, 100, 128, 192, 240, 255};
  const int pitch = source->GetPitch();
  for (int y = 0; y < height; ++y) {
    auto* row = source->GetWritePtr() + y * pitch;
    for (int x = 0; x < width; ++x) {
      row[4 * x + 0] = blue[static_cast<std::size_t>(x)];
      row[4 * x + 1] = green[static_cast<std::size_t>(x)];
      row[4 * x + 2] = red[static_cast<std::size_t>(x)];
      row[4 * x + 3] = alpha[static_cast<std::size_t>(x)];
    }
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  RGBAdjust filter(clip, 0.5, 1.0, 1.0, 1.0, 8.0, 0.0, -6.0, 5.0, 1.0, 1.0, 1.0, 1.0, false, false,
                   false, "", environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* source_row = source->GetReadPtr() + y * source->GetPitch();
    const auto* output_row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      EXPECT_EQ(output_row[4 * x + 0], adjust_channel(blue[static_cast<std::size_t>(x)], 1.0, -6.0))
          << "blue x=" << x << " y=" << y;
      EXPECT_EQ(output_row[4 * x + 1], adjust_channel(green[static_cast<std::size_t>(x)], 1.0, 0.0))
          << "green x=" << x << " y=" << y;
      EXPECT_EQ(output_row[4 * x + 2], adjust_channel(red[static_cast<std::size_t>(x)], 0.5, 8.0))
          << "red x=" << x << " y=" << y;
      EXPECT_EQ(output_row[4 * x + 3], adjust_channel(alpha[static_cast<std::size_t>(x)], 1.0, 5.0))
          << "alpha x=" << x << " y=" << y;
      EXPECT_EQ(source_row[4 * x + 0], blue[static_cast<std::size_t>(x)]);
      EXPECT_EQ(source_row[4 * x + 1], green[static_cast<std::size_t>(x)]);
      EXPECT_EQ(source_row[4 * x + 2], red[static_cast<std::size_t>(x)]);
      EXPECT_EQ(source_row[4 * x + 3], alpha[static_cast<std::size_t>(x)]);
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(RGBAdjustFilter, AppliesScaleBiasToPackedBgr24TailPixels) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xc1, DEFAULT_PLANE);
  constexpr std::array<std::array<std::uint8_t, 3>, 5> pixels{{
      {{0, 3, 1}}, {{5, 17, 16}}, {{20, 31, 32}}, {{200, 191, 180}}, {{255, 255, 255}}}};
  write_frame_plane<std::uint8_t>(source, DEFAULT_PLANE, [&](int byte, int y) {
    const auto& pixel = pixels[static_cast<std::size_t>(byte / 3)];
    return static_cast<std::uint8_t>(pixel[static_cast<std::size_t>(byte % 3)] + y * 2);
  });
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  RGBAdjust filter(clip, 1.25, 0.75, 0.5, 1.0, 8.0, -5.0, 20.0, 0.0, 1.0, 1.0,
                   1.0, 1.0, false, false, false, "", environment.get());
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* row = output->GetReadPtr() + y * output->GetPitch();
    for (int x = 0; x < width; ++x) {
      const auto& pixel = pixels[static_cast<std::size_t>(x)];
      const auto blue = static_cast<std::uint8_t>(pixel[0] + y * 2);
      const auto green = static_cast<std::uint8_t>(pixel[1] + y * 2);
      const auto red = static_cast<std::uint8_t>(pixel[2] + y * 2);
      EXPECT_EQ(row[x * 3 + 0], adjust_channel(blue, 0.5, 20.0))
          << "blue x=" << x << " y=" << y;
      EXPECT_EQ(row[x * 3 + 1], adjust_channel(green, 0.75, -5.0))
          << "green x=" << x << " y=" << y;
      EXPECT_EQ(row[x * 3 + 2], adjust_channel(red, 1.25, 8.0))
          << "red x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(RGBAdjustFilter, AppliesSixteenBitScaleBiasToPackedBgr64IncludingAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 3;
  constexpr int height = 2;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_BGR64, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd2, DEFAULT_PLANE);
  constexpr std::array<std::array<std::uint16_t, 4>, 3> pixels{{
      {{0, 3, 1, 0}}, {{1024, 16384, 32768, 49152}}, {{65535, 60000, 50000, 65535}}}};
  write_frame_plane<std::uint16_t>(source, DEFAULT_PLANE, [&](int component, int y) {
    const auto& pixel = pixels[static_cast<std::size_t>(component / 4)];
    return static_cast<std::uint16_t>(pixel[static_cast<std::size_t>(component % 4)] + y * 17);
  });
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  RGBAdjust filter(clip, 0.8, 1.1, 0.6, 0.5, 2048.0, -1024.0, 4096.0, 8192.0, 1.0,
                   1.0, 1.0, 1.0, false, false, false, "", environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* row = reinterpret_cast<const std::uint16_t*>(output->GetReadPtr() +
                                                              y * output->GetPitch());
    for (int x = 0; x < width; ++x) {
      const auto& pixel = pixels[static_cast<std::size_t>(x)];
      const std::array<std::uint16_t, 4> expected{
          adjust_sample(static_cast<std::uint16_t>(pixel[0] + y * 17), 0.6, 4096.0, 1.0,
                        65535.0),
          adjust_sample(static_cast<std::uint16_t>(pixel[1] + y * 17), 1.1, -1024.0, 1.0,
                        65535.0),
          adjust_sample(static_cast<std::uint16_t>(pixel[2] + y * 17), 0.8, 2048.0, 1.0,
                        65535.0),
          adjust_sample(static_cast<std::uint16_t>(pixel[3] + y * 17), 0.5, 8192.0, 1.0,
                        65535.0)};
      for (int component = 0; component < 4; ++component) {
        EXPECT_EQ(row[x * 4 + component], expected[static_cast<std::size_t>(component)])
            << "component=" << component << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(RGBAdjustFilter, AppliesSixteenBitScaleBiasToPlanarRgbPlanes) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_RGBP16, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  for (const int plane : {PLANAR_G, PLANAR_B, PLANAR_R}) {
    fill_plane_full_pitch(source, static_cast<std::uint8_t>(0x71 + plane * 0x17), plane);
  }
  write_frame_plane<std::uint16_t>(source, PLANAR_B,
                                   [](int x, int y) { return 2000 + x * 3001 + y * 701; });
  write_frame_plane<std::uint16_t>(source, PLANAR_G,
                                   [](int x, int y) { return 4000 + x * 2101 + y * 503; });
  write_frame_plane<std::uint16_t>(source, PLANAR_R,
                                   [](int x, int y) { return 6000 + x * 1701 + y * 307; });
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  RGBAdjust filter(clip, 0.9, 1.2, 0.7, 1.0, 2048.0, -2048.0, 4096.0, 0.0, 1.0,
                   1.0, 1.0, 1.0, false, false, false, "", environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  const auto source_b = read_frame_plane_active<std::uint16_t>(source, PLANAR_B);
  const auto source_g = read_frame_plane_active<std::uint16_t>(source, PLANAR_G);
  const auto source_r = read_frame_plane_active<std::uint16_t>(source, PLANAR_R);
  const auto expected_b = [&]() {
    std::vector<std::uint16_t> values;
    values.reserve(source_b.size());
    for (const auto value : source_b) values.push_back(adjust_sample(value, 0.7, 4096.0, 1.0, 65535.0));
    return values;
  }();
  const auto expected_g = [&]() {
    std::vector<std::uint16_t> values;
    values.reserve(source_g.size());
    for (const auto value : source_g) values.push_back(adjust_sample(value, 1.2, -2048.0, 1.0, 65535.0));
    return values;
  }();
  const auto expected_r = [&]() {
    std::vector<std::uint16_t> values;
    values.reserve(source_r.size());
    for (const auto value : source_r) values.push_back(adjust_sample(value, 0.9, 2048.0, 1.0, 65535.0));
    return values;
  }();
  EXPECT_EQ(read_frame_plane_active<std::uint16_t>(output, PLANAR_B), expected_b);
  EXPECT_EQ(read_frame_plane_active<std::uint16_t>(output, PLANAR_G), expected_g);
  EXPECT_EQ(read_frame_plane_active<std::uint16_t>(output, PLANAR_R), expected_r);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(RGBAdjustFilter, AppliesFloatScaleBiasGammaToPlanarRgbAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 5;
  constexpr int height = 3;
  const auto vi = make_video_info(VideoInfoSpec{width, height, VideoInfo::CS_RGBAPS, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  for (const int plane : {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A}) {
    fill_plane_full_pitch(source, static_cast<std::uint8_t>(0x31 + plane * 0x19), plane);
  }
  write_frame_plane<float>(source, PLANAR_B,
                           [](int x, int y) { return 0.05F + x * 0.07F + y * 0.03F; });
  write_frame_plane<float>(source, PLANAR_G,
                           [](int x, int y) { return 0.15F + x * 0.05F + y * 0.04F; });
  write_frame_plane<float>(source, PLANAR_R,
                           [](int x, int y) { return 0.25F + x * 0.04F + y * 0.02F; });
  write_frame_plane<float>(source, PLANAR_A,
                           [](int x, int y) { return 0.35F + x * 0.03F + y * 0.05F; });
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  RGBAdjust filter(clip, 1.1, 0.8, 0.6, 0.7, -0.1, 0.15, 0.05, 0.2, 1.3, 0.9,
                   2.0, 1.1, false, false, false, "", environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  const auto check_plane = [&](int plane, double scale, double bias, double gamma) {
    const auto source_values = read_frame_plane_active<float>(source, plane);
    const auto output_values = read_frame_plane_active<float>(output, plane);
    ASSERT_EQ(output_values.size(), source_values.size());
    for (std::size_t index = 0; index < source_values.size(); ++index) {
      EXPECT_NEAR(output_values[index], adjust_float(source_values[index], scale, bias, gamma),
                  1.0e-6F)
          << "plane=" << plane << " active_index=" << index;
    }
  };
  check_plane(PLANAR_B, 0.6, 0.05, 2.0);
  check_plane(PLANAR_G, 0.8, 0.15, 0.9);
  check_plane(PLANAR_R, 1.1, -0.1, 1.3);
  check_plane(PLANAR_A, 0.7, 0.2, 1.1);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
