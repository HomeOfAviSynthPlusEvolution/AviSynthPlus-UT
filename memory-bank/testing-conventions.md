# Testing Conventions

This document defines the stable naming and parameter-reporting rules for the
GoogleTest and CTest layers.

## Test Name Shape

The logical CTest name is:

```text
<module>.<google-test-suite>.<google-test-case>[/<instance-name>]
```

The module is supplied by CMake's `TEST_PREFIX` and is lower snake case. It
identifies the ownership area, not an individual source file. Current and
reserved examples are:

```text
support.
turn.
convert_bits.
```

The GoogleTest suite identifies the operation and the relevant element or
format. Use PascalCase, for example `PlaneView`, `TurnLeftPlane8`, or
`ConvertUInt8To16`.

The GoogleTest case identifies one observable behavior or contract. Use a
verb-led PascalCase phrase, for example:

```text
AddressesRowsUsingBytePitch
MapsCoordinatesForSmallFrame
MatchesScalarForTailWidthAndFixedRandomInput
```

Do not use `Test1`, `Smoke`, `Poc`, compiler names, host CPU names, pointer
addresses, timings, or implementation-detail names in a test name.

## CMake Registration

Each test module has a stable CTest prefix:

```cmake
gtest_discover_tests(turn_tests TEST_PREFIX "turn.")
```

Use a lower-snake-case prefix that remains valid if the module gains more
operations. Do not encode a temporary experiment, branch name, or current
number of cases in the prefix.

CTest target names follow the same module identity, for example
`turn_tests` and `convert_bits_tests`. A new module should have its own test
directory and CMake target rather than adding unrelated cases to an existing
target.

## Parameterized Tests

Use GoogleTest parameterized tests when the same behavior is evaluated across
multiple input, format, or implementation dimensions and each combination
should be independently selectable or reported:

```cpp
class ConvertBitsTest : public ::testing::TestWithParam<ConvertBitsCase> {};

INSTANTIATE_TEST_SUITE_P(
    FixedCases,
    ConvertBitsTest,
    ::testing::Values(
        ConvertBitsCase{"Width33_Height17_SeedC0FFEE"},
        ConvertBitsCase{"Width64_Height3_PatternCheckerboard"}),
    [](const ::testing::TestParamInfo<ConvertBitsCase>& info) {
      return info.param.name;
    });
```

Parameterized instance names must:

- Be deterministic, unique within the instantiation, and meaningful without
  reading the implementation.
- Use ASCII letters, digits, and underscores only; start with a letter.
- Include every input dimension that selects a distinct algorithm branch or
  materially changes the contract.
- Include a fixed seed as `Seed` followed by uppercase hexadecimal, such as
  `SeedC0FFEE`, when pseudorandom data is used.
- Name structured data explicitly, such as `PatternCheckerboard` or
  `PatternBoundaryValues`.
- Include source and destination bit depth, range, dither mode, channel kind,
  width, height, or pitch when that value is part of the tested branch.

Do not include the compiler, host CPU model, pointer address, wall-clock time,
or an unstable hash in an instance name. Those are environment or diagnostic
metadata, not test identity.

If a variant is a separate parameter, use a stable token such as `VariantC`,
`VariantSse2`, or `VariantAvx2`. If several variants are intentionally
compared inside one test, the test name may describe the shared invariant, but
the failure message must identify the variant that failed.

GoogleTest parameterized names are commonly rendered with a `/` between the
base case and instance name. Do not depend on a particular CTest punctuation
choice beyond the stable module prefix and the readable GoogleTest identity.

## Variants and CPU Features

Variant names are lower-case in registries and diagnostic labels (`c`, `sse2`,
`avx2`) and PascalCase when embedded in a parameterized instance name
(`VariantSse2`). The required CPU feature is declared beside the function
pointer in the variant registry.

A test must never execute an unsupported SIMD function. This rule applies to
every non-scalar level, including SSE2, SSSE3, SSE4.1, AVX2, and each AVX-512
feature subset. A host without AVX2 must skip AVX2 instances while continuing
to run scalar and any lower-level variants that it supports. An AVX-512
requirement must not be inferred from AVX2 support; the exact required AVX-512
subsets must be declared by the variant.

If a variant is represented as a separate parameterized instance, report it as
skipped when the host lacks the required feature. Do not turn an unavailable
implementation into a pass or silently remove it from the test list. If
variants are compared inside one test, make the conditional execution and its
coverage limitation explicit in the test diagnostic; prefer separate
instances when independent skip reporting is important.

## Stable Hashes and Random Data

Stable hash assertions require fixed structured input or a named fixed seed.
The hash is a change-detection signal, not a replacement for semantic pixel
assertions. Hash only active pixels; check padding and guards separately.

Guarded buffers assign a distinct deterministic padding sentinel to each
allocation. Do not override it with a shared value unless a test needs an
explicit boundary condition. This makes SIMD tail writes that copy source
padding into an output allocation observable.

Differential random tests also use fixed seeds. A failure report must include
the operation, variant, element or format, dimensions, pitch, alignment
offset when relevant, input pattern or seed, and first differing coordinates.
When a random case exposes a defect, preserve that seed as a named regression
case.

## Failure Diagnostics

Failure messages should identify, as applicable:

- Module and operation.
- Implementation variant.
- Element type, source and destination format, and conversion parameters.
- Width, height, source pitch, destination pitch, and alignment offset.
- Structured pattern or fixed seed.
- First differing row and column, expected value, actual value, and numeric
  error for floating-point comparisons.

Keep names stable even when diagnostic detail grows. Put verbose context in
the assertion message rather than making the test identity unwieldy.
