# Row Interval Builder

## Goal

Create legal site-row intervals by subtracting fixed macros and blockages from the die area.

## Inputs

- `doc/proposal.md`: All movable cells must snap to legal row locations and avoid overlaps with fixed macros and blockages.
- `doc/detailed-design.md`: Build one `LegalRow` per site-height row, clip obstacles to the die, subtract any obstacle intersecting a row, and snap interval boundaries inward to site starts.

## Tasks

- [x] Generate site rows from `die.y_min` to `die.y_max` using `site_height`.
- [x] Initialize each row with the die X span snapped to valid site-aligned starts.
- [x] Clip each `MACRO` and `BLOCKAGE` to the die before subtracting its X span from intersecting rows.
- [x] Snap resulting interval boundaries inward and remove intervals too small for one site.
- [x] Merge only truly contiguous site-aligned intervals.
- [x] Add tests for empty rows, interior macro subtraction, edge blockage subtraction, partially outside obstacles, and snapped boundaries.

## Done When

- [x] Every `LegalRow` interval represents fixed-obstacle-free capacity for site-aligned cell starts.
- [x] Rows with no legal space are represented safely without causing crashes.
- [x] Row interval tests pass on small fixture geometry.
