# Tests

## Goal

Provide a lightweight C++ test harness and fixtures that verify parser, model, row interval, legalization, metrics, validation, writer, and CLI behavior without requiring OpenROAD.

## Inputs

- `doc/proposal.md`: Validation plan lists parser, row interval, Abacus, legality, DOR, CLI, and public-flow checks.
- `doc/detailed-design.md`: Tests module calls for `tests/test_legalizer.cpp`, small synthetic fixtures, `make test`, and a CLI smoke case.

## Tasks

- [ ] Create a lightweight test harness in `tests/test_legalizer.cpp` with clear assertion diagnostics.
- [ ] Add tiny `.gp` fixtures for parser, row interval, legalizer, validator, metrics, and CLI smoke coverage.
- [ ] Wire `make test` to build the test binary, build `Legalizer`, run unit tests, and run one CLI smoke case.
- [ ] Group tests by module so failures identify the responsible component.
- [ ] Keep OpenROAD integration as a separate manual or scripted smoke step outside required unit tests.

## Done When

- [ ] `make test` runs locally without OpenROAD.
- [ ] Test failures print the test name, expected value, and actual value where applicable.
- [ ] The test suite covers the core acceptance criteria from the proposal and detailed design.
