# Legality Validator

## Goal

Validate the final placement before TCL output and produce precise diagnostics for illegal cells, obstacles, rows, intervals, or coordinates.

## Inputs

- `doc/proposal.md`: Validation must check inside-die placement, site and row alignment, no overlaps, obstacle avoidance, orientation preservation, and no `detailed_placement` output.
- `doc/detailed-design.md`: Legality Validator checks placement completeness, supported heights, interval containment, movable overlap, and obstacle overlap.

## Tasks

- [ ] Check every movable cell has one valid placement, is inside the die, is site-aligned, and is row-aligned.
- [ ] Check supported cell height and orientation preservation for every movable cell.
- [ ] Check each movable cell is contained in one legal row interval.
- [ ] Check movable-vs-movable overlap by row buckets sorted by x-coordinate.
- [ ] Check movable-vs-macro and movable-vs-blockage overlap directly or through interval containment plus debug direct checks.
- [ ] Add tests for missing placement, out-of-die, off-site, off-row, movable overlap, obstacle overlap, outside-interval, and valid placement.

## Done When

- [ ] Validator returns success only for fully legal placements.
- [ ] Validator can report multiple diagnostics in one pass where practical.
- [ ] Legality validator tests pass.
