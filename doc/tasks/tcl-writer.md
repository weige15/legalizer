# TCL Writer

## Goal

Emit the final OpenROAD placement script with one deterministic `place_cell` command per movable cell.

## Inputs

- `doc/proposal.md`: Output direct `place_cell` commands only, use `-orient R0`, convert origins to microns, and never call `detailed_placement`.
- `doc/detailed-design.md`: Preserve movable input order and fail if any legal coordinate is missing or the output file cannot be opened.

## Tasks

- [x] Iterate movable cells in original input order.
- [x] Convert DBU legal lower-left coordinates to micron coordinates using `DBU_Per_Micron`.
- [x] Format each line as `place_cell -inst_name <instName> -orient R0 -origin {X Y}`.
- [x] Use enough decimal precision for fractional micron origins without noisy formatting.
- [x] Reject output when any cell lacks legal coordinates.
- [x] Add tests for one line per movable cell, `R0` orientation, no `detailed_placement`, DBU-to-micron conversion, and input-order preservation.

## Done When

- [x] Generated TCL contains only legal `place_cell` commands for movable cells.
- [x] Writer tests prove deterministic order and coordinate conversion.
