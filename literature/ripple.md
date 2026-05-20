# Ripple

Source type: open-source C++ placement/legalization repository  
URL: https://github.com/cuhk-eda/ripple

## Summary

Ripple is a VLSI placement tool from CUHK. Its README describes algorithms and data structures for mixed-cell-height legalization, routability-driven placement, maximum displacement optimization, and fixed-row-and-order optimization.

The repository cites several related papers, including:

- `Legalization Algorithm for Multiple-Row Height Standard Cell Design`, DAC 2016.
- `Routability-Driven and Fence-Aware Legalization for Mixed-Cell-Height Circuits`, DAC 2018.
- `Pin-Accessible Legalization for Mixed-Cell-Height Circuits`, TCAD 2021.

## Relevance To `p3_placement.pdf`

The assignment's public cases seem simpler than Ripple's target problem, but Ripple is valuable for advanced repair ideas once the baseline legalizer works:

- local window insertion;
- maximum displacement optimization;
- fixed row/order movement optimization;
- routability/density-aware legal moves.

## Implementation Ideas

- Add a local repair phase for cells with very high displacement.
- Try fixed-row/order compaction to reduce displacement after all cells are legal.
- Use window-based insertion only where a move reduces the final objective and preserves legality.

