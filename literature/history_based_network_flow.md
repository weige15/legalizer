# History-based VLSI Legalization Using Network Flow

Source type: paper metadata

Citation: Minsik Cho, Haoxing Ren, Hua Xiang, Ruchir Puri, DAC 2010

URL: https://research.ibm.com/publications/history-based-vlsi-legalization-using-network-flow

## Summary

This paper introduces a history-based legalization algorithm using min-cost network flow. It aims to find a legal placement with minimum deviation from the input placement and uses history information to avoid repeatedly trying movements that tend to fail.

## Relevance To The Assignment

The assignment's primary cost includes displacement from the input placement. Network-flow legalization directly targets that objective and can help in highly congested designs where greedy row assignment produces large displacement.

## Implementation Ideas

- Treat this as an advanced repair direction, not a first implementation.
- Use flow on local congested regions, not the entire design.
- Use row intervals as bins with capacities measured in sites.
- Assign cells to nearby intervals with movement cost, then run row packing.

## Practical Takeaway

For the homework timeline, Abacus plus local density repair is more practical. Flow is useful if hidden benchmarks create severe congestion around macros/blockages.

