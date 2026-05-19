# GP Parser

## Goal

Parse assignment `.gp` files into typed DBU-based placement records without making legalization decisions.

## Inputs

- `doc/proposal.md`: Input contains technology metadata, die bounds, site dimensions, and `CELL`, `MACRO`, or `BLOCKAGE` instance records.
- `doc/detailed-design.md`: Parser owns lexical and structural validation and outputs a `PlacementModel`.

## Tasks

- [ ] Implement `src/gp_parser.{h,cpp}` to read required metadata keys and the instance table header.
- [ ] Parse instance rows into integer DBU rectangles and classify movable cells versus fixed obstacles.
- [ ] Preserve input order and reject duplicate instance names.
- [ ] Report unreadable files, malformed integers, unknown types, nonpositive dimensions, missing metadata, and invalid site dimensions.
- [ ] Add parser tests for one-cell, macro/blockage, malformed header, unknown type, negative dimension, and duplicate-name fixtures.

## Done When

- [ ] Valid `.gp` fixtures produce a populated `PlacementModel` with integer DBU geometry.
- [ ] Invalid `.gp` fixtures fail with useful diagnostics.
- [ ] Parser tests pass under `make test`.
