# Abacus Interval Solver

## Goal

Pack an ordered list of cells inside one legal interval with non-overlapping, site-aligned x-positions and low row-local displacement.

## Inputs

- `doc/proposal.md`: Abacus clusters merge overlapping tentative placements, clamp to interval bounds, and expand into legal x-coordinates.
- `doc/detailed-design.md`: Abacus Interval Solver accepts interval bounds and ordered cell IDs, uses uniform weights, and reports proposed positions plus cost.

## Tasks

- [ ] Implement cluster construction, overlap detection, merge, target recomputation, and interval clamping for one row interval.
- [ ] Expand clusters into left-to-right cell x positions while preserving the supplied order.
- [ ] Snap final positions to site boundaries and repair site-rounding overflow or overlap inside interval bounds.
- [ ] Return row-local displacement cost and proposed x-coordinate map without mutating global placement state.
- [ ] Return failure when total cell width exceeds the interval or snapping cannot keep all cells in bounds.
- [ ] Add tests for empty input, one-cell clamping, two-cell merge, chain merge, exact-fit width, over-capacity failure, and nonzero interval origin.

## Done When

- [ ] Solver outputs never overlap and never exceed the interval on success.
- [ ] Caller can use solver results as a pure candidate trial.
- [ ] Abacus solver tests pass.
