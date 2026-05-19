# Placement Model

## Goal

Provide canonical geometry, cell, obstacle, and coordinate helper types shared by every legalizer module.

## Inputs

- `doc/proposal.md`: Coordinates stay in integer DBU internally and original placements must be preserved for displacement reporting.
- `doc/detailed-design.md`: Rectangles use half-open bounds and expose helpers for containment, overlap, row/site conversion, and DBU-to-micron conversion.

## Tasks

- [ ] Implement `src/placement_model.{h,cpp}` with `Rect`, `Cell`, `Obstacle`, and `PlacementModel` data types.
- [ ] Derive `urx` and `ury` from lower-left coordinates plus dimensions at parse time.
- [ ] Add geometry helpers for area, overlap, containment, row alignment, site alignment, and coordinate conversion.
- [ ] Track original and placed rectangles separately for each movable cell.
- [ ] Add model tests for overlap semantics, containment, conversion, site-grid alignment, and multi-row-height detection.

## Done When

- [ ] All downstream modules can use the same DBU geometry types without duplicating rectangle logic.
- [ ] Original placement data remains available after legalization.
- [ ] Geometry helper tests pass under `make test`.
