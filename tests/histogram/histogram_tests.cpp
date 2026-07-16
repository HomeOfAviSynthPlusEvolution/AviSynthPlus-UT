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
using avsut::test::write_frame_plane;

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

void write_yv24_levels_source(PVideoFrame& frame) {
  constexpr std::array<std::array<std::uint8_t, 4>, 2> y_values{{
      {{0, 1, 1, 7}},
      {{8, 9, 10, 11}},
  }};
  constexpr std::array<std::array<std::uint8_t, 4>, 2> u_values{{
      {{2, 2, 3, 4}},
      {{5, 6, 7, 8}},
  }};
  constexpr std::array<std::array<std::uint8_t, 4>, 2> v_values{{
      {{12, 13, 14, 14}},
      {{15, 16, 17, 18}},
  }};
  const auto write_values = [](PVideoFrame& target, int plane,
                               const auto& values) {
    const int pitch = target->GetPitch(plane);
    for (int y = 0; y < static_cast<int>(values.size()); ++y) {
      auto* row = target->GetWritePtr(plane) + y * pitch;
      for (int x = 0; x < static_cast<int>(values[static_cast<std::size_t>(y)].size()); ++x) {
        row[x] = values[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      }
    }
  };
  fill_plane_full_pitch(frame, 0xa1, PLANAR_Y);
  fill_plane_full_pitch(frame, 0xb2, PLANAR_U);
  fill_plane_full_pitch(frame, 0xc3, PLANAR_V);
  write_values(frame, PLANAR_Y, y_values);
  write_values(frame, PLANAR_U, u_values);
  write_values(frame, PLANAR_V, v_values);
}

histogram_color2_params no_color_overlays() {
  histogram_color2_params params{};
  params.graticule_type = histogram_color2_params::GRATICULE_OFF;
  return params;
}

TEST(Histogram, DrawsLevelsForEachYuvPlaneBesideKeptSource) {
  AviSynthEnvironment environment;
  constexpr int source_width = 4;
  constexpr int source_height = 2;
  const auto vi = make_video_info(
      VideoInfoSpec{source_width, source_height, VideoInfo::CS_YV24, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  write_yv24_levels_source(source);
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Histogram filter(clip, Histogram::ModeLevels, AVSValue(100.0), 8, true, false, nullptr,
                   no_color_overlays(), environment.get());
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, source_width + 256);
  EXPECT_EQ(output_vi.height, 256);
  EXPECT_EQ(output_vi.pixel_type, VideoInfo::CS_YV24);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  const PVideoFrame output = filter.GetFrame(0, environment.get());
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
    EXPECT_EQ(output->GetRowSize(plane), output_vi.width);
    EXPECT_EQ(output->GetHeight(plane), output_vi.height);
    for (int y = 0; y < source_height; ++y) {
      const auto* source_row = source->GetReadPtr(plane) + y * source->GetPitch(plane);
      const auto* output_row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      EXPECT_TRUE(std::equal(source_row, source_row + source_width, output_row))
          << "plane=" << plane << " row=" << y;
    }
  }

  const auto* output_y = output->GetReadPtr(PLANAR_Y);
  const int output_pitch = output->GetPitch(PLANAR_Y);
  const auto expect_bar = [&](int baseline, int value, int last_row) {
    EXPECT_EQ(output_y[last_row * output_pitch + source_width + value], 235)
        << "baseline=" << baseline << " value=" << value;
    EXPECT_EQ(output_y[(last_row - 1) * output_pitch + source_width + value], 16)
        << "baseline=" << baseline << " value=" << value;
  };
  expect_bar(64, 0, 34);    // One Y sample, maximum population is two.
  expect_bar(64, 1, 2);     // Two Y samples reach the full bar height.
  expect_bar(144, 2, 82);   // Two U samples reach the full bar height.
  expect_bar(144, 3, 114);
  expect_bar(224, 12, 194); // One V sample at the V baseline.
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>({0, 0}));
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

TEST(Histogram, PlotsSubsampledChromaInColorMode) {
  AviSynthEnvironment environment;
  constexpr int width = 4;
  constexpr int height = 4;
  const auto vi = make_video_info(
      VideoInfoSpec{width, height, VideoInfo::CS_YV12, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  fill_plane_full_pitch(source, 0x51, PLANAR_Y);
  fill_plane_full_pitch(source, 0x62, PLANAR_U);
  fill_plane_full_pitch(source, 0x73, PLANAR_V);
  const std::array<std::array<std::uint8_t, 2>, 2> u_values{{
      {{20, 40}},
      {{60, 80}},
  }};
  const std::array<std::array<std::uint8_t, 2>, 2> v_values{{
      {{30, 50}},
      {{70, 90}},
  }};
  for (int y = 0; y < 2; ++y) {
    auto* u_row = source->GetWritePtr(PLANAR_U) + y * source->GetPitch(PLANAR_U);
    auto* v_row = source->GetWritePtr(PLANAR_V) + y * source->GetPitch(PLANAR_V);
    for (int x = 0; x < 2; ++x) {
      u_row[x] = u_values[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      v_row[x] = v_values[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
    }
  }
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Histogram filter(clip, Histogram::ModeColor, AVSValue(), 8, false, false, nullptr,
                   no_color_overlays(), environment.get());
  const auto& output_vi = filter.GetVideoInfo();
  EXPECT_EQ(output_vi.width, 256);
  EXPECT_EQ(output_vi.height, 256);
  EXPECT_EQ(output_vi.pixel_type, VideoInfo::CS_YV12);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);

  const PVideoFrame output = filter.GetFrame(0, environment.get());
  const int y_pitch = output->GetPitch(PLANAR_Y);
  const auto* y_panel = output->GetReadPtr(PLANAR_Y);
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      const int u = u_values[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      const int v = v_values[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      EXPECT_EQ(y_panel[v * y_pitch + u], 17) << "u=" << u << " v=" << v;
    }
  }
  EXPECT_EQ(y_panel[1 * y_pitch + 1], 16);
  EXPECT_EQ(output->GetReadPtr(PLANAR_U)[0], 0);
  EXPECT_EQ(output->GetReadPtr(PLANAR_U)[127], 254);
  EXPECT_EQ(output->GetReadPtr(PLANAR_V)[0], 0);
  EXPECT_EQ(output->GetReadPtr(PLANAR_V)[127 * output->GetPitch(PLANAR_V)], 254);
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>({0, 0}));
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

std::uint8_t amplified_luma(std::uint8_t value) {
  const int pixel = static_cast<int>(value) << 4;
  return static_cast<std::uint8_t>((pixel & 256) ? (255 - (pixel & 0xff)) : (pixel & 0xff));
}

TEST(Histogram, AmplifiesLumaAndRestoresNeutralChromaAndAlpha) {
  AviSynthEnvironment environment;
  constexpr int width = 4;
  constexpr int height = 4;
  const auto vi = make_video_info(
      VideoInfoSpec{width, height, VideoInfo::CS_YUVA420, 1, 25, 1});
  PVideoFrame source = environment.get()->NewVideoFrame(vi);
  for (const int plane : {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A}) {
    fill_plane_full_pitch(source, static_cast<std::uint8_t>(0x80 + plane * 13), plane);
  }
  write_frame_plane<std::uint8_t>(source, PLANAR_Y,
                                  [](int x, int y) { return 3 + x * 11 + y * 17; });
  write_frame_plane<std::uint8_t>(source, PLANAR_U,
                                  [](int x, int y) { return 21 + x * 7 + y * 5; });
  write_frame_plane<std::uint8_t>(source, PLANAR_V,
                                  [](int x, int y) { return 201 - x * 9 - y * 3; });
  write_frame_plane<std::uint8_t>(source, PLANAR_A,
                                  [](int x, int y) { return 31 + x * 13 + y * 19; });
  const auto source_before = FrameSnapshot::capture(source, vi);
  auto* source_clip = new StaticFrameClip(vi, source);
  const PClip clip(source_clip);

  Histogram filter(clip, Histogram::ModeLuma, AVSValue(), 8, false, false, nullptr,
                   no_color_overlays(), environment.get());
  EXPECT_EQ(filter.GetVideoInfo().pixel_type, VideoInfo::CS_YUVA420);
  EXPECT_EQ(filter.SetCacheHints(CACHE_GET_MTMODE, 0), MT_NICE_FILTER);
  const PVideoFrame output = filter.GetFrame(0, environment.get());

  for (int y = 0; y < height; ++y) {
    const auto* output_row = output->GetReadPtr(PLANAR_Y) + y * output->GetPitch(PLANAR_Y);
    for (int x = 0; x < width; ++x) {
      const auto input = static_cast<std::uint8_t>(3 + x * 11 + y * 17);
      EXPECT_EQ(output_row[x], amplified_luma(input)) << "x=" << x << " y=" << y;
    }
  }
  for (const int plane : {PLANAR_U, PLANAR_V}) {
    for (int y = 0; y < output->GetHeight(plane); ++y) {
      const auto* row = output->GetReadPtr(plane) + y * output->GetPitch(plane);
      for (int x = 0; x < output->GetRowSize(plane); ++x) {
        EXPECT_EQ(row[x], 128) << "plane=" << plane << " x=" << x << " y=" << y;
      }
    }
  }
  for (int y = 0; y < height; ++y) {
    const auto* alpha_row = output->GetReadPtr(PLANAR_A) + y * output->GetPitch(PLANAR_A);
    for (int x = 0; x < width; ++x) {
      EXPECT_EQ(alpha_row[x], 255) << "alpha x=" << x << " y=" << y;
    }
  }
  EXPECT_NE(output->CheckMemory(), 1);
  EXPECT_EQ(source_clip->frame_requests(), std::vector<int>({0, 0}));
  EXPECT_EQ(FrameSnapshot::capture(source, vi), source_before);
}

}  // namespace
