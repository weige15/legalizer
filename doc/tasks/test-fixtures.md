# Test Fixtures

## Goal

Provide small reproducible tests and fixtures that verify module behavior before running full OpenROAD validation.

## Inputs

- `doc/proposal.md`: Validation should include focused module tests and public benchmark checks through `flow.tcl`.
- `doc/detailed-design.md`: `make test` should cover parser, geometry, row intervals, legalizer behavior, density accounting, writer formatting, and a small end-to-end smoke test.

## Tasks

- [x] Create tiny `.gp` fixtures for one-cell, macro-split row, boundary blockage, density threshold, and overfull failure cases.
- [x] Build a lightweight C++ test binary using the same source modules as `Legalizer`.
- [x] Add test helpers for temporary TCL output using explicit file paths.
- [x] Wire `make test` to compile and run all fixture and in-memory tests.
- [x] Add an end-to-end smoke test that runs `./Legalizer 0.7 45 fixture.gp fixture.tcl`.
- [x] Document the public OpenROAD validation command using `flow.tcl` for both public benchmarks and two parameter settings.

## Done When

- [x] `make test` runs without OpenROAD and exercises each implementation module.
- [x] Fixture tests catch parser, geometry, legalization, density, and writer regressions.
- [x] Public benchmark validation steps are documented for final quality checks.
