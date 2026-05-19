# TCL Writer

## Goal

Emit assignment-compliant OpenROAD placement TCL with one command per movable cell.

## Inputs

- `doc/proposal.md`: Output must contain `place_cell -inst_name ... -orient R0 -origin {x y}` and must not call `detailed_placement`.
- `doc/detailed-design.md`: Writer preserves deterministic movable-cell order and converts DBU positions to microns.

## Tasks

- [ ] Implement `src/tcl_writer.{h,cpp}` to write only movable `CELL` instances.
- [ ] Preserve original input order for output commands.
- [ ] Convert DBU coordinates to micron text with enough precision for DBU-derived positions.
- [ ] Refuse to write cells without final placement.
- [ ] Add tests for one placed cell, fractional micron formatting, macro/blockage exclusion, and absence of `detailed_placement`.

## Done When

- [ ] The output file contains only valid `place_cell` commands for movable cells.
- [ ] All emitted commands use `-orient R0`.
- [ ] Writer tests pass under `make test`.
