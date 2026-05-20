# Abacus: Fast Legalization of Standard Cell Circuits with Minimal Movement

Source type: paper  
Local file: `abacus.pdf`  
Citation: Peter Spindler, Ulf Schlichtmann, Frank M. Johannes, ISPD 2008, DOI `10.1145/1353629.1353640`  
Web metadata: https://spacefrontiers.org/r/10.1145/1353629.1353640

## Summary

Abacus legalizes standard cells by sorting cells by global x-position, then inserting each cell into candidate rows. For every trial row, it inserts the cell into that row's order and runs `PlaceRow`, a dynamic-programming cluster algorithm that minimizes row movement while preserving ordering and avoiding overlap.

The key difference from Tetris is that already legalized cells in a row may move again when a new cell is inserted. This lowers average displacement significantly compared with freezing each cell once placed.

## Relevance To `p3_placement.pdf`

The assignment primarily scores legality, average displacement, and DOR. Abacus directly targets the displacement part while handling row/site legality. It does not directly optimize DOR, but it is the strongest base legalizer before density repair.

## Implementation Notes

- Maintain row intervals/subrows split by macros and blockages.
- For each candidate row interval, insert the cell in original-x order.
- Run cluster collapse:
  - create a cluster when a cell does not overlap the previous cluster;
  - merge overlapping clusters;
  - compute optimal cluster x from accumulated weighted original positions and widths;
  - clamp clusters to interval bounds.
- Try forward and reverse x-order passes and keep the lower final quality.
- Use vertical movement as a lower bound to prune far rows once a good candidate exists.

## Expected Benefit

The paper reports about 30% lower average movement than Tetris-style legalization. For the current benchmark, where quality is displacement-dominated, this is likely the highest-impact algorithmic upgrade.

