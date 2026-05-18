# GP Parser

## Goal

Parse the assignment `.gp` file into typed design metadata and object records while preserving deterministic input order.

## Inputs

- `doc/proposal.md`: Input contains DBU, die bounds, site dimensions, and `Name LLX LLY Width Height Type` records for `CELL`, `MACRO`, and `BLOCKAGE`.
- `doc/detailed-design.md`: Parser owns file syntax validation, required metadata detection, line-number diagnostics, signed 64-bit geometry, and object type classification.

## Tasks

- [ ] Implement `ParsedDesign` and `ObjectRecord` structures for metadata and raw records.
- [ ] Read metadata keys `DBU_Per_Micron`, `DieArea_LL`, `DieArea_UR`, `Site_Width`, and `Site_Height`.
- [ ] Locate and validate the `Name LLX LLY Width Height Type` header while tolerating optional blank lines.
- [ ] Parse records into rectangles from lower-left plus width and height, rejecting non-positive dimensions.
- [ ] Reject missing metadata, malformed numbers, unknown types, and malformed records with line numbers.
- [ ] Preserve each record's input order for downstream deterministic tie-breaking.
- [ ] Add parser fixtures for the PDF sample shape, missing metadata, bad numeric fields, unknown type, and invalid dimensions.

## Done When

- [ ] Valid `.gp` files load into a complete `ParsedDesign`.
- [ ] Parser failures include enough context to fix the offending line or missing key.
- [ ] `CELL`, `MACRO`, and `BLOCKAGE` records are distinguishable without downstream string parsing.
