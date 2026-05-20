# Literature Survey: P3 Placement / Legalization

Date: 2026-05-21

## Scope

The assignment `p3_placement.pdf` asks for a standalone `Legalizer <alpha> <threshold> <input.gp> <output.tcl>` implementation for OpenROAD-generated global placement. The legalizer must place movable standard cells on legal rows/sites, avoid macros/blockages, minimize average displacement, and reduce density overflow ratio (DOR) over 10um x 10um grids excluding macro-covered bins. It forbids using `detailed_placement` in the submitted output TCL.

## Direct Assignment/Solution Search

I searched for exact public copies or solutions using the distinctive terms:

- `"p3_placement.pdf" "Legalizer"`
- `"./Legalizer <alpha> <threshold>" placement`
- `"Placement with OpenROAD" "Programming Assignment #3"`
- `"Mark P.-H. Lin" "Placement with OpenROAD"`
- `"Quality = α" "Average Displacement" "DOR" "Legalizer"`
- `"ispd15_mgc_matrix_mult_a" Legalizer GitHub`
- `"mgc_matrix_mult_a" "Legalizer"`

Result: I did not find an exact public solution repository for this NYCU assignment. Treat the sources below as algorithmic references, not assignment-specific solutions. The assignment also explicitly forbids plagiarism.

## Most Useful Sources

### 1. Abacus: Fast Legalization of Standard Cell Circuits with Minimal Movement

Type: paper; local copy: `abacus.pdf`

Why it matters: This is the closest match to the assignment objective of minimizing displacement while legalizing to rows. The method sorts cells, tries candidate rows, inserts each cell into the row order, and uses a cluster/dynamic-programming row solver (`PlaceRow`) to minimize movement of the row after insertion. The paper reports around 30% lower average movement than Tetris-style legalization.

Implementation ideas for this repo:

- Replace interval re-solving with true Abacus cluster collapse.
- Use row search with vertical lower-bound pruning.
- Compare left-to-right and right-to-left passes.
- Score candidates by incremental final objective, not only the inserted cell.

### 2. OpenROAD Detailed Placement / OpenDP

Type: open-source implementation and documentation

URLs:

- https://openroad.readthedocs.io/en/latest/main/src/dpl/README.html
- https://github.com/The-OpenROAD-Project/OpenROAD/tree/master/src/dpl

Why it matters: OpenROAD's `dpl` module is the tool the assignment prohibits directly calling in output TCL, but its public code/docs explain practical legalization strategies. Current docs describe a default diamond-search legalizer and an optional negotiation legalizer with an Abacus pre-pass.

Implementation ideas for this repo:

- Diamond/BFS search from original cell location is a simple baseline for reducing max displacement.
- `improve_placement`-style local swaps can be adapted as a post-legalization refinement, without calling OpenROAD's command.
- Fragmented rows, macro blocks, and mixed-height support in OpenDP are useful design references for row interval handling.

### 3. LibreEDA `tetris_legalizer`

Type: open-source Rust implementation

URLs:

- https://libreda.org/doc/tetris_legalizer/index.html

Why it matters: This is a compact educational legalizer implementation. The docs say it includes Tetris, DenseFirst, and DenseFirstMS legalizers, including support for pre-placed macros/complicated placement areas in the MS variant.

Implementation ideas for this repo:

- Use as a conceptual reference for simple legalizer architecture and test cases.
- Tetris-like legalization is fast and easy to implement, but likely worse for displacement than Abacus.
- Dense-first variants may help DOR because they consider occupied regions explicitly.

### 4. Ripple

Type: open-source C++ placement/legalization repository

URL: https://github.com/cuhk-eda/ripple

Why it matters: Ripple is a CUHK EDA placement tool with mixed-cell-height legalization, routability-driven placement, maximum displacement optimization, and fixed-row/order optimization. It cites several related legalization papers.

Implementation ideas for this repo:

- Window-based insertion can be adapted for local repair of high-displacement cells.
- Network-flow maximum displacement optimization is overkill for the current assignment, but useful if max displacement remains very large.
- Fixed-row-and-order optimization aligns with our row interval model.

### 5. Legalization Algorithm for Multiple-Row Height Standard Cell Design

Type: paper; local copy: `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf`

Why it matters: The public cases appear single-row, but this paper gives a local insertion/legalization framework that is useful for window-based post-processing. It minimizes displacement in a local region while preserving row/order constraints.

Implementation ideas for this repo:

- Local window repair around high-displacement cells.
- Enumerate insertion gaps and evaluate displacement before committing.
- Keep this as a later-stage optimization after a robust Abacus-style base legalizer.

### 6. Density-aware Detailed Placement with Instant Legalization

Type: paper metadata / algorithm reference

URL: https://scholar.nycu.edu.tw/en/publications/density-aware-detailed-placement-with-instant-legalization/

Why it matters: The assignment's DOR metric makes density-aware post-legalization relevant. This paper focuses on detailed placement that reduces HPWL and peak bin utilization under displacement constraints using lazy density profit and density-driven swaps.

Implementation ideas for this repo:

- Add a density profit function for local moves/swaps.
- Evaluate move candidates by whether they reduce overflow bins without increasing displacement too much.
- Use after displacement is already acceptable; current `mgc_matrix_mult_a` score is still displacement-dominated.

## Practical Direction For Our Legalizer

The best near-term route is not to copy an assignment solution, but to implement known legalization ideas:

1. Implement true Abacus row clusters for lower displacement.
2. Add row search pruning by vertical displacement lower bound.
3. Add high-displacement local repair using window insertion or nearest-site diamond search.
4. Add density-aware swaps only after displacement improves.
5. Keep `flow.tcl` using our standalone `Legalizer`, not OpenROAD `detailed_placement`, to respect assignment rules.

