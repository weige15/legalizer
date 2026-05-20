# Legalization Engine

## Goal

Assign every movable cell to legal row intervals, run deterministic placement variants, perform bounded repair, and return the best legal placement candidate.

## Inputs

- `doc/proposal.md`: Abacus flow, deterministic variants, density-aware behavior, local repair, and best-result selection.
- `doc/detailed-design.md`: Legalization Engine module ordering, candidate interval pruning, repair acceptance rules, and variant failure handling.

## Tasks

- [x] Precompute candidate intervals by row distance and interval capacity for each movable cell.
- [x] Implement deterministic placement orders for left-to-right, right-to-left, low-`alpha` density-first, and large-cell dense-region tie-breaks.
- [x] Insert cells by evaluating feasible interval trials with the Abacus row solver and candidate scoring policy.
- [x] Prune candidate search by vertical displacement lower bound, bounded row window, and expandable fallback when no fit is found.
- [x] Keep the best exact validated result across successful variants.
- [x] Add bounded local repair for displacement outliers and overflow-bin cells with exact-quality acceptance rules.
- [x] Add tests for single-cell, two-overlap, macro-split row, variant legality, low-`alpha` overflow preference, and repair non-regression.

## Done When

- [x] All movable cells receive legal candidate placements or the failing cell and reason are reported.
- [x] At least one deterministic variant can legalize the small integration fixtures.
