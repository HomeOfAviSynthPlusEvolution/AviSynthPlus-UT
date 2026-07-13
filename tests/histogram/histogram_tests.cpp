#include <avisynth.h>

#ifndef AVS_UNUSED
#define AVS_UNUSED(x) (void)(x)
#define AVSUT_HISTOGRAM_UNDEF_AVS_UNUSED
#endif
#include "filters/histogram.h"
#ifdef AVSUT_HISTOGRAM_UNDEF_AVS_UNUSED
#undef AVS_UNUSED
#undef AVSUT_HISTOGRAM_UNDEF_AVS_UNUSED
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

using InputRow = std::array<std::uint8_t, 8>;

void write_rows(PVideoFrame& frame, const std::array<InputRow, 2>& rows) {
  const int pitch = frame->GetPitch(PLANAR_Y);
  for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
    auto* row = frame->GetWritePtr(PLANAR_Y) + y * pitch;
    std::copy(rows[static_cast<std::size_t>(y)].begin(), rows[static_cast<std::size_t>(y)].end(),
              row);
  }
}

std::array<std::uint8_t, 256> expected_classic_row(const InputRow& input) {
  std::array<int, 256> histogram{};
  for (const std::uint8_t value : input) {
    ++histogram[value];
  }

  std::array<std::uint8_t, 256> exptab{};
  constexpr int tv_range_low = 16;
  constexpr int tv_range_high = 235;
  constexpr int range_luma = tv_range_high - tv_range_low;
  const double k = std::log(0.5 / range_luma) / 255.0;
  exptab[0] = static_cast<std::uint8_t>(tv_range_low);
  for (int i = 1; i < 255; ++i) {
    exptab[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(tv_range_low + 0.5 + range_luma * (1.0 - std::exp(i * k))));
  }
  exptab[255] = static_cast<std::uint8_t>(tv_range_high);

  std::array<std::uint8_t, 256> expected{};
  for (int x = 0; x < 256; ++x) {
    expected[static_cast<std::size_t>(x)] =
        exptab[static_cast<std::size_t>(std::min(255, histogram[static_cast<std::size_t>(x)]))];
  }
  return expected;
}

histogram_color2_params classic_params() {
  histogram_color2_params params{};
  params.graticule_type = histogram_color2_params::GRATICULE_OFF;
  return params;
}

TEST(Histogram, DrawsClassicPerRowHistogramWithStableMapping) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_Y8, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xc1, PLANAR_Y);
  const std::array<InputRow, 2> input{{
      InputRow{0, 16, 16, 128, 128, 128, 235, 255},
      InputRow{16, 16, 32, 32, 32, 32, 32, 240},
  }};
  write_rows(source, input);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Histogram filter(clip, Histogram::ModeClassic, AVSValue(), 8, false, false, nullptr,
                   classic_params(), environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), 256);
  ASSERT_EQ(output->GetHeight(PLANAR_Y), vi.height);
  for (int y = 0; y < vi.height; ++y) {
    const auto* output_row = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto expected = expected_classic_row(input[static_cast<std::size_t>(y)]);
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), output_row)) << "row=" << y;
  }
  EXPECT_NE(output->CheckMemory(), 1);
  const std::vector<int> first_requests{0, 0};
  EXPECT_EQ(source_clip->frame_requests(), first_requests);
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);

  const PVideoFrame repeat = filter.GetFrame(0, environment.get());
  for (int y = 0; y < vi.height; ++y) {
    EXPECT_TRUE(std::equal(output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y),
                           output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y) + 256,
                           repeat->GetReadPtr(PLANAR_Y) + y * repeat->GetPitch(PLANAR_Y)))
        << "row=" << y;
  }
  const std::vector<int> repeated_requests{0, 0, 0};
  EXPECT_EQ(source_clip->frame_requests(), repeated_requests);
}

TEST(Histogram, KeepsSourceToTheLeftOfClassicPanel) {
  AviSynthEnvironment environment;
  const auto vi = make_video_info(VideoInfoSpec{8, 2, VideoInfo::CS_Y8, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0xd2, PLANAR_Y);
  const std::array<InputRow, 2> input{{
      InputRow{0, 16, 16, 128, 128, 128, 235, 255},
      InputRow{16, 16, 32, 32, 32, 32, 32, 240},
  }};
  write_rows(source, input);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Histogram filter(clip, Histogram::ModeClassic, AVSValue(), 8, true, false, nullptr,
                   classic_params(), environment.get());
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  ASSERT_EQ(output->GetRowSize(PLANAR_Y), vi.width + 256);
  ASSERT_EQ(output->GetHeight(PLANAR_Y), vi.height);
  for (int y = 0; y < vi.height; ++y) {
    const auto* output_row = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    const auto* source_row = source->GetReadPtr(PLANAR_Y) + y * source->GetPitch(PLANAR_Y);
    EXPECT_TRUE(std::equal(source_row, source_row + vi.width, output_row)) << "row=" << y;
    const auto expected = expected_classic_row(input[static_cast<std::size_t>(y)]);
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), output_row + vi.width)) << "row=" << y;
  }
  EXPECT_NE(output->CheckMemory(), 1);
  const std::vector<int> expected_requests{0, 0};
  EXPECT_EQ(source_clip->frame_requests(), expected_requests);
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
