# Density-aware Detailed Placement with Instant Legalization

Source type: paper metadata and algorithm reference

Citation: Sergiy Popovych, Hung Hao Lai, Chieh Min Wang, Yih-Lang Li, Wen Hao Liu, Ting Chi Wang, DAC 2014, DOI `10.1145/2593069.2593142`

URL: https://scholar.nycu.edu.tw/en/publications/density-aware-detailed-placement-with-instant-legalization/

DOI: https://doi.org/10.1145/2593069.2593142

## Summary

This detailed-placement work targets both wirelength and peak bin utilization while preserving legality during moves. The paper presents a lazy-update incremental density profit function and density-driven swaps to reduce bin-utilization problems under displacement constraints.

## Relevance To The Assignment

The assignment explicitly scores DOR, so a pure displacement legalizer may rank poorly when `alpha` is low. Density-aware local improvement is a natural second phase after initial legalization.

## Implementation Ideas

- Maintain assignment-specific 10 um by 10 um density bins.
- Exclude fixed macro-covered bins when computing DOR.
- Evaluate a legal move by both displacement delta and overflow-bin reduction.
- Try local moves/swaps in overflow bins first.
- Commit only moves that reduce the final weighted objective:

```text
quality = alpha * average_displacement + (1 - alpha) * DOR
```

## Practical Takeaway

Do not start with density repair. First ensure legality and reasonable displacement, then add targeted overflow-bin improvement.

