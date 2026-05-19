# Tests

## Goal

Create a focused test harness and fixtures that validate each module and the assignment executable path.

## Inputs

- `doc/proposal.md`: Validation should cover parsing, snapping, row intervals, cluster placement, TCL output, legality, public benchmark smoke tests, and runtime.
- `doc/detailed-design.md`: Existing `Makefile` expects `tests/test_legalizer.cpp` and `make test`.

## Tasks

- [ ] Create `tests/test_legalizer.cpp` with lightweight assertions for all implemented modules.
- [ ] Add tiny hand-written `.gp` fixtures for one-cell, obstacle, overlap, density, and writer smoke cases.
- [ ] Ensure `make test` builds and runs the test executable expected by the existing `Makefile`.
- [ ] Add an end-to-end smoke test that invokes `./Legalizer` on a tiny valid fixture and checks TCL command structure.
- [ ] Keep generated test outputs narrow and predictable under `tests/` or a temporary path.

## Done When

- [ ] `make test` exercises parser, model, rows, row solver, density, checker, writer, and CLI paths as they become available.
- [ ] Tiny fixtures can run without OpenROAD.
- [ ] Public benchmark smoke testing is documented or added when `.gp` fixtures are available.
