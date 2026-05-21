# CLI / Main

## Goal

Build the `Legalizer` executable entry point that validates arguments, runs the legalization pipeline in order, and fails before writing output when any stage reports an error.

## Inputs

- `doc/proposal.md`: Required interface is `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>` with legality before quality.
- `doc/detailed-design.md`: CLI / Main orchestrates parsing, model validation, row building, legalization, metric repair, final validation, and TCL writing.

## Tasks

- [ ] Parse and validate four positional CLI arguments, including finite `alpha` in `[0, 1]` and finite `threshold`.
- [ ] Wire the pipeline stages in order through the public APIs of parser, model, row builder, legalizer, metric repair, validator, and writer.
- [ ] Reject unsupported global invariants before legalization, including invalid DBU/site dimensions and multi-row movable cells.
- [ ] Report concise diagnostics and nonzero exit codes for malformed input, legalization failure, validation failure, and output failure.
- [ ] Add CLI smoke and negative tests for success, bad argument count, invalid numbers, missing input, and unsupported multi-row cells.

## Done When

- [ ] `make Legalizer` builds the executable.
- [ ] CLI tests cover successful output creation and expected failure paths.
- [ ] No output file is written or replaced after a failed pre-output stage.
