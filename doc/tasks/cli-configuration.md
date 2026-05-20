# CLI / Configuration

## Goal

Implement the command-line entry point and pipeline orchestration for `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`.

## Inputs

- `doc/proposal.md`: Required assignment interface, exit behavior, and validation-before-write requirement.
- `doc/detailed-design.md`: CLI / Configuration module contract, run configuration fields, and integration test expectations.

## Tasks

- [x] Define `RunConfig` with `alpha`, `threshold`, input path, and output path.
- [x] Validate exactly four user arguments and parse finite numeric values in the required ranges.
- [x] Implement a narrow `run(config)` pipeline that calls parser, row interval builder, legalizer, validator, and writer.
- [x] Route module failures to concise `stderr` diagnostics and nonzero exits without touching output on early failure.
- [x] Add smoke coverage for missing args, malformed numbers, and a one-cell successful run.

## Done When

- [x] `./Legalizer` rejects invalid invocation forms with nonzero status.
- [x] A valid small fixture runs through the same CLI path and writes only placement TCL.
