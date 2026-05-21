# Density / Metric Evaluator

## Goal

Compute average displacement, DOR, weighted quality, and affected-grid information for final reporting and local repair.

## Inputs

- `doc/proposal.md`: Quality is `alpha * AverageDisplacement + (1 - alpha) * DOR`, with DOR measured on 10 um grids above the threshold.
- `doc/detailed-design.md`: Density / Metric Evaluator defines Manhattan displacement, macro-covered grid exclusion, grid-area density, threshold behavior, and incremental repair support.

## Tasks

- [ ] Compute average Manhattan displacement in microns from original to placed lower-left cell origins.
- [ ] Build 10 um by 10 um DBU grid rectangles over the die and exclude grids overlapping fixed `MACRO` rectangles from the DOR denominator.
- [ ] Accumulate movable cell overlap area into included grids and count grids whose density percentage is greater than `threshold`.
- [ ] Compute weighted quality for `alpha = 0`, `alpha = 1`, and mixed alpha values.
- [ ] Expose affected-grid queries or update helpers for cells moved by local repair.
- [ ] Add tests for single-grid cells, multi-grid cells, macro exclusion, equality-at-threshold non-overflow, empty placement, and quality formulas.

## Done When

- [ ] Metrics are deterministic and handle zero denominator grids without division by zero.
- [ ] DOR treats blockages as placement obstacles only unless later integration evidence changes the design.
- [ ] Metric evaluator tests pass.
