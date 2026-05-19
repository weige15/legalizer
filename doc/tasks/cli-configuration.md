# CLI / Configuration

## Goal

Implement the `Legalizer` command-line entry point and top-level pipeline orchestration for the required assignment interface.

## Inputs

- `doc/proposal.md`: Required invocation is `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>`.
- `doc/detailed-design.md`: CLI validates arguments, builds config, runs parse, row build, legalize, check, and write steps.

## Tasks

- [ ] Create `src/main.cpp` and a small config parsing function for `alpha`, `threshold`, input path, and output path.
- [ ] Reject malformed argument counts, non-finite numeric values, and `alpha` values outside `[0, 1]`.
- [ ] Wire the top-level pipeline through parser, row interval builder, legalizer, checker, and TCL writer.
- [ ] Print concise `stderr` diagnostics and return nonzero for parse, legalization, validation, or write failures.
- [ ] Add smoke coverage for bad arguments and a tiny valid fixture through the test runner.

## Done When

- [ ] `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>` runs the full pipeline.
- [ ] CLI failures produce nonzero exit status without writing misleading placement output.
- [ ] `make test` covers argument parsing and at least one executable smoke case.
