#include <gtest/gtest.h>

#include "support/comparators.h"
#include "support/cpu_features.h"
#include "support/deterministic_data.h"
#include "support/guarded_video_buffer.h"
#include "support/plane_view.h"
#include "support/stable_hash.h"
#include "support/variant_registry.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace avsut::test {
namespace {

TEST(PlaneView, AddressesRowsUsingBytePitch) {
  std::array<std::uint16_t, 16> pixels{};
  PlaneView<std::uint16_t> view(pixels.data(), 3, 2, 16);

  EXPECT_EQ(view.width(), 3U);
  EXPECT_EQ(view.height(), 2U);
  EXPECT_EQ(view.pitch_bytes(), 16U);
  EXPECT_EQ(view.active_row_bytes(), 6U);
  EXPECT_EQ(view.row(1), reinterpret_cast<std::uint16_t*>(
                             reinterpret_cast<std::uint8_t*>(pixels.data()) + 16));
  EXPECT_THROW(view.row(2), std::out_of_range);
}

TEST(PlaneView, RejectsPitchSmallerThanActiveRow) {
  std::array<std::uint16_t, 4> pixels{};
  EXPECT_THROW((PlaneView<std::uint16_t>(pixels.data(), 3, 1, 4)),
               std::invalid_argument);
}

TEST(GuardedVideoBuffer, DetectsPaddingAndGuardCorruption) {
  GuardedVideoBuffer<std::uint8_t> buffer(5, 3, 8, 32, 1);
  ASSERT_TRUE(buffer.memory_intact());
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(buffer.view().data()) % 32, 1U);

  buffer.view().row(0)[5] ^= 1;
  EXPECT_FALSE(buffer.padding_intact());
  buffer.reset_sentinels();
  ASSERT_TRUE(buffer.memory_intact());

  buffer.corrupt_suffix_guard_for_test();
  EXPECT_FALSE(buffer.guards_intact());
}

TEST(GuardedVideoBuffer, SnapshotsOnlyActivePixels) {
  GuardedVideoBuffer<std::uint8_t> buffer(3, 2, 5);
  buffer.view().row(0)[0] = 7;
  const auto snapshot = buffer.snapshot_active();
  buffer.view().row(0)[3] ^= 1;
  EXPECT_TRUE(buffer.active_matches(snapshot));
  buffer.view().row(1)[2] = 9;
  EXPECT_FALSE(buffer.active_matches(snapshot));
}

TEST(DeterministicData, XorShiftHasKnownSequence) {
  XorShift32 generator(1);
  EXPECT_EQ(generator.next(), 270369U);
  EXPECT_EQ(generator.next(), 67634689U);
}

TEST(DeterministicData, FixedSeedFillsActivePixelsOnly) {
  GuardedVideoBuffer<std::uint16_t> first(7, 3, 20);
  GuardedVideoBuffer<std::uint16_t> second(7, 3, 20);
  fill_random(first.view(), 0x12345678U);
  fill_random(second.view(), 0x12345678U);
  EXPECT_EQ(first.snapshot_active(), second.snapshot_active());
  EXPECT_TRUE(first.memory_intact());
  EXPECT_TRUE(second.memory_intact());
}

TEST(Comparators, ReportsIntegerCoordinates) {
  GuardedVideoBuffer<std::uint8_t> expected(4, 2, 8);
  GuardedVideoBuffer<std::uint8_t> actual(4, 2, 8);
  actual.view().row(1)[2] = 9;

  const auto result = compare_exact(expected.view().as_const(),
                                    actual.view().as_const());
  EXPECT_FALSE(result);
  EXPECT_NE(std::string(result.message()).find("row=1 col=2"), std::string::npos);
}

TEST(Comparators, UsesAbsoluteAndRelativeFloatTolerance) {
  std::array<float, 2> expected{0.0F, 1000.0F};
  std::array<float, 2> actual{0.000001F, 1000.01F};
  PlaneView<const float> expected_view(expected.data(), 2, 1, sizeof(expected));
  PlaneView<const float> actual_view(actual.data(), 2, 1, sizeof(actual));
  EXPECT_TRUE(compare_float(expected_view, actual_view,
                            FloatTolerance{0.00001F, 0.00002F}));
}

TEST(StableHash, ExcludesPaddingAndUsesXXH3) {
  GuardedVideoBuffer<std::uint8_t> buffer(3, 2, 8);
  fill_incrementing(buffer.view());
  const auto before = hash_active(buffer.view().as_const());
  buffer.view().row(0)[3] ^= 1;
  const auto after_padding_change = hash_active(buffer.view().as_const());
  EXPECT_EQ(before, after_padding_change);
  EXPECT_EQ(format_hash(before).size(), 16U);

  buffer.view().row(1)[2] ^= 1;
  EXPECT_NE(before, hash_active(buffer.view().as_const()));
}

TEST(CpuFeatures, RequirementChecksAreInjectable) {
  const CpuFeatures scalar{};
  const CpuFeatures avx2{true, true, true, true};
  EXPECT_FALSE(scalar.supports(IsaRequirement::Sse2));
  EXPECT_TRUE(avx2.supports(IsaRequirement::Avx2));
}

TEST(VariantRegistry, ReturnsOnlyRunnableVariants) {
  using Function = void (*)();
  const std::vector<Variant<Function>> variants{
      {"c", nullptr, IsaRequirement::Scalar},
      {"avx2", nullptr, IsaRequirement::Avx2},
  };
  const auto runnable = runnable_variants(variants, CpuFeatures{});
  ASSERT_EQ(runnable.size(), 1U);
  EXPECT_EQ(runnable.front().name, "c");
}

}  // namespace
}  // namespace avsut::test
