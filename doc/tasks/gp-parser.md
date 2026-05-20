# GP Parser

## Goal

Parse assignment `.gp` files strictly into the placement model without guessing malformed input.

## Inputs

- `doc/proposal.md`: Required `.gp` fields and need to preserve original cell locations.
- `doc/detailed-design.md`: Exact header order, instance record shape, line-number diagnostics, and parser failure cases.

## Tasks

- [x] Read `.gp` input line by line and parse required metadata in exact order.
- [x] Accept the assignment column header and parse each instance as six whitespace-separated fields.
- [x] Convert lower-left plus width/height into half-open DBU rectangles with overflow checks.
- [x] Preserve input order for deterministic ties and output ordering.
- [x] Return parse diagnostics with line number and reason for malformed metadata or records.
- [x] Add fixtures for valid one-cell, macro/blockage, missing header, unknown type, negative dimension, and truncated record cases.

## Done When

- [x] Valid fixtures produce fully populated placement models.
- [x] Malformed fixtures fail deterministically with useful diagnostics.
