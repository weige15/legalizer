# GP Parser

## Goal

Parse the assignment `.gp` input into typed design data while preserving movable cell order and fixed obstacle geometry.

## Inputs

- `doc/proposal.md`: The parser reads DBU scale, die area, site dimensions, and instance rows classified as `CELL`, `MACRO`, or `BLOCKAGE`.
- `doc/detailed-design.md`: Metadata records are required, coordinates use signed 64-bit DBU integers, blank lines before the header are accepted, and obstacle coordinates may extend outside the die.

## Tasks

- [x] Implement strict parsing for `DBU_Per_Micron`, `DieArea_LL`, `DieArea_UR`, `Site_Width`, and `Site_Height`.
- [x] Accept the `Name LLX LLY Width Height Type` instance header after the required blank-line area.
- [x] Parse instance rows into movable cells for `CELL` and fixed obstacles for `MACRO` and `BLOCKAGE`.
- [x] Reject malformed integers, missing metadata, non-positive dimensions, and unknown instance types with line context.
- [x] Preserve input order for movable cells so TCL output is deterministic.
- [x] Add parser fixtures for valid cells, macros, blockages, blank lines, malformed metadata, bad dimensions, and unknown types.

## Done When

- [x] A public or tiny extracted `.gp` file loads into a complete `Design`.
- [x] Parser tests cover success and failure cases without requiring OpenROAD.
- [x] Obstacles partially outside the die parse successfully for later clipping.
