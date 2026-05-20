# Density Estimator

## Goal

Maintain 10um density-grid information for candidate scoring and compute exact DOR for validation.

## Inputs

- `doc/proposal.md`: `alpha` and `threshold` driven density-aware scoring and exact DOR validation.
- `doc/detailed-design.md`: Density Estimator grid construction, macro-bin exclusion, strict overflow comparison, and candidate penalty behavior.

## Tasks

- [x] Build ceil-divided 10um by 10um bins in DBU, clipping edge bins to die bounds.
- [x] Exclude bins that overlap any `MACRO` while keeping blockage-covered bins countable.
- [x] Implement exact DOR recomputation from movable cell placements using overlap area per bin.
- [x] Implement local candidate overflow-pressure estimation for trial placements.
- [x] Handle invalid DBU and no-countable-bin cases with clear diagnostics or warning metrics.
- [x] Add tests for grid sizing, macro exclusion, strict `density > threshold`, exact DOR, and candidate penalty monotonicity.

## Done When

- [x] Exact DOR matches the documented flow-compatible rules on small fixtures.
- [x] Candidate penalty increases when a trial adds area to already-over-threshold bins.
