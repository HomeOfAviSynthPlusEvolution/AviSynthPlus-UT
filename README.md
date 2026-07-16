# AviSynthPlus-UT

External C++17 unit tests for selected AviSynthPlus internal video and audio
kernels, plus selected public video filter classes. The upstream project is
pinned as a Git submodule; the test build never modifies it. Minimal production
fixes discovered by the tests are kept as separate upstream changes.

The suite targets internal video and public audio kernels, then directly
constructed public video filters with a real AviSynth environment and synthetic
input clips. Tests emphasize independent behavioral checks, deterministic
output, scalar/SIMD equivalence, active-pixel hashing, input immutability, row
padding, and allocation guards without modifying the upstream source.

## Build and run

```bash
git submodule update --init --recursive
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug
```

Use `clang19-debug` or `clang22-debug` for Clang, or `gcc-sanitize` for ASan
plus UBSan. Use `gcc-coverage` to build the GCC gcov instrumentation profile
used by the hosted source-coverage report.

The upstream `AvsCore` static library is built in a separate ExternalProject
build tree and linked as a normal imported target. The sanitizer preset applies
to the test executable and support code; the external upstream build remains a
separately configured dependency.

On Windows, use the MSVC x64 preset from a Visual Studio 2026 Developer
PowerShell:

```powershell
git submodule update --init --recursive
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

The Visual Studio 2026 generator requires a CMake version that supports it
(CMake 4.2 or newer).

Windows/MSVC is part of the supported test matrix. On pushes to `master` and
manual dispatches, GitHub Actions builds the `gcc-coverage` profile, runs
CTest, and publishes the latest test result plus full Linux/GCC-compiled
`AvsCore` coverage report to GitHub Pages. Enable GitHub Pages with the
`GitHub Actions` source in repository settings before the first deployment.

The coverage profile links a reporting-only inventory executable against every
object in the upstream static library. This records zero-count files as well
as executed files, so the report represents the full set of Linux/GCC-compiled
`AvsCore` sources. It does not include platform-specific sources that the
Linux upstream build does not compile.

## Layout

- `tests/support`: C++17 RAII buffers, typed plane views, deterministic data,
  comparison, hashing, CPU features, variant helpers, and reusable support for
  direct public-filter tests.
- `tests/turn`: Video-kernel unit tests and scalar/SIMD comparison cases.
- `tests/convolution`: Direct public-filter tests using a real environment and
  synthetic clips.
- `cmake/UpstreamTargets.cmake`: isolated full-`AvsCore` upstream build and
  imported target.
- `third_party/AviSynthPlus`: official pinned submodule.

Original files in this repository are MIT licensed. AviSynthPlus, GoogleTest,
and xxHash retain their respective upstream licenses. Compiled test binaries
are local/CI intermediates and are not published project artifacts.
