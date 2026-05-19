# Proposal: ABACUS-Inspired Placement Legalizer for OpenROAD Assignment

## Objective
Build a Linux C++17 command-line legalizer for Programming Assignment #3, "Placement with OpenROAD." The program will read the extracted global-placement `.gp` input, place all movable `CELL` instances legally inside the die, avoid `MACRO` and `BLOCKAGE` regions, align cells to site rows and site columns, and write an OpenROAD TCL file containing one `place_cell` command per movable cell.

The algorithm should minimize the assignment quality metric:

```text
Quality = alpha * Average_Displacement + (1 - alpha) * DOR
```

where `DOR` is the percentage of 10um by 10um grids whose non-macro cell density exceeds the input threshold.

## Current Project State
The repository currently contains the assignment PDF, ABACUS paper, an additional multi-row legalization paper, OpenROAD helper scripts, public LEF/DEF benchmarks, `README.md`, `Makefile`, and an existing `Legalizer` binary.

Observed files include:

- `p3_placement.pdf`: assignment specification.
- `abacus.pdf`: "Abacus: Fast Legalization of Standard Cell Circuits with Minimal Movement."
- `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf`: reference for multi-row/local legalization ideas.
- `README.md`: describes a C++17 legalizer layout with `src/`, `tests/`, and `doc/`.
- `Makefile`: expects source files under `src/` and tests under `tests/`.
- `public/`: public benchmark LEF/DEF examples.

One mismatch should be resolved during implementation: `README.md` and `Makefile` reference `src/` and `tests/`, but those directories were not present in the visible file tree during proposal inspection.

## Assumptions
The implementation will target the assignment interface exactly:

```sh
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

The `.gp` input contains `DBU_Per_Micron`, die bounds, site dimensions, and instance records with `CELL`, `MACRO`, or `BLOCKAGE` type. `CELL` instances are movable. `MACRO` and `BLOCKAGE` instances are fixed obstacles.

All generated cell origins must be snapped to site coordinates. Output coordinates should be converted from DBU to microns for OpenROAD TCL, matching the assignment sample.

The base implementation will focus on the assignment's standard single-row-height case. If hidden tests include multi-height cells, the legalizer should at least detect them and either handle them conservatively as height-multiple row occupants or fail with a clear internal diagnostic during development. The output submitted to the grader should never emit illegal placements.

## Proposed Approach
Use an ABACUS-inspired legalization pipeline with a density-aware row assignment cost. The core idea is to process cells in global x-order, try legal insertion into nearby row intervals, use ABACUS cluster placement to minimize row displacement, and choose the row that gives the best combined displacement and density-overflow cost.

### 1. Input Model and Coordinate Normalization
Parse the `.gp` file into a compact placement model:

- Technology data: `dbu_per_micron`, die lower-left and upper-right, site width, and site height.
- Movable cells: name, original lower-left coordinate, width, height.
- Fixed obstacles: macros and blockages with lower-left coordinate, width, and height.

Normalize all coordinates to integer DBU internally. For row and column legality, derive site-index coordinates:

- Row index from `(y - die_lly) / site_height`.
- Site index from `(x - die_llx) / site_width`.

Keep original DBU coordinates for exact displacement reporting and TCL output conversion.

### 2. Row Interval Construction
Construct legal placement intervals for every site row:

1. Start each row with one interval spanning the die x-range.
2. For every macro and blockage overlapping that row, subtract the obstacle's x-span from the row.
3. Snap interval boundaries outward/inward to site columns so any cell placed in the interval remains site-aligned and inside the unblocked area.
4. Drop intervals whose width is less than one site.

This transforms macro/blockage avoidance into a set of independent row segments, matching the ABACUS paper's assumption that rows blocked by macros are sliced into subrows.

### 3. Cell Ordering
Sort movable cells by original x-coordinate, breaking ties by y-coordinate and name. Run the legalizer in increasing x-order and optionally also in decreasing x-order. The ABACUS paper notes that both directions can yield slightly different solutions; keeping the better result is low risk and usually cheap enough for assignment-scale benchmarks.

### 4. ABACUS Row Trial
For each cell, choose candidate rows around the cell's original y-coordinate:

1. Start from the nearest legal row.
2. Expand upward and downward.
3. Stop exploring a direction when the vertical displacement alone already exceeds the best trial cost found so far by a margin.

Within each candidate row interval, insert the cell by original x-order and run `PlaceRow` in trial mode.

`PlaceRow` will follow the ABACUS cluster dynamic program:

- Maintain ordered clusters of cells in a row interval.
- When a new cell overlaps the previous cluster, merge clusters.
- For each cluster, compute the optimal x-coordinate using weighted quadratic displacement:

```text
cluster_x = q / e
```

- Clamp each cluster to the row interval.
- Recursively merge with predecessor clusters if clamping or placement creates overlap.
- Expand clusters back into individual cell x-coordinates.

The trial must be reversible. A row candidate should be evaluated without permanently modifying row state until it is selected as the best row.

### 5. Density-Aware Cost
Pure ABACUS minimizes movement, but the assignment also grades density overflow ratio. Use the command-line parameters to influence row selection:

```text
trial_cost = alpha * normalized_displacement_delta
           + (1 - alpha) * estimated_density_penalty
```

The displacement term should include the target cell movement and, when practical, the movement change induced in already placed cells in the candidate row.

The density penalty should estimate whether placing the cell into the candidate interval worsens 10um by 10um grid overflow:

- Convert 10um to DBU using `dbu_per_micron`.
- Maintain per-grid movable-cell area, excluding grids covered by macros from the denominator as required by the assignment.
- During a trial, estimate the added or changed occupied area for affected grids.
- Penalize grids whose density exceeds `threshold`.

For speed, the first version can use a row-bin approximation during trial and compute exact DOR after full legalization. If runtime allows, refine the trial estimator to update exact grid bins for cells affected by `PlaceRow`.

### 6. Overflow Repair and Detailed Improvement
After the first legalization pass:

1. Verify legality: die containment, site alignment, row alignment, no cell-cell overlap, and no overlap with macros/blockages.
2. Compute average displacement and DOR.
3. If DOR is high, identify overflow grids or row intervals with high utilization.
4. Locally re-place selected cells into nearby underused intervals using the same reversible ABACUS row trial.

Avoid forbidden final-output behavior: the generated TCL must not call OpenROAD `detailed_placement`.

### 7. Output Writer
Write one command per movable cell:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {<x_micron> <y_micron>}
```

Coordinates should be printed in microns with enough precision to represent DBU-derived positions. Cell rotation is forbidden by the assignment, so orientation remains `R0`.

## Milestones
1. Reconcile repository structure by adding or restoring `src/` and `tests/` expected by the existing `Makefile`.
2. Implement the parser, placement data model, and TCL writer.
3. Implement row interval construction from die area, macros, and blockages.
4. Implement ABACUS `PlaceRow` and row trial/final insertion.
5. Add density grid estimation and integrate `alpha`/`threshold` into trial scoring.
6. Add legality, displacement, and DOR checkers for validation.
7. Add focused unit tests and public benchmark smoke tests.
8. Tune candidate-row search, sort direction, and density repair for runtime under the 30-minute limit.

## Open Questions
None for the proposal. During implementation, the main item to confirm is whether hidden benchmarks contain multi-row-height movable cells; if so, the row interval and overlap model should be extended to place a cell across multiple consecutive rows.

## Validation Plan
Validation should cover both correctness and assignment quality:

1. Build with `make` on Linux.
2. Run unit tests for parsing, coordinate snapping, row interval subtraction, cluster placement, TCL output formatting, and legality checks.
3. Run public benchmark smoke tests through the required interface:

```sh
./Legalizer <alpha> <threshold> <design>_insts.gp <design>_insts.tcl
```

4. Verify every generated placement satisfies:
   - cell origin is site-aligned,
   - cell origin is row-aligned,
   - cell is inside die,
   - cells do not overlap each other,
   - cells do not overlap macros or blockages,
   - output contains only `place_cell` commands and no forbidden `detailed_placement` call.
5. Compute average displacement and DOR for both assignment-style parameter settings and compare variants:
   - increasing x-order ABACUS,
   - decreasing x-order ABACUS,
   - density-aware trial scoring,
   - optional overflow repair.
6. Check runtime on public cases and preserve margin below the 30-minute-per-case limit.
