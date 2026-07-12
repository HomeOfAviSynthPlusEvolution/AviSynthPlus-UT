#include <gtest/gtest.h>

#include "support/comparators.h"
#include "support/cpu_features.h"
#include "support/deterministic_data.h"
#include "support/guarded_video_buffer.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include "filters/intel/turn_avx2.h"
#include "filters/intel/turn_sse.h"
#include "filters/turn.h"

#include <array>
#include <cstdint>
#include <ostream>
#include <string>

namespace avsut::test {

using TurnVariant = Variant<TurnFuncPtr>;

void PrintTo(const TurnVariant& variant, std::ostream* stream) {
  *stream << variant.name;
}

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

class TurnLeftPlane8Variants : public ::testing::TestWithParam<TurnVariant> {};

std::string parameter_name(const TurnVariant& variant) {
  std::string result = "Variant";
  bool capitalize = true;
  for (const char character : variant.name) {
    if (character == '_' || character == '-') {
      capitalize = true;
      continue;
    }
    result.push_back(capitalize && character >= 'a' && character <= 'z'
                         ? static_cast<char>(character - ('a' - 'A'))
                         : character);
    capitalize = false;
  }
  return result;
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

TEST_P(TurnLeftPlane8Variants, MatchesScalarForTailWidthAndFixedRandomInput) {
  const auto& variant = GetParam();
  const auto features = CpuFeatures::detect();
  if (!variant_supported(variant, features)) {
    GTEST_SKIP() << "host does not support " << variant.name;
  }

  constexpr std::size_t width = 33;
  constexpr std::size_t height = 17;
  GuardedVideoBuffer<std::uint8_t> source(width, height, 40, 32);
  GuardedVideoBuffer<std::uint8_t> mapped(height, width, 32, 32);
  GuardedVideoBuffer<std::uint8_t> scalar(height, width, 32, 32);
  GuardedVideoBuffer<std::uint8_t> variant_output(height, width, 32, 32);
  fill_random(source.view(), 0xC0FFEEU);
  const auto source_snapshot = source.snapshot_active();

  turn_left(source.view().as_const(), scalar.view(), turn_left_plane_8_c);
  map_turn_left(source.view().as_const(), mapped.view());
  EXPECT_TRUE(compare_exact(mapped.view().as_const(), scalar.view().as_const()));
  turn_left(source.view().as_const(), variant_output.view(), variant.function);
  EXPECT_TRUE(compare_exact(scalar.view().as_const(), variant_output.view().as_const()));

  EXPECT_TRUE(source.active_matches(source_snapshot));
  EXPECT_TRUE(source.memory_intact());
  EXPECT_TRUE(mapped.memory_intact());
  EXPECT_TRUE(scalar.memory_intact());
  EXPECT_TRUE(variant_output.memory_intact());
  EXPECT_EQ(format_hash(hash_active(scalar.view().as_const())),
            "9d4f11c702db4abb");
}

INSTANTIATE_TEST_SUITE_P(
    Implementations,
    TurnLeftPlane8Variants,
    ::testing::Values(
        TurnVariant{"c", turn_left_plane_8_c, IsaRequirement::Scalar},
        TurnVariant{"sse2", turn_left_plane_8_sse2, IsaRequirement::Sse2},
        TurnVariant{"avx2", turn_left_plane_8_avx2, IsaRequirement::Avx2}),
    [](const ::testing::TestParamInfo<TurnVariant>& info) {
      return parameter_name(info.param);
    });

}  // namespace
}  // namespace avsut::test
