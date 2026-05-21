# Proposal: Obstacle-Aware Density-Driven Placement Legalizer

## Objective
Build a standalone C++ legalizer for Programming Assignment #3, "Placement with OpenROAD." The program will read an OpenROAD-extracted `.gp` placement file, legalize all movable standard cells onto site rows while avoiding fixed macros and blockages, and write an OpenROAD TCL script containing only explicit `place_cell` commands.

The implementation should optimize the assignment quality metric:

```text
Quality = alpha * AverageDisplacement + (1 - alpha) * DOR
```

where DOR is the percentage of 10 um by 10 um non-macro grids whose cell density exceeds the provided threshold.

## Current Project State
The workspace contains the assignment PDF `p3_placement_v2.pdf`, the OpenROAD helper flow files `extract_v2.tcl` and `flow.tcl`, public benchmark data under `public/`, and literature notes under `literature/`.

The assignment requires:

- executable name and interface: `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>`;
- C or C++ on Linux, with C++ preferred by the existing `Makefile`;
- legal placements inside the die area, aligned to placement sites and rows;
- no overlap between movable cells, macros, or blockages;
- no cell rotation;
- no `detailed_placement` command in the emitted TCL.

The existing `README.md` and `Makefile` describe a C++17 module layout under `src/` and tests under `tests/`, but those directories were not present during inspection. This proposal therefore treats the implementation as a greenfield C++17 project compatible with the existing `Makefile` shape.

## Assumptions
- Movable `CELL` instances are primarily single-row standard cells, matching the assignment examples. Multi-row movable cells can be detected and rejected with a clear diagnostic unless later tests show they must be supported.
- Fixed `MACRO` and `BLOCKAGE` instances are modeled as rectangular obstacles that split placement rows into independent legal intervals.
- Coordinates are parsed in database units, while output origins are emitted in microns for OpenROAD TCL compatibility.
- The legalizer must be deterministic so repeated grading runs produce stable output.
- The first implementation priority is always legality; quality improvements are applied only after a valid placement is available.

## Proposed Approach
Use a three-phase legalizer: construct legal row intervals, generate a legal placement with Abacus-style row optimization, then refine density and displacement with local legal moves.

### 1. Input Model And Geometry
Parse the `.gp` input into:

- global technology data: DBU per micron, die bounds, site width, and site height;
- movable cells with original lower-left coordinates, dimensions, orientation, and name;
- fixed rectangles for macros and blockages.

Normalize all internal coordinates to database units. Snap candidate cell origins to legal site columns and row y-coordinates. Keep original coordinates for displacement evaluation.

### 2. Obstacle-Aware Row Interval Builder
Create one row for each legal site row in the die. For every fixed macro or blockage, project its x-range onto every row whose y-range it overlaps. Split those rows into legal intervals between obstacle projections.

Each interval stores:

- row y-coordinate;
- legal x-min and x-max;
- site-aligned capacity;
- the ordered cells assigned to the interval;
- cached occupancy for fast feasibility checks.

This follows the obstacle-aware Tetris/Abacus direction from the literature notes and keeps collision handling in the row model instead of scattering obstacle checks throughout placement.

### 3. Baseline Legalization With Abacus Clusters
Use Abacus as the main legalizer because the assignment directly rewards low average displacement.

Process cells sorted by original x-coordinate, with a second optional pass in reverse x-order if time permits. For each cell:

1. Search candidate intervals in rows near the cell's original y-coordinate.
2. Skip intervals whose remaining capacity cannot fit the cell.
3. Tentatively insert the cell into the interval's x-order.
4. Run an Abacus `PlaceRow` cluster solver for that interval:
   - create a cluster for a non-overlapping cell;
   - merge clusters when tentative placement overlaps;
   - compute cluster x-position from original x targets and widths;
   - clamp clusters to interval bounds and site grid;
   - expand clusters back into non-overlapping cell coordinates.
5. Score the tentative insertion by total displacement delta, with vertical displacement as an early pruning lower bound.
6. Commit the lowest-cost legal insertion.

If no Abacus candidate is found for a cell, fall back to a Tetris-style nearest feasible interval search that places the cell in the nearest available gap. This preserves legality even when a local row optimizer cannot find a good insertion quickly.

### 4. Assignment Metric Evaluator
Implement the exact scoring components needed for local decisions:

- average displacement over all movable cells;
- 10 um by 10 um density grid occupancy;
- exclusion of grids covered by fixed macros from the DOR denominator;
- threshold comparison using the command-line `threshold`;
- final weighted quality using command-line `alpha`.

The density grid is maintained incrementally after the initial legal placement so local refinements can be evaluated cheaply.

### 5. DOR-Aware Local Repair
After baseline legalization, run bounded improvement passes while preserving legality. Start from overflow grids because they directly affect DOR when `alpha` is low.

Candidate repairs:

- move a high-contribution cell from an overflow grid to a nearby interval gap;
- swap two nearby cells if both remain legal and quality improves;
- locally reinsert a high-displacement cell into nearby rows and repack affected intervals with the Abacus row solver;
- perform row-local reorder attempts inside a small window.

Each candidate is accepted only if it improves the final assignment quality or improves DOR without a large displacement penalty. The pass should stop after a fixed iteration count or after a full pass with no accepted moves, keeping runtime safely below the 30-minute grading limit.

### 6. Validation And Output
Before writing the output file, run an internal legality checker:

- every movable cell has exactly one placement;
- every cell is inside the die;
- every x-coordinate is site-aligned and every y-coordinate is row-aligned;
- no movable cell overlaps another movable cell;
- no movable cell overlaps a macro or blockage;
- orientations are preserved;
- output text does not contain `detailed_placement`.

Only after validation succeeds, write TCL commands in this form:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

where `X` and `Y` are converted from DBU to microns.

## Milestones
1. Implement parser, geometry model, row interval construction, and TCL writer.
2. Implement internal legality validation and a simple Tetris fallback legalizer.
3. Implement Abacus interval insertion and cluster-based row optimization.
4. Implement exact average displacement and DOR evaluator.
5. Add DOR-aware local move, swap, and reinsertion repair passes.
6. Integrate the required CLI, update the `Makefile`, and validate on both public benchmarks through the OpenROAD flow.

## Open Questions
- Should multi-row movable cells be fully supported, or is rejecting them acceptable for this assignment's benchmark set?
- Should the first version optimize for simpler reliable legality first, or include DOR-aware repair immediately before public benchmark tuning?
- Is the intended output coordinate precision fixed by the TA scripts, or is concise decimal micron output acceptable as in the assignment examples?

## Validation Plan
Validate incrementally:

1. Unit-test parsing with small synthetic `.gp` fixtures, including macros and blockages.
2. Unit-test row interval splitting for obstacles that cover no rows, one row, multiple rows, full rows, and row boundaries.
3. Unit-test Abacus row packing against known small cell sequences and interval bounds.
4. Unit-test the legality checker with deliberate overlap, off-site, off-row, out-of-die, and obstacle-overlap cases.
5. Unit-test DOR calculation on small hand-computable density grids.
6. Smoke-test the required CLI:

```sh
make
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

7. Run `flow.tcl` on the two public benchmarks and confirm OpenROAD accepts the generated `place_cell` script without invoking `detailed_placement`.
8. Compare quality across at least two parameter settings: one displacement-heavy high-`alpha` run and one density-heavy low-`alpha` run.
