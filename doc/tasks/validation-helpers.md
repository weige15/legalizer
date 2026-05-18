# Validation Helpers

## Goal

Provide internal legality checks before writing output and after refinement so implementation bugs are caught before OpenROAD validation.

## Inputs

- `doc/proposal.md`: OpenROAD `check_placement -verbose` is the final legality oracle, but the implementation should pre-check bounds, alignment, and overlap.
- `doc/detailed-design.md`: Validate placed cells against die bounds, site alignment, fixed obstacles, movable overlaps, missing placements, and output command count.

## Tasks

- [ ] Check every cell has a final placement before output.
- [ ] Check die containment for lower-left and upper-right corners.
- [ ] Check X and Y site alignment relative to `DieArea_LL`.
- [ ] Check fixed obstacle overlap using row-index or interval filtering.
- [ ] Check movable-cell overlap by bucketing touched rows and sorting X intervals.
- [ ] Return diagnostics with cell names and coordinates for every failure category.
- [ ] Add tests for valid placement, out-of-bounds, X/Y misalignment, obstacle overlap, cell overlap, and missing placement.

## Done When

- [ ] Validation passes known-good synthetic placements.
- [ ] Validation fails each targeted bad fixture with a useful diagnostic.
- [ ] TCL writing is skipped when validation fails in normal execution.
