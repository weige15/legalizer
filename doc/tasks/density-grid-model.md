# Density Grid Model

## Goal

Estimate 10um x 10um density overflow pressure so placement and refinement can reduce DOR without overriding legality.

## Inputs

- `doc/proposal.md`: Quality combines average displacement and DOR; DOR counts non-macro 10um x 10um grids whose utilization exceeds the threshold.
- `doc/detailed-design.md`: Grid bin size is `10 * DBU_Per_Micron`; fixed macros are excluded from the DOR denominator; blockages remain placement obstacles only.

## Tasks

- [ ] Build density bins over the die with at least one bin in each dimension.
- [ ] Mark bins overlapped by fixed macros as excluded from DOR counting.
- [ ] Maintain approximate occupied cell area per non-excluded bin for committed placements.
- [ ] Implement `densityCost(cell, x, y)` that penalizes placements pushing bins above `threshold`.
- [ ] Implement add/remove placement updates for legalizer commits and refinement trials.
- [ ] Implement optional `estimateDOR()` for diagnostics and tuning.
- [ ] Add tests for bin indexing, macro exclusion, add/remove symmetry, and threshold overflow comparisons.

## Done When

- [ ] Legalization can query density pressure for candidate placements.
- [ ] Density updates stay consistent when cells are moved during refinement.
- [ ] All-macro-excluded or tiny-die cases return stable zero-overflow behavior instead of dividing by zero.
