# AviSynthPlus-UT Memory Bank

This directory stores durable project knowledge for contributors and AI
agents. It describes the test framework's scope, conventions, and current
coverage without replacing the source code or generated build metadata.

## Read Order

1. `project-context.md` for scope, architecture, and non-goals.
2. `testing-conventions.md` for test names, parameters, variants, and failure
   diagnostics.
3. `coverage.md` for the current test matrix, executed variants, and known
   gaps.

## Document Roles

- `project-context.md` records decisions that should remain stable while the
  suite grows.
- `testing-conventions.md` is normative. New tests should follow it unless a
  deliberate change is made to the document first.
- `coverage.md` is a maintained snapshot of coverage. It describes contracts
  and parameter dimensions rather than acting as a second source file list.

## Update Policy

- Keep these documents in English and use ASCII text unless a technical name
  requires otherwise.
- Record durable rules and decisions, not transient build output or a running
  conversation transcript.
- Update `coverage.md` when a tested operation, implementation variant,
  parameter dimension, or supported execution environment changes.
- Do not update `coverage.md` for a refactor that leaves the tested contract
  unchanged.
- Keep generated files, test reports, hashes from ad-hoc experiments, and
  compiler logs out of this directory.
- If a source change conflicts with a memory-bank rule, resolve the rule or
  record an explicit decision before merging the source change.

## AI Workflow

Before analyzing or modifying this repository, read this file and then the
memory-bank documents relevant to the task. Treat the conventions as project
constraints, not optional suggestions. When adding coverage, compare the
result with `coverage.md` and update that snapshot in the same change when the
coverage contract has changed.
