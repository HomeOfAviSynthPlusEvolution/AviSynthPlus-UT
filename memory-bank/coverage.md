# Coverage Snapshot

This document records the current behavioral coverage and its parameter
dimensions. It is intentionally a contract-level inventory, not a generated
list of every source line or every CTest invocation.

## Snapshot Metadata

- Last reviewed: 2026-07-12.
- Current upstream submodule revision:
  `fcb9c8a205c1b01ee1ea491adba50e2217594598`.
- Test presets: `gcc-debug`, `clang19-debug`, `clang22-debug`, and
  `gcc-sanitize`.
- Current CTest count: 16 tests per configured build tree.
- Last verified state: all 16 tests passed under all four presets on the current
  host.

## Support Layer

| Area | Contracts covered | Parameter dimensions |
| --- | --- | --- |
| `PlaneView` | Byte-pitch row addressing; rejection of a pitch smaller than the active row | `uint16_t`; width 3; height 1 and 2; padded and invalid pitches |
| `GuardedVideoBuffer` | Padding and guard corruption detection; active-pixel snapshots exclude padding | `uint8_t`; width 5/3; height 3/2; alignment 32 with offset 1; tight and padded rows |
| `DeterministicData` | Known XorShift32 sequence; repeatable fixed-seed filling of active pixels | Seed `1` for the known sequence; seed `0x12345678`; `uint16_t`; width 7; height 3; pitch 20 |
| `Comparators` | Integer coordinate diagnostics; absolute and relative float tolerances | Integer mismatch at row 1, column 2; float values near zero and at magnitude 1000 |
| `StableHash` | XXH3 formatting; padding exclusion; active-byte sensitivity | `uint8_t`; width 3; height 2; pitch 8; incrementing pattern |
| `CpuFeatures` | Injectable scalar/SIMD requirement checks; distinct AVX-512 Foundation, Base, and Fast requirement predicates | Empty feature set, AVX2 feature set, AVX-512 Foundation-only set, and AVX-512 Base set |
| `VariantRegistry` | Reports whether a variant is supported without removing it from the test matrix | Scalar and AVX2 entries with an empty feature set |

The support layer currently has no GoogleTest `TEST_P` or
`INSTANTIATE_TEST_SUITE_P` instances. The Turn suite has one parameterized
test with three implementation instances; the rows above describe fixed
cases, not an implicit Cartesian product.

## Turn Video Kernels

| Operation | Implementations | Contracts and dimensions |
| --- | --- | --- |
| `turn_left_plane_8` | Parameterized C (`turn_left_plane_8_c`), SSE2 (`turn_left_plane_8_sse2`), and AVX2 (`turn_left_plane_8_avx2`) instances | Independent 3x2 coordinate mapping with incrementing input; 33x17 fixed-random differential case; source pitch 40; destination pitch 32; alignment 32; seed `0xC0FFEE`; active-output hash `9d4f11c702db4abb`; input, padding, and guards checked |

The C instance always runs. The SSE2 and AVX2 instances are independently
skipped when `CpuFeatures::detect()` reports that the host lacks the required
feature. The same rule applies to future SSSE3, SSE4.1, and AVX-512 instances;
an unavailable implementation must remain visible as skipped rather than
silently disappearing or passing.

## Not Covered Yet

The following are deliberate gaps, not silently assumed coverage:

- Turn right rotation and 180-degree rotation.
- Turn 16-bit and 32-bit planar paths.
- Turn RGB24, RGB32, RGB48, RGB64, and YUY2 paths.
- Exhaustive Turn dimension, pitch, alignment-offset, pattern, and seed
  matrices.
- ConvertBits and its bit-depth, range, dither, and float branches.
- Audio, script execution, filter graphs, plugin loading, and distribution
  integration.
- Windows/MSVC execution.
- Unsupported-ISA execution, illegal-instruction checks, and FMA-specific
  dispatch auditing.

## Maintenance Rules

Update this file when a new operation, implementation variant, parameter
dimension, execution preset, or explicit gap status changes. Summarize the
contract and meaningful dimensions; do not copy a complete test source file or
repeat every assertion name here.
