# Abacus Row Solver

## Goal

Compute legal X positions for all cells assigned to one row interval using Abacus cluster collapse.

## Inputs

- `doc/proposal.md`: Abacus as the primary legalization engine and cluster merge behavior.
- `doc/detailed-design.md`: Abacus Row Solver inputs, ordered trial output, cluster construction, clamping, and failure cases.

## Tasks

- [x] Represent an interval trial as existing assigned cells plus an optional inserted candidate.
- [x] Sort trial cells by target original X with stable id tie-breaks.
- [x] Build, merge, and clamp clusters using total width and weighted target position.
- [x] Expand final clusters into non-overlapping lower-left X positions at the interval Y.
- [x] Return failure for over-capacity intervals or cells wider than the interval.
- [x] Add tests for one-cell clamp, two-cell merge, left and right boundary clamps, over-capacity failure, and stable ties.

## Done When

- [x] Trial placements are non-overlapping, in interval bounds, and deterministic.
- [x] Solver tests cover cluster merging and boundary clamping.
