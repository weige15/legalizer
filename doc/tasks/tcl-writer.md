# TCL Writer

## Goal

Write the final OpenROAD placement script containing only legal movable-cell `place_cell` commands in micron units.

## Inputs

- `doc/proposal.md`: Output must be `place_cell -inst_name <instName> -orient R0 -origin {X Y}` for each movable cell; do not output macros, blockages, or `detailed_placement`.
- `doc/detailed-design.md`: Preserve deterministic output order, convert DBU to microns using `DBU_Per_Micron`, and fail before partial output if any cell lacks placement.

## Tasks

- [ ] Implement output-file opening with clear diagnostics on failure.
- [ ] Sort or iterate cells in original input order before writing commands.
- [ ] Convert each placed DBU origin to micron coordinates with stable decimal precision.
- [ ] Emit exactly one `place_cell -inst_name <name> -orient R0 -origin {<x> <y>}` line per movable cell.
- [ ] Exclude every `MACRO` and `BLOCKAGE` record from output.
- [ ] Assert or test that no generated line contains `detailed_placement`.
- [ ] Add a golden-file writer test for a tiny placed design.

## Done When

- [ ] Output TCL contains one command for every movable cell and no commands for fixed objects.
- [ ] Coordinates are emitted in microns, not DBU.
- [ ] The generated TCL never invokes `detailed_placement`.
