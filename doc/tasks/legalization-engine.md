# Legalization Engine

## Goal

Place all movable cells legally by choosing row intervals with displacement-aware and density-aware trial scoring.

## Inputs

- `doc/proposal.md`: Process cells in x-order, try nearby rows, use ABACUS row trials, optionally compare reverse order, and choose the best complete legal solution.
- `doc/detailed-design.md`: Engine owns candidate search, final insertion commits, full-pass validation, and failure diagnostics.

## Tasks

- [ ] Implement `src/legalizer.{h,cpp}` with pass state for rows, intervals, committed cells, and placed coordinates.
- [ ] For each cell, search candidate rows around original y and all intervals wide enough for the cell.
- [ ] Score feasible row trials using movement delta, vertical movement, density penalty, and `alpha`.
- [ ] Add fallback full-row search when pruning finds no feasible candidate.
- [ ] Run forward and reverse ordering variants and keep the legal solution with lower quality.
- [ ] Return cell-specific failure diagnostics when no interval can fit a cell.
- [ ] Add tests for one cell, overlapping cells in one row, macro gap placement, fallback row search, and reverse-pass comparison.

## Done When

- [ ] Every movable cell receives a legal placement or the engine reports a clear failure.
- [ ] Forward and reverse passes are deterministic and independently comparable.
- [ ] Legalization engine tests pass under `make test`.
