# Density Estimator

## Goal

Provide an advisory 10 micron grid density penalty that helps the legalizer reduce DOR without controlling legality.

## Inputs

- `doc/proposal.md`: The assignment quality metric combines average displacement with DOR, where DOR is the percentage of non-macro 10 micron grids above the threshold.
- `doc/detailed-design.md`: Grid size is `10 * DBU_Per_Micron`, fixed macro area is preloaded for exclusion, movable occupancy is updated on commit, and candidate overflow is normalized to a percentage-like penalty.

## Tasks

- [x] Create grid indexing over the die using `10 * dbu_per_micron` DBU cells.
- [x] Track per-grid macro-covered area, movable occupied area, and optional blockage occupancy for scoring only.
- [x] Exclude fully macro-covered grids from overflow scoring where practical.
- [x] Implement candidate scoring by intersecting the candidate rectangle with affected grids and summing threshold overflow.
- [x] Update movable occupancy after each committed placement.
- [x] Add tests for one-grid occupancy, multi-grid area splitting, macro exclusion, and increasing penalty after threshold overflow.

## Done When

- [x] Density scoring returns low cost for uncongested candidates and higher cost for threshold overflow.
- [x] Committed placements update future candidate scores.
- [x] The estimator can switch to sparse storage or otherwise avoid excessive memory for large designs.
