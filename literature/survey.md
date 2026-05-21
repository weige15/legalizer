# Literature Survey: P3 Placement Legalization

Date: 2026-05-22

## Scope

`p3_placement_v2.pdf` asks for a standalone legalizer for OpenROAD-extracted global placement. The legalizer must place movable standard cells on legal rows/sites, avoid fixed macros and blockages, minimize average displacement, and reduce density overflow ratio (DOR) over 10 um by 10 um grids excluding macro-covered bins.

The assignment forbids emitting an OpenROAD `detailed_placement` command in the final TCL, so OpenROAD/OpenDP is useful as a reference, not as a command to call in the submitted output.

## Direct Assignment/Solution Search

I searched for exact public copies or solutions using distinctive terms:

- `"p3_placement_v2.pdf"`
- `"Placement with OpenROAD" "Legalizer"`
- `"./Legalizer <alpha> <threshold>"`
- `"Quality = alpha" "Average Displacement" "DOR" "Legalizer"`
- `"ispd15_mgc_matrix_mult_a" Legalizer`
- `"mgc_matrix_mult_a" "Legalizer"`

Outcome: I did not find an exact public solution repository for this NYCU assignment. Treat the notes below as algorithmic references, not assignment-specific code to copy. The assignment also explicitly forbids plagiarism.

## Best References For This Assignment

### 1. Abacus: Fast Legalization of Standard Cell Circuits with Minimal Movement

Type: paper

URL: https://spacefrontiers.org/r/10.1145/1353629.1353640

DOI: https://doi.org/10.1145/1353629.1353640

Why it matters: Abacus is the closest match to the average-displacement objective. It legalizes cells row-by-row while allowing already-placed cells in a row to move via cluster reoptimization. That is stronger than greedy Tetris placement when displacement dominates the score.

Implementation direction:

- Split rows into legal intervals around macros and blockages.
- Sort movable cells by original x-coordinate.
- Try candidate rows near the original y-coordinate.
- For each trial insertion, run an Abacus-style cluster row solver.
- Prune far rows once vertical displacement alone exceeds the current best candidate.

### 2. Improved Parallel Legalization Schemes for Standard Cell Placement with Obstacles

Type: paper

URL: https://www.mdpi.com/2227-7080/7/1/3

DOI: https://doi.org/10.3390/technologies7010003

Why it matters: The assignment includes macros and blockages, which create obstacle-aware row intervals. This paper explicitly extends Tetris and Abacus to handle obstacles and discusses partitioning the chip area for runtime.

Implementation direction:

- Model each row as subrows/intervals separated by obstacle projections.
- Treat each interval independently for row packing.
- Use partitioning only after single-threaded legality and quality are stable.

### 3. OpenROAD Detailed Placement / OpenDP

Type: official documentation and open-source C++ codebase

Docs: https://openroad.readthedocs.io/en/latest/main/src/dpl/README.html

Code: https://github.com/The-OpenROAD-Project/OpenROAD/tree/master/src/dpl

Why it matters: OpenROAD's detailed placement module is the practical tool environment for this assignment. The docs describe fragmented rows, macro blocks, fence regions, mixed-cell-height handling, a diamond-search placement engine, and a negotiation legalizer with an optional Abacus pre-pass.

Implementation direction:

- Use OpenDP's architecture as a reference for row/site representation and legality checking.
- Adapt the idea of diamond/BFS search from a cell's global-placement location as a local fallback.
- Do not emit `detailed_placement` in final TCL.

### 4. LibreEDA `tetris_legalizer`

Type: open-source Rust crate documentation

URL: https://libreda.org/doc/tetris_legalizer/index.html

Why it matters: This is a compact educational implementation reference for legalization. Its documented variants include a basic Tetris legalizer and a mixed-size/dense-first legalizer that supports pre-placed macros and complicated placement-area shapes.

Implementation direction:

- Use Tetris as a simple baseline or fallback path.
- Use the mixed-size/dense-first framing as a reference for macro/blockage handling.
- Prefer Abacus-style row reoptimization for final displacement quality.

### 5. Density-aware Detailed Placement with Instant Legalization

Type: paper metadata and algorithm reference

URL: https://scholar.nycu.edu.tw/en/publications/density-aware-detailed-placement-with-instant-legalization/

DOI: https://doi.org/10.1145/2593069.2593142

Why it matters: The assignment explicitly includes DOR in the score. This DAC 2014 paper targets peak bin utilization under displacement constraints using a lazy density-profit function and density-driven swaps.

Implementation direction:

- Add a post-legalization density repair phase only after base legality is robust.
- Track 10 um by 10 um bin occupancy excluding macro-covered bins.
- Try local moves or swaps that reduce overflow bins without increasing displacement enough to hurt the final weighted objective.

### 6. Ripple

Type: open-source C++ placement/legalization repository

URL: https://github.com/cuhk-eda/ripple

Why it matters: Ripple is an advanced legalization and routability-driven placement codebase. It includes mixed-cell-height legalization, maximum displacement optimization, and fixed-row/order optimization. The assignment cases are simpler, but Ripple gives useful local repair ideas.

Implementation direction:

- Use window-based insertion for high-displacement outliers.
- Use fixed-row/order optimization as a local compaction/refinement pass.
- Treat network-flow maximum displacement optimization as a later upgrade, not the first implementation.

### 7. easyPlace

Type: open-source C++ placement repository

URL: https://github.com/geziangfinn/easyPlace

Why it matters: The README says the project reimplements ePlace/ePlace-MS and applies Abacus for internal standard-cell legalization. It also includes detailed-placement techniques such as independent set matching, local reordering, and global swap.

Implementation direction:

- Study the separation between legalization and detailed-placement improvement passes.
- Use Abacus as the base legalizer, then add local reorder/swap style refinements.

### 8. DREAMPlace

Type: open-source placement repository

URL: https://github.com/limbo018/DREAMPlace

Why it matters: DREAMPlace is mainly a global placer, but its later versions integrate ABCDPlace for detailed placement and support independent set matching, local reordering, global swap, Tetris-like macro legalization, and min-cost-flow refinement.

Implementation direction:

- It is too large to adapt directly for this assignment.
- Its detailed-placement feature list supports the same high-level plan: legalize first, then improve by deterministic local reorder/swap operations.

### 9. History-based VLSI Legalization Using Network Flow

Type: paper metadata

URL: https://research.ibm.com/publications/history-based-vlsi-legalization-using-network-flow

Why it matters: This DAC 2010 paper frames legalization as min-cost network flow to minimize deviation from global placement. It is more complex than needed for a homework legalizer, but useful if the design has severe overflow or high max displacement.

Implementation direction:

- Consider flow only as a later-stage repair for congested regions.
- For the near term, Abacus plus density repair is more implementation-efficient.

## Recommended Implementation Path

1. Build a robust row-interval model from the die area, site dimensions, macros, and blockages.
2. Implement a Tetris baseline for guaranteed legality.
3. Replace or augment it with Abacus cluster placement to reduce average displacement.
4. Add local repair for high-displacement cells using nearby row windows.
5. Add DOR-aware swaps/moves that evaluate the assignment's exact weighted metric.
6. Keep output limited to explicit `place_cell` commands.

