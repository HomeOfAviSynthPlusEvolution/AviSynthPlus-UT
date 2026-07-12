#include <gtest/gtest.h>

#include "support/comparators.h"
#include "support/cpu_features.h"
#include "support/deterministic_data.h"
#include "support/guarded_video_buffer.h"
#include "support/stable_hash.h"

#include "filters/intel/turn_avx2.h"
#include "filters/intel/turn_sse.h"
#include "filters/turn.h"

#include <array>
#include <cstdint>
#include <string>

namespace avsut::test {
namespace {

void turn_left(const PlaneView<const std::uint8_t> source,
               PlaneView<std::uint8_t> destination,
               TurnFuncPtr function) {
  function(source.data(), destination.data(),
           static_cast<int>(source.active_row_bytes()),
           static_cast<int>(source.height()),
           static_cast<int>(source.pitch_bytes()),
           static_cast<int>(destination.pitch_bytes()));
}

void map_turn_left(const PlaneView<const std::uint8_t> source,
                   PlaneView<std::uint8_t> destination) {
  for (std::size_t source_y = 0; source_y < source.height(); ++source_y) {
    for (std::size_t source_x = 0; source_x < source.width(); ++source_x) {
      destination.row(source.width() - 1 - source_x)[source_y] =
          source.row(source_y)[source_x];
    }
  }
}

TEST(TurnLeftPlane8, MapsCoordinatesForSmallFrame) {
  GuardedVideoBuffer<std::uint8_t> source(3, 2, 8);
  GuardedVideoBuffer<std::uint8_t> destination(2, 3, 8);
  fill_incrementing(source.view());

  turn_left(source.view().as_const(), destination.view(), turn_left_plane_8_c);

  const std::array<std::array<std::uint8_t, 2>, 3> expected{{
      {{2, 5}}, {{1, 4}}, {{0, 3}},
  }};
  for (std::size_t y = 0; y < expected.size(); ++y) {
    EXPECT_EQ(destination.view().row(y)[0], expected[y][0]);
    EXPECT_EQ(destination.view().row(y)[1], expected[y][1]);
  }
  EXPECT_TRUE(source.memory_intact());
  EXPECT_TRUE(destination.memory_intact());
}

TEST(TurnLeftPlane8, SimdMatchesScalarForTailWidthAndFixedRandomInput) {
  constexpr std::size_t width = 33;
  constexpr std::size_t height = 17;
  GuardedVideoBuffer<std::uint8_t> source(width, height, 40, 32);
  GuardedVideoBuffer<std::uint8_t> mapped(height, width, 32, 32);
  GuardedVideoBuffer<std::uint8_t> scalar(height, width, 32, 32);
  GuardedVideoBuffer<std::uint8_t> sse2(height, width, 32, 32);
  GuardedVideoBuffer<std::uint8_t> avx2(height, width, 32, 32);
  fill_random(source.view(), 0xC0FFEEU);
  const auto source_snapshot = source.snapshot_active();

  turn_left(source.view().as_const(), scalar.view(), turn_left_plane_8_c);
  map_turn_left(source.view().as_const(), mapped.view());
  EXPECT_TRUE(compare_exact(mapped.view().as_const(), scalar.view().as_const()));
  const auto features = CpuFeatures::detect();
  if (features.sse2) {
    turn_left(source.view().as_const(), sse2.view(), turn_left_plane_8_sse2);
    EXPECT_TRUE(compare_exact(scalar.view().as_const(), sse2.view().as_const()));
    EXPECT_TRUE(sse2.memory_intact());
  }
  if (features.avx2) {
    turn_left(source.view().as_const(), avx2.view(), turn_left_plane_8_avx2);
    EXPECT_TRUE(compare_exact(scalar.view().as_const(), avx2.view().as_const()));
    EXPECT_TRUE(avx2.memory_intact());
  }

  EXPECT_TRUE(source.active_matches(source_snapshot));
  EXPECT_TRUE(source.memory_intact());
  EXPECT_TRUE(mapped.memory_intact());
  EXPECT_TRUE(scalar.memory_intact());
  EXPECT_EQ(format_hash(hash_active(scalar.view().as_const())),
            "9d4f11c702db4abb");
}

}  // namespace
}  // namespace avsut::test
