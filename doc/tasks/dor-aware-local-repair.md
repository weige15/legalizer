# DOR-Aware Local Repair

## Goal

Improve the completed legal placement with bounded local moves that preserve legality and accept only equal-or-better metric outcomes under the configured policy.

## Inputs

- `doc/proposal.md`: Local repair moves cells, swaps cells, reinserts cells, and reorders windows to reduce DOR while preserving legality.
- `doc/detailed-design.md`: DOR-Aware Local Repair ranks overflow grids, snapshots affected state, tests candidate operations, and stops under fixed iteration limits.

## Tasks

- [ ] Compute current metrics and rank overflow grids by density.
- [ ] Collect and rank candidate cells contributing area to each inspected overflow grid.
- [ ] Implement bounded reinsertion and gap-move candidates using affected interval snapshots.
- [ ] Implement swap or small-window reorder candidates only when both affected intervals can be repacked legally.
- [ ] Accept candidates that improve weighted quality and restore all placements and interval lists on rejection.
- [ ] Add tests for no-overflow no-op, accepted move out of overflow, rejected-candidate rollback, legal-only swap acceptance, and iteration-limit termination.

## Done When

- [ ] Repair never makes a legal baseline placement illegal.
- [ ] Repair keeps the baseline placement when no improving candidate is found.
- [ ] DOR repair tests pass.
