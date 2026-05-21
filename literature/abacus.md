# Abacus: Fast Legalization of Standard Cell Circuits with Minimal Movement

Source type: paper

Citation: Peter Spindler, Ulf Schlichtmann, Frank M. Johannes, ISPD 2008, DOI `10.1145/1353629.1353640`

URL: https://spacefrontiers.org/r/10.1145/1353629.1353640

DOI: https://doi.org/10.1145/1353629.1353640

## Summary

Abacus legalizes standard cells by sorting cells by global x-position, then inserting each cell into candidate rows. For every trial row, it inserts the cell into that row's order and runs a row optimizer often called `PlaceRow`. The row optimizer uses clusters to minimize movement while preserving order and avoiding overlap.

The key difference from Tetris is that already legalized cells in a row may move again when a new cell is inserted. This lowers average displacement compared with freezing each cell once placed.

## Relevance To The Assignment

The assignment score directly includes average displacement. Abacus directly targets this part of the score while satisfying row/site legality. It does not directly optimize DOR, but it is the strongest base legalizer before adding density repair.

## Implementation Notes

- Maintain row intervals/subrows split by macros and blockages.
- For each candidate row interval, insert the cell in original x-order.
- Run cluster collapse:
  - create a cluster when a cell does not overlap the previous cluster;
  - merge overlapping clusters;
  - compute optimal cluster x from accumulated weighted original positions and widths;
  - clamp clusters to interval bounds.
- Try forward and reverse x-order passes and keep the lower final quality.
- Use vertical movement as a lower bound to prune far rows once a good candidate exists.

## Expected Benefit

Abacus is likely the highest-impact upgrade when quality is displacement-dominated. It should generally beat a simple first-fit/Tetris legalizer on average movement.

