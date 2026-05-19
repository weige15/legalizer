# Row Interval Builder

## Goal

Convert the die area and fixed obstacles into snapped legal row intervals that can contain movable cells.

## Inputs

- `doc/proposal.md`: Rows are sliced by `MACRO` and `BLOCKAGE` regions and snapped to site columns.
- `doc/detailed-design.md`: Builder computes full rows, subtracts obstacle x-spans from overlapping rows, clips to die, and drops unusable intervals.

## Tasks

- [ ] Implement `src/row_interval_builder.{h,cpp}` with row and interval data structures.
- [ ] Generate full-height site rows from die bounds and site height while preserving empty row indexes.
- [ ] Subtract clipped macro and blockage spans from every overlapping row.
- [ ] Snap interval starts upward and interval ends downward to legal site columns.
- [ ] Add tests for empty die rows, middle macro split, edge blockage clipping, non-site-aligned obstacle snapping, and multi-row obstacle overlap.

## Done When

- [ ] Every row interval represents only legal, site-aligned horizontal placement space.
- [ ] Inputs with no legal interval fail early with a clear diagnostic.
- [ ] Row interval tests pass under `make test`.
