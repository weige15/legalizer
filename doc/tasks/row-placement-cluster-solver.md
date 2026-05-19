# Row Placement / Cluster Solver

## Goal

Implement reversible ABACUS-style row placement for one interval and fixed cell order.

## Inputs

- `doc/proposal.md`: `PlaceRow` inserts a cell into a row interval, merges overlapping clusters, clamps clusters to interval bounds, and expands cluster positions.
- `doc/detailed-design.md`: Trials must report feasibility, changed placements, and displacement delta without mutating committed state.

## Tasks

- [ ] Implement cluster placement logic for an ordered interval cell sequence.
- [ ] Support trial insertion of a candidate cell by target x-coordinate.
- [ ] Mark trials infeasible when any cell or total cell width exceeds interval capacity.
- [ ] Return per-cell x positions, changed cell ids, and movement delta for feasible trials.
- [ ] Add commit logic that applies a selected trial to interval state and placed cell rectangles.
- [ ] Add tests for single cell, non-overlap preservation, overlap spreading, left/right clamping, infeasible capacity, and trial non-mutation.

## Done When

- [ ] Trial placement never changes committed rows.
- [ ] Committed placement creates non-overlapping cells inside the selected interval.
- [ ] Cluster solver tests pass under `make test`.
