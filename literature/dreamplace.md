# DREAMPlace

Source type: open-source placement repository

URL: https://github.com/limbo018/DREAMPlace

## Summary

DREAMPlace is an open-source placement framework using deep-learning toolkits for GPU acceleration. Its README lists ABCDPlace integration for detailed placement, independent set matching, local reordering, global swap, movable macro support with Tetris-like macro legalization, and min-cost-flow refinement.

## Relevance To The Assignment

DREAMPlace is much larger than this assignment and should not be adapted directly. It is still useful as evidence for the standard structure of a modern placement flow:

1. produce or receive global placement,
2. legalize to rows/sites,
3. improve quality through deterministic local detailed-placement operations.

## Implementation Ideas

- Use the feature list to guide post-legalization refinements:
  - independent small-window local reorder,
  - row-local swaps,
  - min-cost-flow style repair only for hard congested regions.
- Keep runtime and implementation complexity appropriate for the 30-minute grading timeout.

