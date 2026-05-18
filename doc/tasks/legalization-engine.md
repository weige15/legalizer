# Legalization Engine

## Goal

Assign every movable cell to a legal site-row origin using deterministic candidate search that balances displacement and density pressure.

## Inputs

- `doc/proposal.md`: Legality is primary; among legal placements, reduce `alpha * Average Displacement + (1 - alpha) * DOR`.
- `doc/detailed-design.md`: Sort cells deterministically, search nearby rows and free intervals, score candidates by displacement and density, expand fallback search on failure, and commit through row and density models.

## Tasks

- [ ] Sort cells by snapped original row, original X, descending width tie-breaker, and input index.
- [ ] Generate candidate rows by increasing vertical distance from each cell's original row.
- [ ] Generate candidate X positions from clamped original X, interval boundaries, and nearby legal sites.
- [ ] Score legal candidates with `alpha * displacementMicrons + (1 - alpha) * densityCost`.
- [ ] Implement deterministic tie-breaking by score, displacement, row distance, X, and row index.
- [ ] Commit the selected candidate through row occupancy and density updates, storing final cell origins.
- [ ] Add full-design fallback search and a clear fatal diagnostic when a cell cannot be legalized.
- [ ] Add synthetic tests for already-legal cells, overlapping cells, macro-split rows, density-aware movement, and fallback expansion.

## Done When

- [ ] Every movable cell receives a placement or the program fails with the blocking cell identified.
- [ ] The placement is deterministic for the same input and parameters.
- [ ] Initial placement passes internal validation on synthetic legalizable cases.
