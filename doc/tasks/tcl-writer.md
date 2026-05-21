# TCL Writer

## Goal

Write a deterministic OpenROAD placement TCL file containing only validated movable-cell `place_cell` commands.

## Inputs

- `doc/proposal.md`: Output must contain explicit `place_cell` commands, preserve orientation, convert origins to microns, and omit `detailed_placement`.
- `doc/detailed-design.md`: TCL Writer runs after validation, uses fixed decimal precision, preserves parser order, and writes through a same-directory temporary file.

## Tasks

- [ ] Format one `place_cell -inst_name <name> -orient <orient> -origin {<x> <y>}` command per movable cell.
- [ ] Convert DBU coordinates to microns with sufficient fixed decimal precision for OpenROAD.
- [ ] Preserve parser order and skip macros and blockages.
- [ ] Ensure no emitted text contains `detailed_placement`.
- [ ] Write to a same-directory temporary file and rename only after successful flush and close.
- [ ] Add tests for one-cell formatting, orientation preservation, coordinate conversion, deterministic order, forbidden command absence, and write failure.

## Done When

- [ ] Writer emits valid TCL only after the caller has validated placement.
- [ ] Output is deterministic for the same parsed input and placement.
- [ ] TCL writer tests pass.
