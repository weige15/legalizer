# GP Parser

## Goal

Read the assignment `.gp` format into the placement model without modifying, clipping, or legalizing coordinates.

## Inputs

- `doc/proposal.md`: Parse `DBU_Per_Micron`, die bounds, site dimensions, and instance records with `Name LLX LLY Width Height Type`.
- `doc/detailed-design.md`: Tolerate optional blank lines, require the instance header, preserve instance names, and report line-numbered diagnostics.

## Tasks

- [x] Parse all required metadata fields as integers before the instance table.
- [x] Require the instance header tokens before reading instance records.
- [x] Parse `CELL`, `MACRO`, and `BLOCKAGE` records into movable cells or fixed obstacles.
- [x] Preserve instance names exactly as input tokens.
- [x] Fail with line numbers for unknown types, malformed rows, non-integer geometry, missing metadata, or invalid dimensions.
- [x] Add parser fixtures for valid input, missing blank line, unknown type, missing metadata, and mixed cell/macro/blockage classification.

## Done When

- [x] Valid `.gp` fixtures produce a complete `Design`.
- [x] Invalid `.gp` fixtures fail deterministically with actionable diagnostics.
