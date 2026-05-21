# Row Interval Builder

## Goal

Generate obstacle-free, site-aligned legal row intervals by splitting site rows around fixed macros and blockages.

## Inputs

- `doc/proposal.md`: Fixed `MACRO` and `BLOCKAGE` rectangles split rows into independent legal intervals.
- `doc/detailed-design.md`: Row Interval Builder owns row generation, obstacle projection, outward site snapping, interval subtraction, capacity, and deterministic sorting.

## Tasks

- [ ] Generate site rows within the die using `siteHeight`, excluding partial rows above `die.uy`.
- [ ] Project each overlapping macro or blockage onto row bands using half-open y-overlap semantics.
- [ ] Clip obstacle projections to the die, snap blocked spans outward to site boundaries, and subtract them from row spans.
- [ ] Drop intervals with no full-site capacity and store sorted site-aligned `[xMin, xMax)` bounds.
- [ ] Track interval row index, y-coordinate, occupied width, and assigned cell list fields for later legalizer stages.
- [ ] Add tests for no obstacles, outside obstacles, middle splits, boundary touches, full-row coverage, multi-row obstacles, and row-boundary contact.

## Done When

- [ ] Row intervals are sorted, site-aligned, obstacle-free, and deterministic.
- [ ] Fully blocked rows produce no legal intervals without failing the whole build.
- [ ] Row interval builder tests pass.
