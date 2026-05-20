# Density-aware Detailed Placement with Instant Legalization

Source type: paper metadata / algorithm reference  
NYCU page: https://scholar.nycu.edu.tw/en/publications/density-aware-detailed-placement-with-instant-legalization/  
Citation: Sergiy Popovych, Hung Hao Lai, Chieh Min Wang, Yih-Lang Li, Wen Hao Liu, Ting Chi Wang, DAC 2014, DOI `10.1145/2593069.2593142`

## Summary

This detailed placement work targets both wirelength and peak bin utilization while preserving legality during moves. It uses lazy density profit computation and density-driven swaps to improve density metrics under displacement constraints.

## Relevance To `p3_placement.pdf`

The assignment explicitly scores DOR, so this is relevant once the base legalizer has reasonable displacement. For the current `mgc_matrix_mult_a` run, displacement dominates quality, but density-aware repair can still reduce DOR after displacement improves.

## Implementation Ideas

- Maintain density bins during local moves.
- Score a move by overflow-bin reduction and displacement delta.
- Try swaps or small-window shifts in overflow grids.
- Commit only if the final assignment quality improves.

