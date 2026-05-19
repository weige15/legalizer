# Abacus Row Engine

## Goal

Optimize an ordered set of single-row cells inside one row segment while preserving row order and supporting non-mutating trials.

## Inputs

- `doc/proposal.md`: Use Abacus-style row optimization with cluster collapse instead of greedy slot filling.
- `doc/detailed-design.md`: Preserve original-X order with input-index tie breaks, detect infeasible capacity, and site-align final origins.

## Tasks

- [x] Represent committed row contents and trial row contents without mutating committed state during candidate evaluation.
- [x] Sort or insert cells by original X with deterministic input-index tie breaks.
- [x] Implement cluster creation, overlap detection, recursive merging, target computation, and segment clamping.
- [x] Expand clusters into non-overlapping, site-aligned legal origins inside the segment.
- [x] Return infeasible when total width or site snapping cannot fit the sequence.
- [x] Add tests for non-overlap preservation, forced cluster merging, left/right clamping, non-mutating trial insertion, and tie breaking.

## Done When

- [x] Single-row trial placement returns legal origins and displacement cost or an infeasible result.
- [x] Committed row state changes only through an explicit commit path.
