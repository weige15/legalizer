# Local Refinement

## Goal

Improve displacement and density after the initial legal placement while preserving the same legality guarantees.

## Inputs

- `doc/proposal.md`: Refinement is intended to reduce average displacement and smooth density after the first legal solution.
- `doc/detailed-design.md`: Use bounded single-cell relocation, row-local compaction, and optional nearby swaps through commit/uncommit APIs with rollback on failed trials.

## Tasks

- [ ] Add a bounded single-cell relocation pass ordered by high displacement or density contribution.
- [ ] Use row and density uncommit/commit operations for every trial move and restore the original placement on rejection.
- [ ] Add row-local compaction that shifts cells toward original X without changing row order or breaking intervals.
- [ ] Add an optional nearby pair-swap pass only when both cells remain legal and combined cost improves.
- [ ] Stop refinement early when a full pass makes no accepted changes.
- [ ] Validate after each pass or accepted batch and fail loudly on occupancy inconsistency.
- [ ] Add tests for accepted moves, rejected move rollback, compaction legality, and no-improvement early stop.

## Done When

- [ ] Refinement never produces a placement that fails internal legality checks.
- [ ] Accepted moves do not increase the local cost used by the pass.
- [ ] Runtime remains bounded by explicit pass and candidate limits.
