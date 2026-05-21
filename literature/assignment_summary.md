# Assignment Summary: `p3_placement_v2.pdf`

Date read: 2026-05-22

## Problem

The assignment is Programming Assignment #3, "Placement with OpenROAD", for Physical Design Automation, Spring 2026. It asks for a standalone C/C++ legalizer invoked as:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

The input is an OpenROAD-extracted placement file containing:

- DBU per micron
- die area lower-left and upper-right
- placement site width and height
- movable `CELL` instances
- fixed `MACRO` and `BLOCKAGE` rectangles

The output must be an OpenROAD TCL script containing explicit `place_cell` commands:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

## Legality Requirements

- Every movable cell must be placed inside the rectangular die area.
- Cells must be aligned to standard-cell site rows.
- Cells must not overlap other cells, fixed macros, or blockages.
- Cell rotation is forbidden; movable cells keep their original orientation.
- The submitted TCL must not directly call `detailed_placement`.

## Scoring Objective

The assignment gives 70 points for passing legality. The remaining performance score is based on quality rankings over four benchmarks and two parameter configurations.

The quality metric is:

```text
Quality = alpha * AverageDisplacement + (1 - alpha) * DOR
```

where DOR is the density overflow ratio over 10 um by 10 um grids excluding grids covered by fixed macros.

## Implications For Literature Search

The most relevant sources are not full global placers. They are legalization and detailed-placement methods that:

- remove overlaps after global placement,
- snap cells to legal rows/sites,
- preserve global-placement locations by minimizing displacement,
- handle fragmented rows caused by macros and blockages,
- optionally improve local density without breaking legality.

The closest algorithm families are Tetris, Abacus, obstacle-aware Abacus/Tetris, OpenDP-style search, and density-aware detailed placement.

