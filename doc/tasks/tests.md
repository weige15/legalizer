# Tests

## Goal

Provide fast unit and integration coverage for the modular legalizer without requiring OpenROAD.

## Inputs

- `doc/proposal.md`: Validation plan covering parser, row intervals, Abacus, legality, DOR, CLI, and public flow checks.
- `doc/detailed-design.md`: Tests module structure, fixture expectations, `make test`, and manual OpenROAD flow commands.

## Tasks

- [x] Create small `.gp` fixtures for one-cell, two-overlap, macro-split, blockage, malformed parser cases, and density examples.
- [x] Build a lightweight C++ test executable using `assert` or a tiny local check macro.
- [x] Cover parser, geometry, row interval construction, density, Abacus row solving, validator, writer, and CLI smoke behavior.
- [x] Wire `make test` to build and run unit tests plus one executable smoke test.
- [x] Keep OpenROAD-dependent flow checks documented as manual commands.
- [x] Add public benchmark notes for one high-`alpha` and one low-`alpha` run after implementation exists.

## Done When

- [x] `make test` runs without OpenROAD and fails on any unit or smoke-test failure.
- [x] Manual flow commands are documented for public benchmark validation.
