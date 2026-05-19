# Overflow Repair and Detailed Improvement

## Goal

Optionally improve a complete legal placement by moving cells out of overflowed regions while preserving legality.

## Inputs

- `doc/proposal.md`: After initial legalization, identify overflow grids and locally re-place selected cells into nearby underused intervals.
- `doc/detailed-design.md`: Repair is optional and should commit only moves that preserve legality and improve estimated quality.

## Tasks

- [ ] Add a repair entry point that can be disabled without changing base legalization behavior.
- [ ] Use exact DOR results to identify overflow grids and contributing movable cells.
- [ ] Trial-remove selected cells from their current interval and reinsert them into nearby underused intervals.
- [ ] Commit only improving moves that pass legality checks and quality scoring.
- [ ] Limit iteration count and candidate radius to protect benchmark runtime.
- [ ] Add tests for discovering an improving move, rejecting non-improving moves, and disabling repair.

## Done When

- [ ] Base legal output remains available when repair finds no improvement.
- [ ] Repair never leaves the final placement illegal.
- [ ] Repair tests pass or the module is explicitly disabled for the first implementation milestone.
