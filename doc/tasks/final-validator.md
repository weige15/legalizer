# Final Validator

## Goal

Gate output generation by checking every legality constraint and recomputing exact metrics.

## Inputs

- `doc/proposal.md`: Required legality checks, no-overlap constraints, displacement and DOR recomputation before writing.
- `doc/detailed-design.md`: Final Validator checks, metric formulas, efficient row grouping, and failure reporting expectations.

## Tasks

- [x] Check every movable cell has a placement inside the die.
- [x] Check site X alignment, legal row Y alignment, and supported one-row cell height.
- [x] Verify each placed cell rectangle lies inside one legal row interval.
- [x] Detect cell-cell overlaps by row grouping and sorted X sweeps.
- [x] Detect direct overlap with every macro and blockage as a defensive validation.
- [x] Compute average displacement, normalized displacement, exact DOR, flow quality, and handout quality.
- [x] Add tests for unplaced, off-site, off-row, cell-cell overlap, obstacle overlap, out-of-die, and small-fixture metrics.

## Done When

- [x] Validation returns all easily collected violation categories.
- [x] Output writing is impossible unless final validation passes.
