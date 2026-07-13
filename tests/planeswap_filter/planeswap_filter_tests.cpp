#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_PLANESWAP_UNDEF_AVS_UNUSED
#endif
#include "filters/planeswap.h"
#ifdef AVSUT_PLANESWAP_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_PLANESWAP_UNDEF_AVS_UNUSED
#endif

#include "support/video_filter_test_support.h"

#include <gtest/gtest.h>

#include <array>
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
void write_values(PVideoFrame& frame, int plane, int width, int height,
                  const std::array<Pixel, 8>& values) {
  const int pitch = frame->GetPitch(plane);
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<Pixel*>(frame->GetWritePtr(plane) + y * pitch);
    for (int x = 0; x < width; ++x) {
      row[x] = values[static_cast<std::size_t>(x) % values.size()];
    }
  }
}

void expect_plane_equal(const PVideoFrame& expected, int expected_plane, const PVideoFrame& actual,
                        int actual_plane, int width, int height) {
  const int expected_pitch = expected->GetPitch(expected_plane);
  const int actual_pitch = actual->GetPitch(actual_plane);
  for (int y = 0; y < height; ++y) {
    const auto* expected_row = expected->GetReadPtr(expected_plane) + y * expected_pitch;
    const auto* actual_row = actual->GetReadPtr(actual_plane) + y * actual_pitch;
    for (int x = 0; x < width; ++x) {
      EXPECT_EQ(actual_row[x], expected_row[x]) << "x=" << x << " y=" << y;
    }
  }
}

TEST(PlaneSwap, SwapsPlanarUvPointersForYv24) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa1, PLANAR_Y);
  fill_plane_full_pitch(source, 0xb2, PLANAR_U);
  fill_plane_full_pitch(source, 0xc3, PLANAR_V);
  const std::array<std::uint8_t, 8> luma_values{0, 16, 32, 64, 128, 192, 235, 255};
  const std::array<std::uint8_t, 8> u_values{1, 3, 5, 7, 11, 13, 17, 19};
  const std::array<std::uint8_t, 8> v_values{2, 4, 6, 8, 12, 14, 18, 20};
  write_values<std::uint8_t>(source, PLANAR_Y, vi.width, vi.height, luma_values);
  write_values<std::uint8_t>(source, PLANAR_U, vi.width, vi.height, u_values);
  write_values<std::uint8_t>(source, PLANAR_V, vi.width, vi.height, v_values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  SwapUV filter(clip, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  expect_plane_equal(source, PLANAR_Y, output, PLANAR_Y, vi.width, vi.height);
  expect_plane_equal(source, PLANAR_V, output, PLANAR_U, vi.width, vi.height);
  expect_plane_equal(source, PLANAR_U, output, PLANAR_V, vi.width, vi.height);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

class SwapUvToYTest : public ::testing::TestWithParam<int> {};

TEST_P(SwapUvToYTest, ExtractsSubsampledPlanarChannel) {
  const int mode = GetParam();
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 4, VideoInfo::CS_YV12, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd4, PLANAR_Y);
  fill_plane_full_pitch(source, 0xe5, PLANAR_U);
  fill_plane_full_pitch(source, 0xf6, PLANAR_V);
  write_values<std::uint8_t>(source, PLANAR_Y, vi.width, vi.height,
                             std::array<std::uint8_t, 8>{0, 16, 32, 64, 128, 192, 235, 255});
  const std::array<std::uint8_t, 8> u_values{11, 22, 33, 44, 55, 66, 77, 88};
  const std::array<std::uint8_t, 8> v_values{101, 102, 103, 104, 105, 106, 107, 108};
  write_values<std::uint8_t>(source, PLANAR_U, source->GetRowSize(PLANAR_U),
                             source->GetHeight(PLANAR_U), u_values);
  write_values<std::uint8_t>(source, PLANAR_V, source->GetRowSize(PLANAR_V),
                             source->GetHeight(PLANAR_V), v_values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  SwapUVToY filter(clip, mode, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(), vi.width / 2);
  ASSERT_EQ(output->GetHeight(), vi.height / 2);
  const int source_plane = mode == SwapUVToY::UToY8 ? PLANAR_U : PLANAR_V;
  expect_plane_equal(source, source_plane, output, PLANAR_Y, vi.width / 2, vi.height / 2);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

INSTANTIATE_TEST_SUITE_P(Channels, SwapUvToYTest,
                         ::testing::Values(SwapUVToY::UToY8, SwapUVToY::VToY8),
                         [](const ::testing::TestParamInfo<int>& info) {
                           return info.param == SwapUVToY::UToY8 ? "UToY8" : "VToY8";
                         });

}  // namespace
