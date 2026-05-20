# Placement Model

## Goal

Create the shared DBU geometry model, instance storage, placement state, and metric data contracts used by all modules.

## Inputs

- `doc/proposal.md`: Integer DBU normalization, movable/fixed instance separation, original coordinate preservation, and single-row scope assumptions.
- `doc/detailed-design.md`: Placement Model module types, half-open rectangle contract, helper functions, and unsupported multi-row handling.

## Tasks

- [x] Implement `Point`, `Rect`, `Instance`, `RowInterval`, `Metrics`, and instance type definitions.
- [x] Store all coordinates and dimensions as signed 64-bit DBU values with half-open rectangle semantics.
- [x] Add geometry helpers for overlap, overlap area, site snapping, row conversion, and alignment checks.
- [x] Partition movable `CELL` instances from fixed `MACRO` and `BLOCKAGE` instances with stable cell ids.
- [x] Reject invalid model metadata, invalid instance dimensions, unknown types, and unsupported multi-row movable cells.
- [x] Add direct unit tests for overlap edges, snapping, row conversion, partitioning, and multi-row detection.

## Done When

- [x] Other modules can use typed model objects without raw string parsing or micron conversion.
- [x] Geometry unit tests pass for touching-edge and true-overlap cases.
