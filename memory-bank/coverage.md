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
| `Greyscale` | YUY2 neutral chroma and packed RGB32/RGB64 luma replication | YUY2 SSE2; RGB32 SSE2; RGB64 SSE4.1 | Fixed matrix-aware channel patterns; independent 15-bit fixed-point Rec601/Rec709 limited/full reference; alpha preservation; active-output hashes, padding, and guard checks |
| `Focus` | Planar horizontal and vertical three-tap blur/sharpen for 8-/16-bit and float samples, plus packed RGB32/RGB64 and YUY2 horizontal kernels | Planar 8-bit SSE2 and AVX2; planar 16-bit SSE2, SSE4.1, and AVX2; planar float SSE2; RGB32 SSE2; RGB64 SSE2 and SSE4.1; YUY2 SSE2 | Fixed boundary/ramp or finite-anchor inputs with blur and sharpen weights; independent edge-replicating fixed-point or floating-point reference; vector-tail coverage; integer active-output hashes, float ULP/absolute comparison, row padding, guards, source immutability, and line-buffer checks |
| `Layer` | YUV masked add through placement-aware Layer SIMD getters | 8- and 16-bit SSE4.1 and AVX2 wrappers, with scalar masked-merge baseline | Fixed boundary-value destination, overlay, and luma masks; independent MASK444/MASK420/MASK422 placement reference including MPEG1/MPEG2/TopLeft; full and partial opacity; active-output hashes, input immutability, padding, and guard checks |
| `Turn` | Planar quarter-turn rotation, left and right | 8-, 16-, and 32-bit C, SSE2, and AVX2 kernels | Fixed deterministic input; independent coordinate reference; scalar differential comparison; active-output hash; padding, guards, and source immutability checks |
| `Turn` | Packed RGB quarter-turn rotation, left and right | RGB24/RGB48 C kernels; RGB32/RGB64 C, SSE2, and AVX2 kernels | Fixed deterministic input; pixel-group coordinate reference using the upstream packed-RGB direction convention; scalar differential comparison; active-output hash; padding and guard checks |
| `Turn` | Planar 180-degree rotation | 8-, 16-, 32-, and 64-bit C, SSE2, and AVX2 kernels; SSSE3 for 8- and 16-bit kernels | Fixed deterministic input with vector-tail dimensions; coordinate reference; scalar differential comparison; active-output hash; padding, guards, and source immutability checks |
| `Merge` | Integer weighted plane merge | 8-, 10-, 12-, 14-, and 16-bit C, SSE2, and AVX2 kernels | Fixed deterministic two-plane input; independent integer reference; scalar differential comparison; active-output hash; padding, guards, and second-input immutability checks |
| `Merge` | Floating-point weighted plane merge | C, SSE2, and AVX2+FMA3 kernels | Fixed finite random and mixed-magnitude/cancellation inputs; independent double reference; hybrid 4-ULP and `1e-4` absolute-floor comparison; scalar differential, finite-output, padding, guards, and second-input immutability checks; raw float hashes intentionally omitted |
| `Merge` | YUY2 weighted chroma, weighted luma, and luma replacement | SSE2 kernels | Fixed boundary-value packed input with aligned vector blocks and scalar tails; independent byte-position and fixed-point reference; active-output hashes, second-input immutability, row padding, and allocation guard checks; public scalar counterparts are unavailable |
| `Merge` | Integer and floating-point plane averaging | 8-/16-bit SSE2 and AVX2; float SSE2 and AVX2 | Fixed boundary-value or finite-anchor inputs with unaligned addresses and vector tails; independent rounded-integer or double reference; SSE2/AVX2 differential through common expected output; integer active-output hashes, float ULP/absolute comparison, second-input immutability, row padding, and allocation guard checks |
| `Resize/Resample` | Planar integer resampling with fixed Triangle coefficients in both directions | Vertical: 8-bit SSE2 and AVX2; 10- and 16-bit SSE2 and AVX2. Horizontal: 8-bit and 10-/16-bit SSSE3 and AVX2 | Fixed coefficient program built through the public resampling setup; independent fixed-point reference; active-output XXH3 hashes; source immutability, row padding, and allocation guard checks |
| `Resize/Resample` | Planar float resampling with fixed Triangle coefficients in both directions | Vertical: SSE2 and AVX2+FMA3 memory-stream path; horizontal: SSSE3 and AVX2+FMA3 generic path | Fixed finite anchor pattern; independent double reference; 4-ULP comparison with `1e-4` absolute floor; finite-output, source immutability, row padding, and allocation guard checks; raw float hashes intentionally omitted |
| `Conditional/SAD` | Planar pixel sum and absolute-difference statistics | SSE2 pixel-sum, byte SAD, and 8-/16-bit SAD kernels; packed RGB32/RGB64 alpha-masked SAD | Fixed boundary/ramp byte and word planes with aligned SIMD blocks and scalar tails; independent exact sum/SAD reference; source immutability, row padding, and allocation guard checks |
| `ConvertBits` | Non-dithered integer depth and range conversion | SSE4.1 and AVX2 templates for 8-/16-bit source and destination storage | Fixed boundary-value inputs across representative 8/10/12/14/16-bit widening and narrowing, luma/chroma, full/limited, and limited-shift branches; independent numerical reference and cross-ISA exact comparison; active-output hashes, source immutability, row padding, and allocation guard checks |
| `ConvertBits` | Ordered-dither integer conversion | SSE4.1 and AVX2 templates for 8-/16-bit source and destination storage | Independently generated Bayer patterns cover 2x2, 4x4, 8x8, and 16x16 coordinate phases, odd/even depth differences, range remapping, lower-than-8-bit dither, and target-depth backscaling; exact cross-ISA output, active-output hashes, source immutability, row padding, and allocation guard checks |
| `ConvertBits` | Floating-point to integer depth and range conversion | SSE4.1 and AVX2+FMA3 templates for 8-/16-bit destinations | Fixed finite luma/chroma anchors cover full/limited mapping, lower/upper clamping, and 8/10/16-bit destinations; independent numerical reference, exact cross-ISA output, active-output hashes, source immutability, row padding, and allocation guard checks |
| `ConvertBits` | Integer to floating-point depth and range conversion | AVX2+FMA3 templates for 8-/16-bit sources | Fixed boundary-value luma/chroma inputs cover full/limited mapping; independent double reference with 4-ULP and `1e-4` absolute-floor comparison, finite output, source immutability, row padding, and allocation guard checks; raw float hashes intentionally omitted |

## Deliberate Gaps

- `Turn` YUY2 rotations: the upstream implementations are file-local
  `static` functions and are outside the public-kernel boundary.
- `Turn` 180-degree RGB24/RGB48 rotations: the required pixel types are
  private to the upstream implementation.
- `Turn` ARM/NEON variants: the current execution scope is Linux x86.
- `Turn` filter frame allocation, dispatch, and script-environment behavior:
  these are filter-level concerns rather than direct kernel unit tests.
- Exhaustive dimension, pitch, alignment-offset, pattern, and seed matrices.
- ConvertBits Floyd-Steinberg/filter-level paths and exhaustive conversion
  combinations; audio, script execution, filter graphs, plugin loading,
  distribution integration, Windows/MSVC execution, and unsupported-ISA or
  FMA-specific dispatch auditing.

## Maintenance Rules

Update this file when a new plugin operation, implementation variant, or
meaningful behavioral branch is covered or removed. Keep exact test vectors,
expected hashes, and execution results in the test sources or generated test
reports rather than duplicating them here.
