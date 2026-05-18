# Placement Model

## Goal

Provide shared geometry, instance, and design structures used consistently by parsing, row construction, legalization, density scoring, and writing.

## Inputs

- `doc/proposal.md`: The legalizer keeps internal geometry in DBU, treats `CELL` as movable, and treats `MACRO` and `BLOCKAGE` as fixed obstacles.
- `doc/detailed-design.md`: Rectangles use half-open intervals, store original and placed cell rectangles, and expose geometry helpers for snapping, clipping, overlap, and containment.

## Tasks

- [x] Define `Rect`, `InstanceType`, `Cell`, `Obstacle`, and `Design` structures with 64-bit DBU coordinates.
- [x] Implement helpers for rectangle width, height, intersection, containment, clipping, and site snapping.
- [x] Track original coordinates separately from final placement coordinates for displacement and debugging.
- [x] Validate die dimensions, site dimensions, duplicate movable names, and unsupported cell heights.
- [x] Add geometry tests for edge-touching rectangles, true overlaps, clipping, containment, and site snapping.
- [x] Add model validation tests for invalid dimensions and duplicate names.

## Done When

- [x] All modules can share the same typed model without ad hoc coordinate conventions.
- [x] Geometry helper tests pass for half-open rectangle behavior.
- [x] Invalid model data is rejected before TCL emission.
