# LibreEDA `tetris_legalizer`

Source type: open-source Rust crate documentation  
URL: https://libreda.org/doc/tetris_legalizer/index.html

## Summary

The crate documents three legalizers:

- `TetrisLegalizer`: compact educational standard-cell legalizer.
- `DenseFirstLegalizer`: denser legalizer without pre-placed macro support.
- `DenseFirstLegalizerMS`: supports pre-placed macros and more complicated placement-area shapes.

It frames legalization as converting rough global-placement coordinates into legal, non-overlapping standard-cell positions.

## Relevance To `p3_placement.pdf`

This is useful as a clean conceptual implementation reference. The assignment includes macros/blockages and density concerns, so the mixed-size/dense-first variants are more relevant than pure Tetris.

## Implementation Ideas

- Use Tetris as a baseline or fallback because it is simple and fast.
- Prefer Abacus-like row reoptimization for lower displacement.
- Borrow the idea of separating simple, dense-first, and mixed-size-capable legalizer variants in tests.

