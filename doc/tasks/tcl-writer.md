# TCL Writer

## Goal

Emit an OpenROAD-compatible TCL script with one direct `place_cell` command per movable cell.

## Inputs

- `doc/proposal.md`: The output must convert DBU origins to microns and must not call `detailed_placement`.
- `doc/detailed-design.md`: Iterate movable cells in input order, require final placements, emit `-orient R0`, and format micron values without unnecessary numeric noise.

## Tasks

- [x] Open the requested output path and report file errors.
- [x] Verify every movable cell has a final placement before writing.
- [x] Convert placed lower-left DBU coordinates to microns using `DBU_Per_Micron`.
- [x] Emit `place_cell -inst_name <instName> -orient R0 -origin {X Y}` for each movable cell.
- [x] Validate or safely format instance names for TCL compatibility.
- [x] Add writer tests for command shape, DBU-to-micron conversion, unplaced-cell rejection, and absence of `detailed_placement`.

## Done When

- [x] Output contains exactly one `place_cell` command per movable `CELL`.
- [x] All commands use `-orient R0` and micron origins.
- [x] Output TCL never contains `detailed_placement`.
