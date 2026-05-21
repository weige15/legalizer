# Placement Model

## Goal

Provide the shared DBU geometry, placement state, snapping helpers, and overlap predicates used by every legalizer stage.

## Inputs

- `doc/proposal.md`: All internal coordinates are DBU, placements must be site and row aligned, and orientations are preserved.
- `doc/detailed-design.md`: Placement Model defines value types, half-open rectangles, snapping helpers, row alignment, and multi-row detection.

## Tasks

- [ ] Define `Dbu`, `Point`, `Rect`, `Cell`, `Obstacle`, `Tech`, and object type structures in a shared model module.
- [ ] Implement rectangle containment and half-open overlap helpers, including edge-touch non-overlap behavior.
- [ ] Implement cell rectangle construction, DBU-to-micron conversion, site snapping, site alignment, and row alignment helpers.
- [ ] Implement row index or row-y helpers using `die.ly + k * siteHeight` and die upper-bound clipping.
- [ ] Implement supported-cell checks that accept only single-row movable cells with site-compatible dimensions.
- [ ] Add unit tests for geometry predicates, snapping with nonzero die origin, row bounds, micron conversion, and unsupported cell dimensions.

## Done When

- [ ] All modules can depend on the model without duplicating geometry logic.
- [ ] Geometry and snapping tests pass.
- [ ] Unsupported movable cell diagnostics can identify the offending cell.
