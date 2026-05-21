# Tetris Fallback

## Goal

Provide a simple deterministic gap insertion path that preserves legality when Abacus candidate insertion cannot place a cell.

## Inputs

- `doc/proposal.md`: Tetris fallback searches nearest feasible intervals and gaps to preserve legality.
- `doc/detailed-design.md`: Tetris Fallback enumerates occupied spans, legal gaps, snapped preferred x locations, and Manhattan displacement scoring.

## Tasks

- [ ] Enumerate occupied spans for each candidate interval from currently placed cells.
- [ ] Derive legal gaps between interval bounds and occupied spans.
- [ ] Snap the cell's preferred x-coordinate to the nearest legal site inside each fitting gap.
- [ ] Score fitting gap placements by Manhattan displacement and deterministic tie-breakers.
- [ ] Commit the winning placement by updating cell coordinates, interval cell order, and occupied width.
- [ ] Add tests for empty intervals, before/between/after placement, site snapping, equal-gap tie-breaking, and no-fit failure.

## Done When

- [ ] Fallback inserts exactly one cell into a legal interval on success.
- [ ] Fallback reports failure when no legal gap can fit the cell.
- [ ] Tetris fallback tests pass.
