# Candidate Scoring

## Goal

Score row-insertion trials with a deterministic scalar that balances flow-compatible displacement and estimated DOR pressure.

## Inputs

- `doc/proposal.md`: Candidate cost formula and high-`alpha` versus low-`alpha` behavior.
- `doc/detailed-design.md`: Candidate Scoring contract, normalized displacement factor, density estimate inputs, tie-breaker penalties, and fallback behavior.

## Tasks

- [x] Implement candidate cost as displacement delta plus estimated DOR delta weighted by `alpha`.
- [x] Use flow-compatible normalized displacement based on average displacement in microns multiplied by `18.2`.
- [x] Add deterministic local penalties for over-threshold bins, interval-edge placement, and large vertical movement.
- [x] Keep penalty constants named and centralized for benchmark tuning.
- [x] Fall back to displacement-first scoring if density estimates are unavailable.
- [x] Add tests for displacement preference, overflow-pressure penalty, `alpha = 1.0`, and `alpha = 0.0` behavior.

## Done When

- [x] Equal-displacement trials prefer lower overflow pressure.
- [x] Equal-density trials prefer lower normalized displacement.
