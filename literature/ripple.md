# Ripple

Source type: open-source C++ placement/legalization repository

URL: https://github.com/cuhk-eda/ripple

## Summary

Ripple is a VLSI placement tool from CUHK. Its README describes a holistic framework for mixed-cell-height legalization and routability-driven placement, including:

- window-based cell insertion,
- iterative network-flow maximum-displacement optimization,
- dual-network-flow fixed-row-and-order optimization,
- fence-aware and pin-accessible legalization papers.

## Relevance To The Assignment

The public assignment cases appear simpler than Ripple's target problem, but Ripple is valuable for advanced repair ideas once a baseline legalizer is working:

- high-displacement local repair,
- fixed-row/order compaction,
- nearby window insertion,
- routability or density-aware legal moves.

## Implementation Ideas

- Identify cells with unusually high displacement after Abacus/Tetris.
- Extract a small row/interval window around the original coordinate.
- Enumerate valid insertion gaps in nearby rows.
- Repack the affected window and commit only if assignment quality improves.
- Avoid full network-flow machinery unless simpler repairs fail on hidden cases.

