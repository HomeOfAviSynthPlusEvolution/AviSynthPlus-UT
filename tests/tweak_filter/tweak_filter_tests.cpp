#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_TWEAK_UNDEF_AVS_UNUSED
#endif
#include "filters/levels.h"
#ifdef AVSUT_TWEAK_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_TWEAK_UNDEF_AVS_UNUSED
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
using avsut::test::FrameSnapshot;
using avsut::test::make_video_info;
using avsut::test::StaticFrameClip;
using avsut::test::VideoInfoSpec;

template <std::size_t N>
void write_values(PVideoFrame& frame, int plane, const std::array<std::uint8_t, N>& values) {
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

TEST(TweakFilter, AppliesEightBitLumaBrightnessWithoutTouchingSource) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_Y8, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xa7, PLANAR_Y);
  const std::array<std::uint8_t, 8> values{0, 1, 16, 64, 128, 200, 243, 255};
  write_values(source, PLANAR_Y, values);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Tweak filter(clip, 0.0, 1.0, 12.0, 1.0, false, 0.0, 360.0, 150.0, 0.0, 0.0, false, true, 1.0,
               environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < vi.height; ++y) {
    const auto* source_row = source->GetReadPtr(PLANAR_Y) + y * source->GetPitch(PLANAR_Y);
    const auto* output_row = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    for (int x = 0; x < vi.width; ++x) {
      const int expected = std::clamp(static_cast<int>(source_row[x]) + 12, 0, 255);
      EXPECT_EQ(output_row[x], expected) << "x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>{0});
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
