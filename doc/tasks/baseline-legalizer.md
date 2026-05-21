# Baseline Legalizer

## Goal

Produce the first complete legal placement for every supported movable cell using deterministic interval assignment, Abacus-style trials, and Tetris fallback.

## Inputs

- `doc/proposal.md`: Baseline legalization searches nearby intervals, scores displacement, commits legal insertion, and falls back to Tetris if needed.
- `doc/detailed-design.md`: Baseline Legalizer owns cell ordering, candidate row search, commit semantics, and failure behavior.

## Tasks

- [ ] Implement deterministic cell processing order based on original coordinates and name.
- [ ] Search candidate intervals from the nearest original row outward, using remaining capacity and row-radius limits.
- [ ] Trial candidate insertions on copied interval state and call the Abacus interval solver for row-local packing.
- [ ] Score candidate trials by displacement delta with deterministic tie-breakers and commit only the selected candidate.
- [ ] Invoke Tetris fallback over legal intervals when no Abacus trial succeeds.
- [ ] Add tests for nearest-row placement, overlapping originals, macro-forced splits, farther-row selection, fallback placement, and insufficient capacity failure.

## Done When

- [ ] Every supported cell is placed exactly once on successful legalization.
- [ ] Failed legalization names the unplaceable cell and leaves no partially committed candidate trial.
- [ ] Baseline legalizer tests pass.
