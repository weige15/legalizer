# Row Interval Builder

## Goal

Build legal placement row intervals by subtracting fixed macros and blockages from every site row.

## Inputs

- `doc/proposal.md`: Row interval construction, obstacle avoidance, site snapping, and capacity precheck intent.
- `doc/detailed-design.md`: Row Interval Builder module behavior, macro/blockage subtraction, sorting, and snapping rules.

## Tasks

- [x] Generate site rows from die lower Y through the last full site-height row.
- [x] Start each row with the full die-width interval and subtract obstacle horizontal overlap for vertically intersecting obstacles.
- [x] Snap interval starts up and interval ends down to legal site columns.
- [x] Discard intervals narrower than the minimum movable cell width and keep remaining intervals sorted.
- [x] Report no-capacity and insufficient-total-capacity diagnostics before legalization.
- [x] Add tests for empty rows, centered macro splits, edge clipping, off-row obstacles, and partial-site fragments.

## Done When

- [x] Every interval is site-aligned, inside the die, and free of fixed obstacles.
- [x] Synthetic interval tests pass for split, clip, and discard behavior.
