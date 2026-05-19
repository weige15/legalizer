# CLI and Configuration

## Goal

Build the assignment-facing executable entry point that validates arguments, runs the legalizer pipeline, and prevents partial output on failure.

## Inputs

- `doc/proposal.md`: Required command line is `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`; output must be deterministic and contain no `detailed_placement`.
- `doc/detailed-design.md`: CLI owns `Config`, top-level pipeline ordering, finite numeric parsing, short `stderr` diagnostics, and non-zero exits.

## Tasks

- [x] Implement `Config` with `alpha`, `threshold`, `input_path`, and `output_path`.
- [x] Parse exactly four user arguments after the program name and reject missing or extra arguments.
- [x] Parse `alpha` and `threshold` as finite `double` values and emit concise errors for invalid values.
- [x] Wire `main.cpp` to call parser, row segment builder, legalizer, legality checker, and TCL writer in order.
- [x] Ensure parse, legalize, check, or write failures return non-zero and do not leave a partial output TCL.
- [x] Add CLI tests for valid arguments, missing arguments, non-numeric scalars, and failed output creation.

## Done When

- [x] `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl` runs the full pipeline.
- [x] Invalid CLI input exits non-zero with no generated placement output.
