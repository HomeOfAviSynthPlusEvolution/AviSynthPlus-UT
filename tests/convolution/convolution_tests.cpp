#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_CONVOLUTION_UNDEF_AVS_UNUSED
#endif
#include "filters/convolution.h"
#ifdef AVSUT_CONVOLUTION_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_CONVOLUTION_UNDEF_AVS_UNUSED
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

template <typename Pixel>
void fill_ramp(PVideoFrame& frame, int width, int height, int plane = PLANAR_Y) {
  const int pitch = frame->GetPitch(plane);
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<Pixel*>(frame->GetWritePtr(plane) + y * pitch);
    for (int x = 0; x < width; ++x) {
      row[x] = static_cast<Pixel>(x * 7 + y * 19 + 3);
    }
  }
}

void fill_clipping_pattern(PVideoFrame& frame, int width, int height, int plane = PLANAR_Y) {
  const int pitch = frame->GetPitch(plane);
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<std::uint16_t*>(frame->GetWritePtr(plane) + y * pitch);
    for (int x = 0; x < width; ++x) {
      row[x] = ((x + y) % 4 == 0) ? 0 : static_cast<std::uint16_t>(900 + ((x * 17 + y * 23) % 124));
    }
  }
}

template <typename Pixel, std::size_t Size>
Pixel expected_integer_pixel(const PVideoFrame& source, int x, int y, int width, int height,
                             const std::array<int, Size>& matrix, int divisor, int bias,
                             int bits_per_pixel) {
  const int limit = static_cast<int>(std::sqrt(static_cast<double>(Size))) / 2;
  long long sum = 0;
  for (int yy = -limit; yy <= limit; ++yy) {
    const int source_y = std::clamp(y + yy, 0, height - 1);
    const auto* row = reinterpret_cast<const Pixel*>(source->GetReadPtr(PLANAR_Y) +
                                                     source_y * source->GetPitch(PLANAR_Y));
    for (int xx = -limit; xx <= limit; ++xx) {
      const int source_x = std::clamp(x + xx, 0, width - 1);
      sum += static_cast<long long>(row[source_x]) *
             matrix[(yy + limit) * (2 * limit + 1) + xx + limit];
    }
  }
  const long long rounded =
      (sum >= 0) ? (sum + divisor / 2) / divisor : (sum - divisor / 2) / divisor;
  const long long clipped = std::clamp(rounded + bias, 0LL, (1LL << bits_per_pixel) - 1);
  return static_cast<Pixel>(clipped);
}

TEST(GeneralConvolution, AppliesThreeByThreeIntegerKernelWithReplicatedEdges) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{7, 5, VideoInfo::CS_Y8, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa5, PLANAR_Y);
  fill_ramp<std::uint8_t>(source, vi.width, vi.height);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  GeneralConvolution filter(clip, 4.0, 2.0f, "0 1 0 1 0 1 0 1 0", false, true, false, false,
                            environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(), vi.width);
  ASSERT_EQ(output->GetHeight(), vi.height);
  for (int y = 0; y < vi.height; ++y) {
    const auto* row = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    for (int x = 0; x < vi.width; ++x) {
      const auto expected = expected_integer_pixel<std::uint8_t>(
          source, x, y, vi.width, vi.height, std::array<int, 9>{0, 1, 0, 1, 0, 1, 0, 1, 0}, 4, 2,
          8);
      EXPECT_EQ(row[x], expected) << "x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_TRUE(FrameSnapshot::capture(source, vi) == source_before);
}

TEST(GeneralConvolution, AppliesFiveByFiveIntegerKernelAndClipsToBitDepth) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{6, 4, VideoInfo::CS_YUV444P10, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0x5a, PLANAR_Y);
  fill_clipping_pattern(source, vi.width, vi.height);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  GeneralConvolution filter(clip, 16.0, 0.0f, "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1",
                            false, true, false, false, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), vi.width * static_cast<int>(sizeof(std::uint16_t)));
  const auto* output_base = output->GetReadPtr(PLANAR_Y);
  bool saw_clipped_value = false;
  for (int y = 0; y < vi.height; ++y) {
    const auto* row =
        reinterpret_cast<const std::uint16_t*>(output_base + y * output->GetPitch(PLANAR_Y));
    for (int x = 0; x < vi.width; ++x) {
      const auto expected = expected_integer_pixel<std::uint16_t>(
          source, x, y, vi.width, vi.height,
          std::array<int, 25>{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                              1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
          16, 0, 10);
      saw_clipped_value = saw_clipped_value || expected == 1023;
      EXPECT_EQ(row[x], expected) << "x=" << x << " y=" << y;
    }
  }
  EXPECT_TRUE(saw_clipped_value);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_TRUE(FrameSnapshot::capture(source, vi) == source_before);
}

TEST(GeneralConvolution, AppliesThreeByThreeFloatKernelWithReplicatedEdges) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{5, 4, VideoInfo::CS_Y32, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xcd, PLANAR_Y);
  fill_ramp<float>(source, vi.width, vi.height);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  constexpr std::array<float, 9> matrix{0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f, 0.25f, 0.0f};
  GeneralConvolution filter(clip, 1.0, 0.125f, "0 .25 0 .25 0 .25 0 .25 0", false, true, false,
                            false, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < vi.height; ++y) {
    const auto* row = reinterpret_cast<const float*>(output->GetReadPtr(PLANAR_Y) +
                                                     y * output->GetPitch(PLANAR_Y));
    for (int x = 0; x < vi.width; ++x) {
      float expected = 0.125f;
      for (int yy = -1; yy <= 1; ++yy) {
        const int source_y = std::clamp(y + yy, 0, vi.height - 1);
        const auto* source_row = reinterpret_cast<const float*>(
            source->GetReadPtr(PLANAR_Y) + source_y * source->GetPitch(PLANAR_Y));
        for (int xx = -1; xx <= 1; ++xx) {
          const int source_x = std::clamp(x + xx, 0, vi.width - 1);
          expected += source_row[source_x] * matrix[(yy + 1) * 3 + xx + 1];
        }
      }
      EXPECT_FLOAT_EQ(row[x], expected) << "x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_TRUE(FrameSnapshot::capture(source, vi) == source_before);
}

}  // namespace
