# Placement Data Model

## Goal

Provide shared DBU-based geometry, cell, obstacle, and conversion utilities used consistently by parser, placement, validation, and writing.

## Inputs

- `doc/proposal.md`: Only `CELL` objects are movable; `MACRO` and `BLOCKAGE` objects are fixed obstacles; output coordinates convert DBU to microns.
- `doc/detailed-design.md`: Use half-open rectangles, signed 64-bit DBU coordinates, site snapping relative to `DieArea_LL`, and helpers for overlap, displacement, and conversion.

## Tasks

- [ ] Define `Point`, `Rect`, `Cell`, `Obstacle`, and `Design` structures using 64-bit integer geometry.
- [ ] Convert `ParsedDesign` into movable cells and fixed obstacles while preserving original coordinates and input indices.
- [ ] Implement half-open rectangle overlap and rectangle-at-origin helpers.
- [ ] Implement site snapping helpers relative to die lower-left and site width/height.
- [ ] Implement Manhattan displacement in DBU/microns and DBU-to-micron conversion helpers.
- [ ] Validate die dimensions, site dimensions, and cells that cannot physically fit in the die.
- [ ] Add tests for touching rectangles, overlap, snapping, conversion, and object splitting.

## Done When

- [ ] Downstream modules can consume `Design` without reading parser text fields.
- [ ] Geometry helpers use one consistent half-open rectangle convention.
- [ ] Invalid global design dimensions fail before legalization starts.
