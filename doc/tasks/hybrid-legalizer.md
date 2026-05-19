# Hybrid Legalizer

## Goal

Own final placement: run deterministic trials, score legal candidates, commit placements, keep the best legal result, and apply bounded density smoothing.

## Inputs

- `doc/proposal.md`: Legalize first, optimize displacement and DOR second, use deterministic trials, and keep runtime under the assignment timeout.
- `doc/detailed-design.md`: Coordinate Abacus, multi-row placement, density scoring, local-to-global fallback, and smoothing.

## Tasks

- [x] Implement deterministic trial orders for increasing X, decreasing X, and large-or-tall cells first.
- [x] For each cell, enumerate candidate rows or row spans in increasing displacement from original Y.
- [x] Request trial placements from the Abacus Row Engine or Multi-Row Placement Layer.
- [x] Score feasible candidates with displacement delta, estimated DOR delta, and deterministic tie breakers.
- [x] Broaden local search to all feasible rows or row spans before declaring a trial failure.
- [x] Keep the best completed trial and run a bounded smoothing pass that accepts only legality-preserving score improvements.
- [x] Add tests for one-cell legalization, overlapping cells, macro gaps, deterministic repeated runs, illegal smoothing rejection, and too-full failure.

## Done When

- [x] Every movable cell has final legal coordinates when a legal solution exists in test fixtures.
- [x] Failed legalization reports a clear error and never produces overlapping fallback placements.
