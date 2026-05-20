# TCL Writer

## Goal

Serialize a validated placement to OpenROAD `place_cell` commands without emitting prohibited commands.

## Inputs

- `doc/proposal.md`: Output must contain explicit placement commands only and never call `detailed_placement`.
- `doc/detailed-design.md`: TCL Writer deterministic ordering, DBU-to-micron conversion, temporary sibling write, scan, and rename behavior.

## Tasks

- [x] Emit one `place_cell -inst_name <name> -orient R0 -origin {<x> <y>}` line per movable cell in input order.
- [x] Convert DBU coordinates to microns with enough decimal precision for site alignment.
- [x] Write to a temporary sibling file and rename only after stream flush and content checks succeed.
- [x] Scan generated content to reject any accidental `detailed_placement` text.
- [x] Fail cleanly if any movable cell lacks a placement or the output path cannot be written.
- [x] Add tests for one-cell output shape, fractional micron conversion, deterministic ordering, and prohibited-command absence.

## Done When

- [x] Generated TCL contains only deterministic `place_cell` commands.
- [x] Existing output is left untouched on writer failure before final rename.
