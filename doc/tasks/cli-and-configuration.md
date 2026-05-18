# CLI and Configuration

## Goal

Implement the required `Legalizer` command-line entry point and immutable runtime configuration so later modules can rely on validated arguments.

## Inputs

- `doc/proposal.md`: Required command is `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`; build must produce `Legalizer`.
- `doc/detailed-design.md`: CLI owns argument count, finite numeric parsing for `alpha` and `threshold`, input/output paths, usage diagnostics, and no geometry or file parsing.

## Tasks

- [ ] Create the C++ program entry point and `Config` type with `alpha`, `threshold`, `inputPath`, and `outputPath`.
- [ ] Parse exactly four user arguments after the executable name and print usage on missing or extra arguments.
- [ ] Parse `alpha` and `threshold` with trailing-character rejection and finite-value validation.
- [ ] Wire `main` to pass `Config` into the placement pipeline and return nonzero status on configuration errors.
- [ ] Add `Makefile` support so `make` builds an executable named `Legalizer`.
- [ ] Add a small CLI smoke or unit test path for valid arguments, malformed numerics, and wrong argument counts.

## Done When

- [ ] `make` produces `./Legalizer`.
- [ ] Invalid command-line arguments fail before input parsing with a clear diagnostic.
- [ ] A valid command shape reaches the input parsing stage without losing the input and output paths.
