# Multi-Row Placement Layer

## Goal

Place cells taller than one site row using consecutive-row geometric feasibility and local repacking.

## Inputs

- `doc/proposal.md`: Detect `height > Site_Height`, search row spans, and use a geometric fallback because rail phase is unavailable.
- `doc/detailed-design.md`: Intersect free intervals across covered rows, try local insertion first, then expand to full row-span search.

## Tasks

- [x] Compute `row_span = ceil(cell.height / Site_Height)` and bypass this module for one-row cells.
- [x] Enumerate consecutive row spans near original Y before farther spans.
- [x] Intersect free X intervals across covered rows and reject spans too narrow for the cell.
- [x] Evaluate candidate X positions near original X with deterministic tie breaks.
- [x] Repack affected single-row neighbors through row-engine trials where local overlap must be realized.
- [x] Add tests for bypassing one-row cells, placing a two-row cell, rejecting narrow common intervals, rejecting partially blocked spans, and fallback expansion.

## Done When

- [x] Tall movable cells either receive legal lower-left coordinates or report infeasible placement.
- [x] Multi-row tests validate common-interval and fallback behavior.
