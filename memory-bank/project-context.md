# Project Context

## Purpose

AviSynthPlus-UT is an external C++17 unit-test repository for selected
AviSynthPlus internal video and audio kernels and selected public video and
audio filter classes. The repository tests upstream behavior without adding
source files to or rebuilding the upstream project inside the test source
tree. A test-triggered production defect may be fixed minimally in the
submodule working tree for verification and recorded separately from the
test-framework changes.

The upstream project is consumed as a pinned Git submodule. The baseline
submodule revision is `fcb9c8a205c1b01ee1ea491adba50e2217594598`; updating the
pointer or retaining a minimal production fix is an explicit, reviewed change.

## Scope

- Unit tests for internal video processing functions.
- Unit tests for public audio sample-conversion kernels.
- Unit tests for public video filter classes constructed directly with a real
  `IScriptEnvironment` and test-owned synthetic video clips.
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
  buffers, deterministic data, comparison, hashing, CPU-feature, variant, real
  environment, synthetic-video-clip, and frame-snapshot helpers.
- `tests/turn` and `tests/merge` contain direct unit tests for exposed Turn and
  Merge kernel functions.
- `tests/convolution` contains direct tests for public video filter classes.
- `tests/convert_audio` contains direct tests for exposed audio conversion
  functions.
- `tests/audio_filter` contains direct tests for public core audio filter
  classes using a real `IScriptEnvironment` and test-owned audio clips.

## Test Boundary

Tests may call upstream headers and functions that are already exposed by the
normal upstream build. They must not include upstream `.cpp` files, redefine
`static`, inject source into an upstream namespace, or maintain a partial-link
source list just to reach a file-local function. A production fix discovered by
these tests must remain a minimal upstream change and must not expose a private
function solely for testing.

The direct public-video-filter tier may include a public filter declaration and
construct that class directly against the normally built `AvsCore` library. It
uses a real `IScriptEnvironment` created by `CreateScriptEnvironment2()` and
test-owned concrete `IClip` implementations. Those clips provide controlled
frames, validate frame indexes, and record frame and cache-hint requests; they
are test doubles, not replacements for the script environment. The support
layer must not provide a general `IScriptEnvironment` mock, a universal
callback clip, or a video clip with silently ignored audio requests.

This tier tests a public constructor and `GetFrame` behavior only. It does not
call filter `Create` entry points, `IScriptEnvironment::Invoke`, registration,
plugin loading, or a complete filter graph. Explicit video information and
full-pitch frame snapshots belong to the test support layer so format metadata,
source immutability, and padding behavior remain observable.

The direct audio-kernel boundary is limited to public pointer/count conversion
functions. A separate audio-filter tier may construct public core audio filter
classes against a real `IScriptEnvironment` and strict test-owned audio clips.
It observes `GetAudio`, metadata, cache hints, and source request behavior but
does not call filter `Create` entry points, script registration, plugin
loading, or audio I/O.

SIMD functions are called only when their declared CPU feature is available.
Unsupported variants must not be invoked. This project does not infer hidden
ISA requirements, emulate unsupported CPUs, or audit instructions by
disassembly.

## Current Non-Goals

- Audio I/O and external audio plugins that require file/device backends.
- `.avs` script execution, filter `Create` entry points, `Invoke` conversion
  orchestration, or black-box filter-graph tests.
- Filter registration, full distribution, or plugin-loading tests.
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
