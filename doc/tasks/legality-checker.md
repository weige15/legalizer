# Legality Checker

## Goal

Validate final placements before TCL output and report placement violations without repairing them.

## Inputs

- `doc/proposal.md`: Required correctness checks include inside die, legal site alignment, no movable overlap, no fixed overlap, and one placement per movable cell.
- `doc/detailed-design.md`: Check row Y membership, site X alignment, row-segment membership, fixed obstacles, and movable overlap efficiently.

## Tasks

- [x] Verify every movable cell has an assigned legal coordinate.
- [x] Check each movable rectangle is inside the die.
- [x] Check lower-left X is site-aligned and lower-left Y is a legal row Y.
- [x] Check movable cells do not overlap fixed `MACRO` or `BLOCKAGE` rectangles.
- [x] Check movable-movable overlaps using row grouping or sweep-line sorting instead of naive global quadratic checks.
- [x] Add tests for legal placement, off-row Y, off-site X, movable overlap, fixed overlap, and out-of-die placement.

## Done When

- [x] The checker passes all legal fixture placements and fails each targeted violation fixture.
- [x] The top-level pipeline stops before writing TCL when the checker fails.
