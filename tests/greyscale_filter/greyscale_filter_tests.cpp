#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_GREYSCALE_UNDEF_AVS_UNUSED
#endif
#include "filters/greyscale.h"
#ifdef AVSUT_GREYSCALE_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_GREYSCALE_UNDEF_AVS_UNUSED
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

TEST(Greyscale, NeutralizesPlanarChromaAndPreservesLuma) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 4, VideoInfo::CS_YV12, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa1, PLANAR_Y);
  fill_plane_full_pitch(source, 0xb2, PLANAR_U);
  fill_plane_full_pitch(source, 0xc3, PLANAR_V);
  write_values<std::uint8_t>(source, PLANAR_Y, vi.width, vi.height,
                             std::array<std::uint8_t, 8>{0, 16, 32, 64, 128, 192, 235, 255});
  write_values<std::uint8_t>(source, PLANAR_U, source->GetRowSize(PLANAR_U),
                             source->GetHeight(PLANAR_U),
                             std::array<std::uint8_t, 8>{0, 32, 96, 127, 128, 160, 224, 255});
  write_values<std::uint8_t>(source, PLANAR_V, source->GetRowSize(PLANAR_V),
                             source->GetHeight(PLANAR_V),
                             std::array<std::uint8_t, 8>{255, 224, 160, 128, 127, 96, 32, 0});
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Greyscale filter(clip, nullptr, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  const int luma_pitch = output->GetPitch(PLANAR_Y);
  const int source_luma_pitch = source->GetPitch(PLANAR_Y);
  for (int y = 0; y < vi.height; ++y) {
    const auto* output_row = output->GetReadPtr(PLANAR_Y) + y * luma_pitch;
    const auto* source_row = source->GetReadPtr(PLANAR_Y) + y * source_luma_pitch;
    EXPECT_EQ(std::vector<std::uint8_t>(output_row, output_row + vi.width),
              std::vector<std::uint8_t>(source_row, source_row + vi.width))
        << "luma row=" << y;
  }
  for (const int plane : {PLANAR_U, PLANAR_V}) {
    const int width = output->GetRowSize(plane);
    const int height = output->GetHeight(plane);
    const int pitch = output->GetPitch(plane);
    for (int y = 0; y < height; ++y) {
      const auto* row = output->GetReadPtr(plane) + y * pitch;
      for (int x = 0; x < width; ++x) {
        EXPECT_EQ(row[x], 128) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(Greyscale, NeutralizesInterleavedChromaForYuy2) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_YUY2, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd4, DEFAULT_PLANE);
  for (int y = 0; y < vi.height; ++y) {
    auto* row = source->GetWritePtr() + y * source->GetPitch();
    for (int x = 0; x < vi.width; ++x) {
      row[x * 2] = static_cast<std::uint8_t>(7 + x * 23 + y * 17);
      row[x * 2 + 1] = static_cast<std::uint8_t>(19 + x * 29 + y * 11);
    }
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Greyscale filter(clip, nullptr, environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < vi.height; ++y) {
    const auto* row = output->GetReadPtr() + y * output->GetPitch();
    const auto* source_row = source->GetReadPtr() + y * source->GetPitch();
    for (int x = 0; x < vi.width; ++x) {
      EXPECT_EQ(row[x * 2], source_row[x * 2]) << "luma x=" << x << " y=" << y;
      EXPECT_EQ(row[x * 2 + 1], 128) << "chroma x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
