# Legalizer

## Goal

Assign every movable cell a legal, deterministic placement that balances displacement and density while guaranteeing no overlap.

## Inputs

- `doc/proposal.md`: Use row-based candidate search from each global-placement coordinate, score displacement plus density penalty, and guarantee die containment, site alignment, and obstacle avoidance.
- `doc/detailed-design.md`: Place harder cells first, search rows by vertical distance, evaluate site-aligned X starts near original X, maintain row occupancy, and fall back to exhaustive row-interval search.

## Tasks

- [x] Implement deterministic cell ordering by placement difficulty, area, original coordinates, and input order.
- [x] Generate candidate rows in increasing distance from each cell's original Y coordinate.
- [x] Generate site-aligned X candidates within row intervals around each cell's original X coordinate.
- [x] Reject candidates outside the die, outside fixed-obstacle-free intervals, or overlapping committed row occupancy.
- [x] Score feasible candidates with `alpha`-weighted displacement and density-estimator penalty.
- [x] Commit chosen placements by updating cell rectangles, row occupancy, and density state.
- [x] Add fallback exhaustive search and explicit failure diagnostics for unplaceable cells.

## Done When

- [x] Every movable cell has `has_placement = true` or legalization fails with the cell name.
- [x] Placements are inside the die, site-aligned, fixed-obstacle-free, and mutually non-overlapping.
- [x] Legalizer tests cover simple placement, adjacent cells, macro avoidance, blockage displacement, deterministic output, and overfull failure.
