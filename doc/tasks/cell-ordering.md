# Cell Ordering

## Goal

Provide deterministic movable-cell orderings for forward and reverse legalization passes.

## Inputs

- `doc/proposal.md`: Cells are sorted by original x-coordinate, with optional decreasing x-order variant.
- `doc/detailed-design.md`: Tie breakers are original y-coordinate, name, and original input index as needed.

## Tasks

- [ ] Implement ordering helpers used by the legalization engine.
- [ ] Sort forward order by original `llx`, original `lly`, name, and input index.
- [ ] Implement reverse-x ordering while keeping deterministic tie handling.
- [ ] Return an empty order for designs with no movable cells.
- [ ] Add tests for equal-x tie breaking, reverse ordering, and empty input.

## Done When

- [ ] Legalization receives stable cell-id vectors for both direction modes.
- [ ] Repeated runs produce identical ordering for identical input.
- [ ] Ordering tests pass under `make test`.
