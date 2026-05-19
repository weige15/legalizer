# Legality and Metric Checker

## Goal

Validate final placements and compute development metrics without making placement decisions.

## Inputs

- `doc/proposal.md`: Final checks include die containment, site alignment, row alignment, cell-cell non-overlap, obstacle avoidance, average displacement, and DOR.
- `doc/detailed-design.md`: Checker returns structured diagnostics and delegates DOR computation to the density estimator.

## Tasks

- [ ] Implement a checker module for placed-cell completeness, die containment, x/y site alignment, and row interval membership.
- [ ] Detect all cell-cell overlaps.
- [ ] Detect overlaps with both `MACRO` and `BLOCKAGE` obstacles.
- [ ] Compute average Manhattan displacement in microns from original lower-left to placed lower-left.
- [ ] Call the density estimator for final DOR and report metrics separately from legality failures.
- [ ] Add tests for out-of-die, x/y misalignment, cell overlap, obstacle overlap, known legal placement, and average displacement.

## Done When

- [ ] Illegal placements produce actionable diagnostics.
- [ ] Valid placements produce average displacement and DOR metrics.
- [ ] Checker tests pass under `make test`.
