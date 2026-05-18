# CLI Driver

## Goal

Build the root executable entry point that implements the TA command line and coordinates parse, legalize, and emit.

## Inputs

- `doc/proposal.md`: The final submission must build `Legalizer` with `make` and run as `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`.
- `doc/detailed-design.md`: CLI Driver validates arguments, parses numeric parameters, invokes parser, row builder, density estimator, legalizer, and TCL writer, then reports concise failures.

## Tasks

- [x] Create `main` with exactly four user arguments: `alpha`, `threshold`, input `.gp`, and output `.tcl`.
- [x] Parse `alpha` and `threshold` as full-string doubles and reject invalid values with a non-zero exit.
- [x] Wire the driver to `GpParser::parse`, `RowIntervalBuilder::build`, `DensityEstimator`, `Legalizer::legalize`, and `TclWriter::write`.
- [x] Convert module failures or exceptions into short `stderr` diagnostics and non-zero exit codes.
- [x] Add a `Makefile` target that builds a root-level `Legalizer` executable with C++17.
- [x] Add a smoke check for missing args, invalid numeric args, unreadable input, and one valid tiny fixture.

## Done When

- [x] `make` creates `./Legalizer`.
- [x] `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl` uses the exact filenames passed by the caller.
- [x] Invalid arguments and module failures exit non-zero with useful diagnostics.
