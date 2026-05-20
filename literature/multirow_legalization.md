# Legalization Algorithm for Multiple-Row Height Standard Cell Design

Source type: paper  
Local file: `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf`  
Citation: Wing-Kai Chow, Chak-Wa Pui, Evangeline F. Y. Young, DAC 2016, DOI `10.1145/2897937.2898038`

## Summary

This paper addresses legalization when cells can span multiple placement rows. The core algorithm performs multi-row local legalization by selecting a local region, enumerating valid insertion points, evaluating displacement, and realizing a legal placement with minimal movement in that local window.

## Relevance To `p3_placement.pdf`

The assignment examples use site-height cells, so full multi-row machinery may not be necessary. The local insertion idea is still useful for post-processing: when a cell is placed far from its original position, a bounded local window can try to reinsert it closer and push neighboring cells minimally.

## Implementation Ideas

- Identify high-displacement cells after initial legalization.
- Extract a small row/interval window near the original coordinate.
- Enumerate insertion gaps in nearby rows.
- Commit the best legal move only if final quality improves.

