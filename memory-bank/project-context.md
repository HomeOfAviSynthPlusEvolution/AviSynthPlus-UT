# Project Context

## Purpose

AviSynthPlus-UT is an external C++17 unit-test repository for selected
AviSynthPlus internal video and audio kernels. The repository tests upstream
behavior without adding source files to or rebuilding the upstream project
inside the test source tree. A test-triggered production defect may be fixed
minimally in the submodule working tree for verification and recorded
separately from the test-framework changes.

The upstream project is consumed as a pinned Git submodule. The baseline
submodule revision is `fcb9c8a205c1b01ee1ea491adba50e2217594598`; updating the
pointer or retaining a minimal production fix is an explicit, reviewed change.

## Scope

- Unit tests for internal video processing functions.
- Unit tests for public audio sample-conversion kernels.
- Stable output checks using fixed structured inputs or fixed seeds.
- Differential checks between scalar C and available SIMD implementations.
- Memory-integrity checks for active pixels, row padding, and allocation guards.
- Linux GCC and Clang builds at the current stage, with C++17 throughout.

The test project owns its support layer and converts RAII views to the exact
raw-pointer and pitch arguments expected by upstream functions at the call
boundary.

## Architecture

- CMake and CMake Presets define configure, build, and test entry points.
- GoogleTest `v1.17.0` is acquired with `FetchContent`.
- xxHash `v0.8.3` provides explicit `XXH3_64bits`-based stable hashes.
- CTest discovers individual GoogleTest cases using module prefixes.
- `cmake/UpstreamTargets.cmake` configures and builds the upstream `AvsCore`
  static library in a separate external build tree and exposes it as an
  imported target.
- `tests/support` contains reusable C++17 RAII buffers, views, guarded audio
  buffers, deterministic data, comparison, hashing, CPU-feature, and variant
  helpers.
- `tests/turn` and `tests/merge` contain direct unit tests for exposed Turn and
  Merge kernel functions.
- `tests/convert_audio` will contain direct tests for exposed audio conversion
  functions.

## Test Boundary

Tests may call upstream headers and functions that are already exposed by the
normal upstream build. They must not include upstream `.cpp` files, redefine
`static`, inject source into an upstream namespace, or maintain a partial-link
source list just to reach a file-local function. A production fix discovered by
these tests must remain a minimal upstream change and must not expose a private
function solely for testing.

The direct audio boundary is limited to public pointer/count conversion
functions. Audio filter classes that require `PClip`, `IScriptEnvironment`,
cache behavior, or private resampling state remain outside the direct-kernel
scope.

SIMD functions are called only when their declared CPU feature is available.
Unsupported variants must not be invoked. This project does not infer hidden
ISA requirements, emulate unsupported CPUs, or audit instructions by
disassembly.

## Current Non-Goals

- Audio filter-level tests and audio I/O.
- `.avs` script execution or black-box filter-graph tests.
- Full distribution or plugin-loading tests.
- MSVC and Windows presets at the current stage.
- Performance benchmarks and unbounded fuzzing.
- ISA-dispatch contract testing, illegal-instruction testing, or FMA-specific
  environment simulation.
- Publishing test binaries, object files, or upstream static libraries.

## Licensing Boundary

Original test-framework content is MIT licensed. AviSynthPlus, GoogleTest, and
xxHash retain their upstream licenses. Build artifacts and test reports are
outputs of the test project and are not source distributions of the upstream
project.
