# Placement Model

## Goal

Create the DBU-based in-memory model for geometry, movable cells, fixed obstacles, and final legal coordinates.

## Inputs

- `doc/proposal.md`: Store `.gp` metadata and classify `CELL` as movable while treating `MACRO` and `BLOCKAGE` as fixed obstacles.
- `doc/detailed-design.md`: Use signed 64-bit DBU geometry, half-open rectangles, input-order preservation, and validation helpers.

## Tasks

- [x] Define `Rect`, `Instance`, movable cell, obstacle, and `Design` data structures using integer DBU coordinates.
- [x] Implement half-open rectangle helpers for overlap, width, height, area, center, displacement, and rectangle construction.
- [x] Preserve movable input order separately from fixed obstacle storage.
- [x] Store legal lower-left coordinates for movable cells with an explicit unset state before legalization.
- [x] Reject invalid metadata, non-positive die dimensions, and non-positive instance dimensions.
- [x] Add geometry tests for touching-edge overlap, area arithmetic, input order, and obstacle classification.

## Done When

- [x] Later modules can consume a validated `Design` without reparsing placement records.
- [x] Geometry tests prove half-open overlap and integer DBU area behavior.
