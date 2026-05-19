# Density Estimator

## Goal

Estimate 10 micron by 10 micron movable density, overflow, and candidate density deltas for scoring and smoothing.

## Inputs

- `doc/proposal.md`: Maintain a grid with `grid_size_dbu = 10 * DBU_Per_Micron`, penalize overflow, and use it for relative candidate scoring.
- `doc/detailed-design.md`: Exclude macro-covered grids where practical, handle partial grids, and avoid division by zero.

## Tasks

- [x] Build density grid bounds from the die with at least one intersecting grid for small designs.
- [x] Compute rectangle-grid intersection area using integer DBU geometry.
- [x] Track movable area increments and decrements for committed placements and candidate moves.
- [x] Implement estimated DOR and overflow classification against the CLI threshold policy.
- [x] Exclude fully macro-covered grids from DOR counting according to the final `flow.tcl`-matched policy.
- [x] Add tests for grid sizing, cell area updates, move deltas, macro exclusion, and threshold edge cases.

## Done When

- [x] Candidate scoring can query touched-grid overflow deltas without full recomputation.
- [x] Density tests document threshold and fixed-obstacle exclusion behavior.
