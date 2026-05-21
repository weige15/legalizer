# easyPlace

Source type: open-source C++ placement repository

URL: https://github.com/geziangfinn/easyPlace

## Summary

easyPlace is a clean C++ reimplementation of ePlace and ePlace-MS. Its README states that it applies Abacus for internal standard-cell legalization and uses independent set matching, local reordering, and global swap for detailed placement.

## Relevance To The Assignment

The assignment does not need a full global placer, but easyPlace is useful because it separates global placement, legalization, and detailed-placement improvement passes. That maps well onto the homework structure: the `.gp` input is already a global placement, and the legalizer should preserve it while removing overlaps.

## Implementation Ideas

- Treat Abacus as the base legalization engine.
- Add local reordering after legality, especially within each row interval.
- Add global or local swaps only if they improve the assignment's weighted quality.
- Keep the implementation standalone and emit explicit OpenROAD `place_cell` commands.

