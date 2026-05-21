# GP Parser

## Goal

Parse OpenROAD-extracted `.gp` files into typed technology, movable cell, macro, and blockage records while preserving deterministic object order.

## Inputs

- `doc/proposal.md`: The program reads `.gp` placement data in DBU and keeps original movable coordinates for displacement.
- `doc/detailed-design.md`: GP Parser defines required headers, object record shapes, line-numbered diagnostics, and validation rules.

## Tasks

- [ ] Implement parser support for required technology headers and validate each appears exactly once.
- [ ] Parse `CELL` and `MACRO` records with orientation plus type, and parse six-field `BLOCKAGE` records.
- [ ] Convert object lower-left plus dimensions into model records using positive DBU dimensions and half-open obstacle rectangles.
- [ ] Preserve input object order for deterministic placement and output.
- [ ] Return line-numbered diagnostics for malformed fields, missing headers, unknown object types, and nonpositive dimensions.
- [ ] Add parser unit fixtures for valid cells, macros, blockages, missing headers, bad object types, bad dimensions, and missing orientation.

## Done When

- [ ] Valid `.gp` fixtures produce the expected `Tech`, `Cell`, and `Obstacle` records.
- [ ] Invalid parser fixtures fail with specific diagnostics.
- [ ] Parser tests run through `make test` or the project test binary.
