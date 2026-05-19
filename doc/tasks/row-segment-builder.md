# Row Segment Builder

## Goal

Construct legal site rows and free X segments by subtracting fixed obstacles from the die.

## Inputs

- `doc/proposal.md`: Build rows from die and site dimensions, subtract fixed obstacles, clip to die, and snap segment boundaries to legal site coordinates.
- `doc/detailed-design.md`: Keep empty rows, use half-open row spans, and expose segments that can test whether a cell origin fits.

## Tasks

- [x] Generate row Y coordinates from `DieArea_LL.y + row_index * Site_Height` while rows fit inside the die.
- [x] Start each row with the full die X interval and subtract clipped obstacle X projections when obstacle Y overlaps the row.
- [x] Merge blocked intervals and keep resulting free intervals in deterministic X order.
- [x] Snap free segment lower bounds up and upper bounds down to the site grid from `DieArea_LL.x`.
- [x] Drop unusable segments while preserving rows with empty segment lists.
- [x] Add tests for obstacle-free rows, middle macro subtraction, overlapping obstacles, site snapping, and row-boundary touching.

## Done When

- [x] Each row reports only legal, site-aligned free segments.
- [x] Segment tests cover obstacle clipping and half-open vertical overlap.
