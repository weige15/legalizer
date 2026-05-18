# Row and Obstacle Model

## Goal

Build and maintain the legal placement space by deriving site rows, subtracting fixed obstacles, and tracking committed movable-cell occupancy.

## Inputs

- `doc/proposal.md`: Cells must be inside the die, site-row aligned, site-column aligned, and non-overlapping with cells, macros, and blockages.
- `doc/detailed-design.md`: Rows are derived from die Y bounds and `Site_Height`; free intervals subtract obstacles, snap to legal site columns, and support `canPlace`, slot enumeration, commit, and uncommit.

## Tasks

- [ ] Generate legal row origins from `DieArea_LL.y`, `DieArea_UR.y`, and `Site_Height`.
- [ ] Initialize each row with the die-width free interval and subtract intersecting macro/blockage spans.
- [ ] Snap free interval starts and ends to site-column legality so candidates can contain full cell widths.
- [ ] Implement `canPlace(cell, x, y)` for one-row and multi-row cells across all touched rows.
- [ ] Implement candidate slot enumeration near a target X and target row with bounded per-row output.
- [ ] Implement occupancy commit and uncommit without allowing illegal mutations.
- [ ] Add synthetic tests for obstacle subtraction, boundary-touching intervals, multi-row cells, and occupied-row overlap.

## Done When

- [ ] Every candidate accepted by this module satisfies die bounds, row alignment, site alignment, and obstacle avoidance.
- [ ] Committed movable cells cannot overlap later placements.
- [ ] The module reports when no legal row or no possible slot exists for a cell.
