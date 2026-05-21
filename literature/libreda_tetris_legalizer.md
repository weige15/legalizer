# LibreEDA `tetris_legalizer`

Source type: open-source Rust crate documentation

URL: https://libreda.org/doc/tetris_legalizer/index.html

## Summary

The crate documents three legalizer variants:

- `TetrisLegalizer`: compact educational standard-cell legalizer.
- `DenseFirstLegalizer`: denser legalizer without pre-placed macro support.
- `DenseFirstLegalizerMS`: supports pre-placed macros and complicated placement-area shapes.

The docs define legalization as converting rough global-placement coordinates into legal, non-overlapping standard-cell positions.

## Relevance To The Assignment

This is useful as a compact conceptual reference. The assignment includes macros/blockages and density concerns, so the mixed-size/dense-first variant is more relevant than pure Tetris. The pure Tetris algorithm remains useful as a baseline because it is simple, fast, and easy to validate.

## Implementation Ideas

- Use Tetris as a first legal placement generator or fallback.
- Use dense-first thinking when repairing overfull regions for DOR.
- Keep a clear interface between placement-area geometry, cell ordering, and legalization policy.
- Prefer Abacus for final displacement quality if time permits.

