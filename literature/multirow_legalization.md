# Legalization Algorithm for Multiple-Row Height Standard Cell Design

Source type: paper

Citation: Wing-Kai Chow, Chak-Wa Pui, Evangeline F. Y. Young, DAC 2016, DOI `10.1145/2897937.2898038`

Related repository reference: https://github.com/cuhk-eda/ripple

DOI: https://doi.org/10.1145/2897937.2898038

## Summary

This paper addresses legalization when cells can span multiple placement rows. The core approach uses local regions/windows, insertion-point enumeration, and displacement-aware legal movement while respecting row/order constraints.

## Relevance To The Assignment

The assignment examples appear to use single-row standard cells, so the full multi-row machinery is probably unnecessary. The local insertion framework is still useful for post-processing:

- repair cells with high displacement,
- move cells into nearby legal gaps,
- preserve legality while changing a small window,
- avoid destabilizing the whole placement.

## Implementation Ideas

- Identify high-displacement cells after initial legalization.
- Extract a small window of nearby rows/subrows.
- Enumerate legal insertion positions around the cell's original coordinate.
- Repack affected cells with an Abacus-style row solver.
- Commit only if the final assignment quality improves.

## Practical Takeaway

Use this as a local repair pattern after a robust single-row Abacus or Tetris legalizer is already working. It is a refinement source, not the base legalizer for this homework.

