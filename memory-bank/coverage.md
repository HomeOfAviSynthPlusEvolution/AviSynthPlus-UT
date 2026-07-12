# Coverage Snapshot

This document records which plugin operations are covered, which upstream
implementation variants are exercised, and how the tests establish the
behavior. Exact dimensions, seeds, hashes, and parameterized instance names
belong to the test sources and test vectors.

| Plugin | Operation | Covered implementation types | Test method |
| --- | --- | --- | --- |
| `Support layer` | Plane views and guarded buffers | C++17 RAII views and buffers | Unit checks for byte-pitch row addressing, invalid pitches, active-pixel snapshots, row padding, and allocation guards |
| `Support layer` | Deterministic data and stable hashes | XorShift32 input generation and XXH3 64-bit hashing | Known-sequence and repeatability checks; hashes cover active bytes only and exclude padding |
| `Support layer` | CPU features and variant registry | Scalar, SSE2, SSSE3, AVX2, AVX2+FMA3, and AVX-512 requirement predicates | Injectable feature sets verify exact requirements; unsupported SIMD variants remain visible as skipped |
| `Limiter` | 8- and 16-bit plane clamp | 8-bit SSE2; 16-bit SSE2 and SSE4.1 | Fixed boundary-value pattern; independent exact clamp reference; in-place output, active-byte hash, padding, and guard checks |
| `Overlay` | Masked merge for `MASK444`, `MASK420`, and `MASK422` | Integer C, SSE4.1, and AVX2 for 8/10/12/14/16-bit paths; float C, SSE4.1, and AVX2 | Fixed boundary-value planes and masks; independent placement/opacity reference; C/SIMD differential comparison; integer active-byte hashes; floating-point ULP comparison; input immutability, padding, and guard checks |
| `PlaneSwap` | YUY2 UV byte swap and packed RGB32/RGB64 channel extraction | YUY2 SSE2 and SSSE3; RGB32/RGB64 SSE2 and AVX2 | Fixed channel-ramp packed layouts; independent byte/word position reference; bottom-up RGB row handling; active-output hashes; source immutability, padding, and guard checks |
| `Turn` | Planar quarter-turn rotation, left and right | 8-, 16-, and 32-bit C, SSE2, and AVX2 kernels | Fixed deterministic input; independent coordinate reference; scalar differential comparison; active-output hash; padding, guards, and source immutability checks |
| `Turn` | Packed RGB quarter-turn rotation, left and right | RGB24/RGB48 C kernels; RGB32/RGB64 C, SSE2, and AVX2 kernels | Fixed deterministic input; pixel-group coordinate reference using the upstream packed-RGB direction convention; scalar differential comparison; active-output hash; padding and guard checks |
| `Turn` | Planar 180-degree rotation | 8-, 16-, 32-, and 64-bit C, SSE2, and AVX2 kernels; SSSE3 for 8- and 16-bit kernels | Fixed deterministic input with vector-tail dimensions; coordinate reference; scalar differential comparison; active-output hash; padding, guards, and source immutability checks |
| `Merge` | Integer weighted plane merge | 8-, 10-, 12-, 14-, and 16-bit C, SSE2, and AVX2 kernels | Fixed deterministic two-plane input; independent integer reference; scalar differential comparison; active-output hash; padding, guards, and second-input immutability checks |
| `Merge` | Floating-point weighted plane merge | C, SSE2, and AVX2+FMA3 kernels | Fixed finite random and mixed-magnitude/cancellation inputs; independent double reference; hybrid 4-ULP and `1e-4` absolute-floor comparison; scalar differential, finite-output, padding, guards, and second-input immutability checks; raw float hashes intentionally omitted |

## Deliberate Gaps

- `Turn` YUY2 rotations: the upstream implementations are file-local
  `static` functions and are outside the public-kernel boundary.
- `Turn` 180-degree RGB24/RGB48 rotations: the required pixel types are
  private to the upstream implementation.
- `Turn` ARM/NEON variants: the current execution scope is Linux x86.
- `Turn` filter frame allocation, dispatch, and script-environment behavior:
  these are filter-level concerns rather than direct kernel unit tests.
- Exhaustive dimension, pitch, alignment-offset, pattern, and seed matrices.
- ConvertBits, audio, script execution, filter graphs, plugin loading,
  distribution integration, Windows/MSVC execution, and unsupported-ISA or
  FMA-specific dispatch auditing.

## Maintenance Rules

Update this file when a new plugin operation, implementation variant, or
meaningful behavioral branch is covered or removed. Keep exact test vectors,
expected hashes, and execution results in the test sources or generated test
reports rather than duplicating them here.
