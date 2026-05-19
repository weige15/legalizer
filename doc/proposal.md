# Proposal: Density-Aware Abacus Legalizer for OpenROAD Placement

## Objective
Build the algorithm and implementation plan for Programming Assignment #3, "Placement with OpenROAD." The final program should read the `.gp` file emitted by the provided OpenROAD extraction flow, legalize all movable `CELL` instances onto legal site rows, avoid fixed `MACRO` and `BLOCKAGE` rectangles, and write an OpenROAD TCL script containing direct `place_cell` commands.

The algorithm should prioritize legality first, then optimize the assignment quality metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of non-macro 10 micron by 10 micron grids whose density exceeds the command-line `threshold`.

## Current Project State
The repository currently contains the assignment PDF, OpenROAD helper scripts, public benchmark data, a `README.md`, a `Makefile`, and an existing compiled `Legalizer` binary.

Observed files and constraints:

- `p3_placement.pdf` defines the required input format, output format, command line, legality rules, grading metric, and 30 minute timeout.
- `abacus.pdf` describes Abacus, a fast row-based legalizer that sorts cells by X position and uses dynamic programming to optimally place cells within a row while preserving row order.
- `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf` describes local legalization for multi-row-height cells using local regions, insertion intervals, insertion-point evaluation, and left/right realization.
- `flow.tcl` extracts `.gp`, records original cell positions, can run OpenROAD detailed placement for debugging, checks legality, dumps a 10 micron heatmap, and computes displacement, DOR, and final quality.
- `extract.tcl` writes records with fields `Name LLX LLY Width Height Type`, classifying OpenROAD block masters as `MACRO` and other instances as `CELL`, then appending `BLOCKAGE` entries.
- `public/` contains the public LEF/DEF benchmarks.
- The working tree currently does not contain the source and test directories described by the README, so the proposal treats implementation as a C++17 reconstruction around the assignment interface.

## Assumptions
The project should be implemented as a Linux C++17 program because the assignment prefers C/C++ and the TA command is:

```sh
make
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

All internal geometry should use DBU integers. Output TCL origins should be converted to microns using `DBU_Per_Micron`, matching the assignment examples.

The generated TCL must not call `detailed_placement`; it should only emit `place_cell` commands for movable cells.

All emitted placements should use `-orient R0` because the assignment forbids cell rotation.

Most movable cells are expected to be one site row high, but the parser and legalizer should detect `height > Site_Height`. Supporting multi-row-height cells is useful because the second paper directly addresses that case and the `.gp` format includes per-cell heights.

Power-rail compatibility from the multi-row paper is not enforceable from the assignment `.gp` format because no rail phase or legal orientation metadata is provided. Multi-row support should therefore focus on geometric row-span legality while keeping `R0`.

## Proposed Approach
Use a hybrid legalization algorithm:

1. Preprocess fixed obstacles into legal row segments.
2. Use Abacus-style row optimization as the primary legalization engine for single-row cells.
3. Extend placement checks to multi-row cells through consecutive-row segment intersection and local insertion.
4. Add an assignment-specific density estimator and smoothing pass to reduce DOR under the given `threshold`.

### Data Model and Parsing
Parse the required `.gp` metadata:

- `DBU_Per_Micron`
- `DieArea_LL`
- `DieArea_UR`
- `Site_Width`
- `Site_Height`
- instance rows with `Name LLX LLY Width Height Type`

Classify `CELL` as movable and `MACRO` or `BLOCKAGE` as fixed obstacles. Store rectangles as half-open DBU ranges. Preserve input order for deterministic output.

### Row and Segment Construction
Create legal site rows from the die area:

```text
row_y = die_y_min + row_index * Site_Height
```

For each row, begin with the full die X range, subtract every fixed obstacle whose vertical span intersects that row, clip obstacles to the die, then snap remaining segment boundaries inward to legal site coordinates.

For multi-row cells, derive candidate row spans of `ceil(cell_height / Site_Height)` consecutive rows. A row span is feasible only where the covered rows share enough common X capacity after obstacle subtraction.

### Core Legalization Algorithm
Run a density-aware Abacus variant with multiple deterministic trials.

For each trial:

1. Choose a cell order, such as increasing X, decreasing X, and optionally area-first for large or multi-row cells.
2. For each cell, enumerate candidate row or row-span positions near its original Y first.
3. For each candidate, insert the cell according to original X order and evaluate the affected row placement.
4. Use Abacus `PlaceRow` dynamic programming for single-row affected rows:
   - preserve row order,
   - form clusters when cells overlap,
   - compute each cluster's optimal X from weighted original positions,
   - clamp clusters to the legal segment,
   - collapse overlapping clusters recursively.
5. Score each trial insertion using displacement plus estimated density impact.
6. Commit the best candidate and update row occupancy and density grids.

The Abacus paper is the right core for this assignment because it minimizes movement while allowing already legalized cells in the same row to shift. This is better than a pure Tetris-style greedy slot fill, which tends to trap later cells and increase displacement.

### Multi-Row Cell Handling
When a movable cell is taller than one site row, use the multi-row paper as a fallback layer:

1. Extract a local region around the target position across the required consecutive rows.
2. Treat each uninterrupted free span as a segment.
3. Enumerate insertion gaps across the row span that share a common legal X interval.
4. Reject insertion points that would cross the left/right side of an existing multi-row cell inconsistently.
5. Choose the insertion point with lowest displacement and density penalty.
6. Realize the placement by pushing overlapping neighbors left and right inside the local region.

If local multi-row insertion fails, expand the search region. If it still fails, perform a full row-span segment search before reporting failure. This keeps the common single-row case fast while giving a principled path for taller cells.

### Density-Aware Scoring
Maintain an approximate 10 micron by 10 micron density grid:

```text
grid_size_dbu = 10 * DBU_Per_Micron
```

Track movable occupied area per grid and mark grids fully covered by fixed macros as excluded when practical. For each candidate placement, estimate the new density of touched grids and penalize overflow above `threshold`.

Use a scoring function aligned with the assignment:

```text
score =
  alpha * normalized_displacement_delta
  + (1 - alpha) * estimated_DOR_delta
  + legality_or_boundary_penalty
```

`flow.tcl` normalizes average displacement with a `norm_factor`, so implementation tuning should use a similar scale to keep displacement and density penalties comparable.

### Density Smoothing Pass
After the first legal placement, run a bounded improvement pass:

1. Identify overflow grids from the estimator.
2. Collect movable cells contributing to the worst overflow grids.
3. Try relocating selected cells to nearby lower-density row segments using the same Abacus trial placement.
4. Accept a move only if it preserves legality and improves the assignment-like score.
5. Stop after a time budget, no-improvement limit, or when estimated DOR is below threshold.

This pass directly targets the assignment's DOR term, which the original Abacus paper does not optimize.

### Output
Write one line per movable cell:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

`X` and `Y` should be lower-left coordinates in microns. The output must not contain `detailed_placement`.

## Milestones
1. Reconstruct a buildable C++17 project with `Makefile`, parser, model, row segment builder, legalizer, density estimator, and TCL writer.
2. Implement strict `.gp` parsing, DBU geometry helpers, site snapping, obstacle subtraction, and deterministic TCL output.
3. Implement baseline Abacus-style single-row legalization with cluster collapse and candidate row search.
4. Add density-aware candidate scoring and estimated 10 micron grid occupancy.
5. Add multi-row cell detection and local insertion fallback for cells taller than one site row.
6. Add post-legalization density smoothing to reduce DOR for low-`alpha` configurations.
7. Validate on public benchmarks through `flow.tcl` with both displacement-heavy and density-heavy parameter settings.

## Open Questions
None for the proposal. The implementation should still log and test whether public or hidden `.gp` files contain movable cells taller than `Site_Height`, because that determines how often the multi-row fallback is exercised.

## Validation Plan
Build with:

```sh
make
```

Run with the exact assignment interface:

```sh
./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl
```

Validate through OpenROAD `flow.tcl` on both public benchmarks and at least two parameter sets:

- displacement-oriented: high `alpha`,
- density-oriented: low `alpha` or stricter `threshold`.

Required correctness checks:

- `check_placement -verbose` passes,
- every movable cell is inside the die,
- every movable cell is aligned to a legal site row and site X,
- movable cells do not overlap each other,
- movable cells do not overlap fixed macros or blockages,
- output contains one `place_cell` command per movable cell,
- all output orientations are `R0`,
- output does not contain `detailed_placement`,
- runtime stays below 30 minutes per benchmark.

Quality checks:

- compare average displacement against a simple greedy/Tetris baseline,
- compare estimated DOR against OpenROAD heatmap DOR from `flow.tcl`,
- track final `Quality = alpha * Average Displacement + (1 - alpha) * DOR`,
- keep the best result among deterministic trial orders when runtime allows.
