# AviSynthPlus-UT

External C++17 unit tests for selected AviSynthPlus internal video kernels.
The upstream project is pinned as a read-only Git submodule and is never
modified by the test build.

The suite targets internal video kernels and is designed to grow as coverage
is added. Tests emphasize independent behavioral checks, deterministic output,
scalar/SIMD equivalence, active-pixel hashing, input immutability, row padding,
and allocation guards without modifying the upstream source.

## Build and run

```bash
git submodule update --init --recursive
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug
```

Use `clang19-debug` or `clang22-debug` for Clang, or `gcc-sanitize` for ASan
plus UBSan.

The upstream `AvsCore` static library is built in a separate ExternalProject
build tree and linked as a normal imported target. The sanitizer preset applies
to the test executable and support code; the external upstream build remains a
separately configured dependency.

## Layout

- `tests/support`: C++17 RAII buffers, typed plane views, deterministic data,
  comparison, hashing, CPU features, and variant helpers.
- `tests/turn`: Video-kernel unit tests and scalar/SIMD comparison cases.
- `cmake/UpstreamTargets.cmake`: isolated full-`AvsCore` upstream build and
  imported target.
- `third_party/AviSynthPlus`: official pinned submodule.

Original files in this repository are MIT licensed. AviSynthPlus, GoogleTest,
and xxHash retain their respective upstream licenses. Compiled test binaries
are local/CI intermediates and are not published project artifacts.
