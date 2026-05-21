# Improved Parallel Legalization Schemes for Standard Cell Placement with Obstacles

Source type: paper

Citation: Panagiotis Oikonomou, Antonios N. Dadaliaris, Kostas Kolomvatsos, Thanasis Loukopoulos, Athanasios Kakarountas, Georgios I. Stamoulis, Technologies 2019, DOI `10.3390/technologies7010003`

URL: https://www.mdpi.com/2227-7080/7/1/3

DOI: https://doi.org/10.3390/technologies7010003

## Summary

This paper extends Tetris and Abacus legalization to standard-cell placement with obstacles, such as fixed preplaced modules. It also studies partitioning strategies that reduce search space and improve runtime through parallel execution.

## Relevance To The Assignment

The assignment has fixed `MACRO` and `BLOCKAGE` rectangles, so pure Abacus/Tetris descriptions are incomplete unless row intervals are split around obstacles. This paper is directly relevant to that missing piece.

## Implementation Notes

- Project each macro/blockage onto overlapping placement rows.
- Split every affected row into legal subrows.
- Run row packing within a subrow only when the cell fits.
- Keep obstacle handling in the row data model rather than adding special collision cases during placement.
- Delay parallelization until the serial row/subrow legalizer is correct.

## Practical Takeaway

The simplest assignment-compatible adaptation is obstacle-aware subrow construction plus Abacus row insertion. Parallel partitioning is optional because the assignment timeout is 30 minutes and the public cases are small enough to prioritize quality first.

