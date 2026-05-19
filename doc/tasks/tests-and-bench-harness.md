# Tests and Bench Harness

## Goal

Provide local unit, integration, and benchmark validation paths that prove assignment correctness and guide quality tuning.

## Inputs

- `doc/proposal.md`: Validate with `make`, `make test`, assignment-style runs, public benchmarks, OpenROAD `flow.tcl`, and both displacement-heavy and density-heavy parameter settings.
- `doc/detailed-design.md`: Align tests with the source layout and fixtures, and keep OpenROAD benchmark validation separate from unit tests.

## Tasks

- [x] Create or reconcile the `tests/` layout with the project `Makefile`.
- [x] Add synthetic `.gp` fixtures covering parser, geometry, row segments, Abacus, density, writer, and end-to-end legalization.
- [x] Implement `make test` to build and run local tests without requiring OpenROAD.
- [x] Add an assignment-style fixture run for `./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl`.
- [x] Document the OpenROAD benchmark validation command sequence and expected logs.
- [ ] Record public benchmark results for at least one high-alpha and one low-alpha or strict-threshold configuration. OpenROAD is not available in this environment, so `doc/benchmark-validation.md` records the exact command sequence and leaves measured metrics unreported.

## Done When

- [x] `make` and `make test` pass locally.
- [ ] Benchmark validation reports legality, displacement, DOR, quality, and runtime when OpenROAD is available.
