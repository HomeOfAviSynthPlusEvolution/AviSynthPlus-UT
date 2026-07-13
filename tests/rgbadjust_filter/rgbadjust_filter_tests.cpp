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
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;

std::uint8_t adjust_channel(std::uint8_t value, double scale, double bias) {
  const double normalized =
      std::clamp((bias + static_cast<double>(value) * scale) / 255.0, 0.0, 1.0);
  return static_cast<std::uint8_t>(std::pow(normalized, 1.0) * 255.0 + 0.5);
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

}  // namespace
