# AI Contribution Instructions

Before analyzing, planning, or modifying this repository, read:

1. `memory-bank/README.md`.
2. The relevant memory-bank documents for the task, especially
   `project-context.md`, `testing-conventions.md`, and `coverage.md`.

Treat the memory bank as the project's durable source of conventions and
current test-coverage constraints. Do not infer a broader scope from an
individual test file when the memory bank defines a narrower boundary.

When adding or changing tests:

- Follow `testing-conventions.md` for CTest prefixes, GoogleTest names,
  parameterized instance names, variant labels, fixed seeds, and diagnostics.
- Check `coverage.md` before selecting new cases so that new tests add a
  distinct contract or parameter branch rather than duplicating coverage.
- Update `coverage.md` in the same change when the tested operation, variant,
  parameter matrix, or execution environment changes.

Keep AviSynthPlus as a read-only submodule. Do not add upstream source files to
the test target, patch upstream code, or commit build artifacts, generated
reports, temporary references, or compiler output.

Keep documentation under `docs/` out of Git commits. AI may create or update
those documents when requested, but must never stage or commit any file under
`docs/`.
