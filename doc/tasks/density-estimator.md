# Density Estimator

## Goal

Compute density-overflow pressure for trial scoring and exact final DOR for validation.

## Inputs

- `doc/proposal.md`: DOR is based on 10um by 10um grids whose non-macro movable density exceeds `threshold`.
- `doc/detailed-design.md`: Grid size is `10 * dbu_per_micron`, macro-covered grids are excluded, and final DOR uses exact cell-grid overlap area.

## Tasks

- [ ] Implement `src/density_estimator.{h,cpp}` with grid construction from die bounds and DBU-per-micron.
- [ ] Mark macro-covered grids as excluded from the DOR denominator.
- [ ] Accumulate exact movable-cell overlap area into all touched grids for final DOR.
- [ ] Provide a lightweight trial penalty API for candidate row scoring.
- [ ] Treat `threshold` as a percentage and compare density using `> threshold`.
- [ ] Add tests for empty placement, known overflow grid, macro exclusion, boundary-spanning cells, and threshold comparison.

## Done When

- [ ] Exact final DOR is computed from placed cell rectangles.
- [ ] Trial scoring can query density pressure without owning placement decisions.
- [ ] Density tests pass under `make test`.
