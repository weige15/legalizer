# Detailed Design

## Purpose

This document turns `doc/proposal.md` into an implementation-level design for Programming Assignment #3, "Placement with OpenROAD." The final program is a Linux C++17 executable named `Legalizer` that reads an OpenROAD-extracted `.gp` placement file, legalizes all movable `CELL` instances, and writes an OpenROAD TCL script containing direct `place_cell` commands.

The design is legality-first. Every output cell must be inside the die, aligned to the site grid, non-overlapping with other movable cells, and non-overlapping with fixed `MACRO` or `BLOCKAGE` rectangles. After legality is achieved, candidate choices and post-processing should reduce the assignment quality metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of non-macro 10 micron by 10 micron grids whose density exceeds the command-line `threshold`.

## Source Proposal Summary

The proposal defines a density-aware Abacus legalizer with these explicit module boundaries:

- CLI and Configuration
- Placement Model
- GP Parser
- Row Segment Builder
- Abacus Row Engine
- Multi-Row Placement Layer
- Density Estimator
- Hybrid Legalizer
- Legality Checker
- TCL Writer
- Tests and Bench Harness

The proposal assumes DBU integer geometry, `R0` output orientation, no generated `detailed_placement` call, deterministic output, and support for detecting cells taller than one site row. The common case is expected to be single-row cells legalized by Abacus-style row optimization. Multi-row cells use a geometric row-span fallback because rail phase and orientation compatibility are not available in the assignment `.gp` format.

The assignment PDF confirms the required command line:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

and the output line format:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

The generated TCL must not call `detailed_placement`. Each benchmark must finish within 30 minutes.

## Design Goals

- Build a deterministic C++17 implementation using `make`.
- Parse the exact `.gp` metadata and instance table emitted by the provided extraction flow.
- Use DBU integers for all legality-sensitive geometry.
- Preserve movable cell input order for output.
- Construct legal row segments by subtracting fixed `MACRO` and `BLOCKAGE` rectangles.
- Use Abacus row optimization to reduce displacement while preserving row order.
- Include density-aware scoring and a bounded smoothing pass for low-`alpha` or strict-threshold runs.
- Validate final placements before writing TCL.
- Keep each module independently testable with small synthetic fixtures.

## Non-Goals

- The output script will not invoke OpenROAD `detailed_placement`.
- The legalizer will not rotate or flip cells; every command emits `-orient R0`.
- The implementation will not use timing, netlist, routing, or pin information because the assignment input only exposes geometry.
- The implementation will not enforce power-rail compatibility for multi-row cells because the `.gp` input does not contain rail phase or legal orientation data.
- The design does not require exact OpenROAD heatmap reproduction during optimization; OpenROAD `flow.tcl` remains the final scoring reference.

## Architecture Overview

The program runs as a pipeline:

```text
main
  -> CLI and Configuration
  -> GP Parser
  -> Placement Model
  -> Row Segment Builder
  -> Hybrid Legalizer
       -> Abacus Row Engine
       -> Multi-Row Placement Layer
       -> Density Estimator
  -> Legality Checker
  -> TCL Writer
```

The parser owns input interpretation and produces DBU-based model records. The row segment builder derives legal placement capacity from die rows after fixed obstacle subtraction. The hybrid legalizer tries deterministic placement orders, evaluates candidates with displacement and density penalties, commits the best legal candidate, then optionally runs a bounded density smoothing pass. The legality checker is the final gate before the writer emits one TCL command per movable cell.

Recommended source layout, matching the existing `Makefile` intent:

```text
src/main.cpp
src/placement_model.{h,cpp}
src/gp_parser.{h,cpp}
src/row_interval_builder.{h,cpp}
src/density_estimator.{h,cpp}
src/legalizer.{h,cpp}
src/tcl_writer.{h,cpp}
tests/test_legalizer.cpp
tests/fixture_*.gp
```

## Module Designs

### CLI and Configuration

#### Responsibility

Own command-line validation and runtime configuration. It does not parse placement records, legalize cells, or write TCL.

#### Inputs and Outputs

Inputs:

- `argv[1]`: `alpha`, parsed as finite `double`.
- `argv[2]`: `threshold`, parsed as finite `double`.
- `argv[3]`: input `.gp` path.
- `argv[4]`: output `.tcl` path.

Outputs:

- `Config { double alpha; double threshold; std::string input_path; std::string output_path; }`
- Non-zero process exit with a short diagnostic on invalid arguments.

#### Internal Design

`main.cpp` should parse arguments, call the pipeline in order, and catch top-level exceptions. It should reject missing arguments, non-numeric scalar values, unreadable input, and unwritable output paths. The assignment does not define valid numeric ranges for `alpha` and `threshold`; the implementation should accept finite numeric values and optionally warn when `alpha` is outside `[0, 1]`.

#### Dependencies

- Standard library only.
- Calls `parseGpFile`, `buildRowSegments`, `runLegalizer`, `checkLegality`, and `writeTcl`.

#### Failure Handling

Return non-zero and print a concise message to `stderr`. Do not emit partial TCL when parsing or legalization fails.

#### Independent Test Plan

- Unit-test valid four-argument parsing.
- Unit-test missing arguments.
- Unit-test non-numeric `alpha` and `threshold`.
- Unit-test that invalid configuration prevents output creation.

#### Open Questions

- The assignment does not state whether `alpha` is guaranteed in `[0, 1]`; this design treats out-of-range values as accepted but undesirable unless implementation testing shows the grader expects rejection.

### Placement Model

#### Responsibility

Own in-memory geometry and placement data in DBU. It does not read files, choose legal positions, or emit TCL.

#### Inputs and Outputs

Inputs:

- Parsed metadata: DBU per micron, die lower-left, die upper-right, site width, site height.
- Parsed instance records.

Outputs:

- `Design` model used by all later modules.
- Mutable final legal coordinates for movable cells.

Core data contracts:

```text
Rect = [lx, ux) x [ly, uy)
Instance = name, original_lx, original_ly, width, height, type
Cell = movable Instance with legal_lx, legal_ly after legalization
Obstacle = fixed MACRO or BLOCKAGE Rect
```

#### Internal Design

Use signed 64-bit integers for DBU coordinates and areas where possible. Represent rectangles as half-open intervals to make overlap checks deterministic:

```text
overlap(a, b) iff a.lx < b.ux && b.lx < a.ux && a.ly < b.uy && b.ly < a.uy
```

Store movable cells separately from fixed obstacles while preserving the input index of every movable cell for deterministic TCL output. Derived helpers should include width, height, area, center, Manhattan displacement, and rectangle construction from legal coordinates.

#### Dependencies

- Geometry helpers only.

#### Failure Handling

Reject invalid model metadata, including non-positive DBU per micron, site width, site height, or die dimensions. Reject records with non-positive width or height.

#### Independent Test Plan

- Construct rectangles and verify half-open overlap behavior at touching edges.
- Verify DBU area calculations do not use floating point.
- Verify input order is preserved for movable cells.
- Verify fixed `MACRO` and `BLOCKAGE` objects become obstacles.

#### Open Questions

- None.

### GP Parser

#### Responsibility

Read the assignment `.gp` file and populate the Placement Model. It does not legalize, snap coordinates, or infer missing fields.

#### Inputs and Outputs

Inputs:

- Input file path.

Outputs:

- `Design` with metadata, movable cells, fixed obstacles, and parse diagnostics.

Expected input fields:

```text
DBU_Per_Micron <int>
DieArea_LL <x> <y>
DieArea_UR <x> <y>
Site_Width <int>
Site_Height <int>

Name LLX LLY Width Height Type
<name> <llx> <lly> <width> <height> <CELL|MACRO|BLOCKAGE>
```

#### Internal Design

Read line by line. Ignore blank lines around the metadata/table boundary. Require the header names from the assignment before parsing instance records. Classify `CELL` as movable and `MACRO` or `BLOCKAGE` as fixed. Preserve names exactly as tokens because output uses `-inst_name <instName>`.

All coordinates are DBU integers. The parser should clip nothing; clipping fixed obstacles to the die is the Row Segment Builder's responsibility.

#### Dependencies

- Placement Model.

#### Failure Handling

Report line numbers for malformed metadata, missing header, unknown type, short records, non-integer geometry, and invalid dimensions. Return failure rather than silently skipping records.

#### Independent Test Plan

- Parse the sample format from the assignment.
- Parse files with and without the blank line before the header.
- Reject unknown instance types.
- Reject missing metadata.
- Verify a fixture with cells, macros, and blockages is classified correctly.

#### Open Questions

- The assignment says the blank line is mandatory, but the sample text after PDF extraction does not visually preserve it. The parser should tolerate missing blank lines while still requiring the header.

### Row Segment Builder

#### Responsibility

Create legal row Y positions and free X intervals after subtracting fixed obstacles. It does not assign movable cells to rows.

#### Inputs and Outputs

Inputs:

- Die rectangle.
- Site width and site height.
- Fixed obstacles.

Outputs:

- `std::vector<Row>`, where each row has `y`, `row_index`, and site-aligned free `Segment` intervals.

Segment contract:

```text
Segment.x_min <= legal_origin_x
legal_origin_x + cell_width <= Segment.x_max
legal_origin_x is aligned to Site_Width from DieArea_LL.x
row.y is aligned to Site_Height from DieArea_LL.y
```

#### Internal Design

Derive rows with:

```text
row_y = die.ly + row_index * Site_Height
```

for rows whose vertical span `[row_y, row_y + Site_Height)` is inside the die. Start each row with `[die.lx, die.ux)`. For every obstacle whose vertical span intersects the row, subtract the obstacle's X projection clipped to the die. After subtraction, snap each remaining segment inward:

```text
snapped_lx = ceil_to_site(segment_lx, die.lx, site_width)
snapped_ux = floor_to_site(segment_ux, die.lx, site_width)
```

For placement of a cell with width `w`, the maximum origin is `segment.x_max - w`, snapped down to the site grid.

#### Dependencies

- Placement Model geometry helpers.

#### Failure Handling

Rows with no free segments are kept with an empty segment list so row indices remain stable. Segments narrower than one site or unable to fit any cell are dropped. Obstacles outside the die are clipped before subtraction.

#### Independent Test Plan

- Build rows for a small die with no obstacles.
- Subtract one macro from the middle of a row and verify two segments.
- Subtract overlapping obstacles and verify merged blocked intervals.
- Verify inward site snapping at non-site-aligned obstacle edges.
- Verify obstacles touching a row boundary do not block the adjacent half-open row.

#### Open Questions

- None.

### Abacus Row Engine

#### Responsibility

Optimize placement of an ordered set of single-row cells inside one row segment while preserving row order. It does not choose the target row or score density.

#### Inputs and Outputs

Inputs:

- A row segment.
- Ordered cells currently assigned to that segment.
- Optional candidate cell insertion at a specific order position.

Outputs:

- Trial placement coordinates and displacement cost, or infeasible.
- Committed row state when called by the Hybrid Legalizer.

#### Internal Design

Maintain a row sequence sorted by original X, with deterministic tie-break by input index. Use Abacus-style clusters:

1. Scan cells from left to right.
2. Create a cluster with total width and weighted target X.
3. If the new cluster overlaps the previous cluster, merge them.
4. Compute cluster X as the weighted average target clamped to the segment.
5. Recursively merge until clusters no longer overlap.
6. Expand clusters into legal site-aligned cell origins.

For site alignment, clamp and round cluster or cell origins to the nearest legal site without violating segment bounds. If exact Abacus weighted averages produce fractional DBU, retain integer arithmetic until final rounding.

#### Dependencies

- Placement Model.
- Row Segment Builder segment definitions.

#### Failure Handling

Return infeasible when total cell width exceeds segment capacity or when site snapping cannot fit the sequence. Trial calls must not mutate committed row state.

#### Independent Test Plan

- Place a non-overlapping sequence and verify origins stay near original X.
- Force two cells to overlap and verify cluster merging.
- Force clamping against the left and right segment bounds.
- Verify insertion of one candidate leaves the original committed row unchanged during trial.
- Verify deterministic tie-breaking for equal original X.

#### Open Questions

- The exact weighting strategy is not specified by the proposal. Default to equal cell weights unless public benchmark tuning justifies area-based weights.

### Multi-Row Placement Layer

#### Responsibility

Place movable cells whose height exceeds one site row using geometric consecutive-row feasibility. It does not enforce rail compatibility or orientation changes.

#### Inputs and Outputs

Inputs:

- Tall movable cell.
- Row segment lists for all rows.
- Current row occupancy.
- Search radius or local region bounds from the Hybrid Legalizer.

Outputs:

- Legal lower-left coordinate for the tall cell, or infeasible.
- Any required local shifts of overlapping neighbors inside the affected region.

#### Internal Design

Compute:

```text
row_span = ceil(cell.height / Site_Height)
```

Enumerate consecutive row spans near the cell's original Y first. For each row span, intersect the free X intervals of the covered rows to find common legal X intervals wide enough for the cell. Enumerate insertion points near the original X. When existing cells in the span would overlap the candidate, use a local left/right realization strategy: shift affected single-row neighbors within the local region using row-engine trials and reject candidates that cannot be realized without overlap.

The proposal references the multi-row legalization paper for local regions, insertion intervals, insertion-point evaluation, and left/right realization. Because the assignment input lacks rail phase and legal orientation metadata, this module treats feasibility as pure geometry over consecutive site rows.

#### Dependencies

- Placement Model.
- Row Segment Builder.
- Abacus Row Engine for local single-row repacking.

#### Failure Handling

If local insertion fails, expand the local search region. If expanded local insertion fails, perform a full row-span segment search. If no feasible row span exists, report legalization failure to the Hybrid Legalizer.

#### Independent Test Plan

- Detect a one-row cell and confirm the module is not invoked.
- Place a two-row cell in two obstacle-free consecutive rows.
- Reject a two-row cell when common X interval is too narrow.
- Reject row spans blocked in only one covered row.
- Verify fallback search expands beyond the initial row neighborhood.

#### Open Questions

- Public and hidden `.gp` files may contain no movable multi-row cells. The implementation should still detect and log tall-cell counts so the fallback path can be validated when present.

### Density Estimator

#### Responsibility

Estimate 10 micron by 10 micron density occupancy, overflow, and candidate density impact. It does not decide legality by itself.

#### Inputs and Outputs

Inputs:

- DBU per micron.
- Die rectangle.
- Fixed macro and blockage rectangles.
- Current movable placements.
- Candidate insertions or moves.
- Threshold from CLI.

Outputs:

- Estimated DOR.
- Candidate overflow delta.
- Lists of high-overflow grids and contributing cells for smoothing.

#### Internal Design

Use:

```text
grid_size_dbu = 10 * DBU_Per_Micron
```

Maintain movable occupied area per grid using rectangle-grid intersection area. For DOR accounting, exclude grids covered by fixed macros where practical. The assignment specifically excludes regions occupied by fixed macros from the grid count; blockages should be treated as fixed obstacles for legality, but macro exclusion should follow the assignment wording.

Candidate scoring should compute only touched-grid deltas where possible:

```text
overflow(grid) = movable_area(grid) / usable_area(grid) > threshold
```

The implementation must define whether `threshold` is a percentage such as `45` or a ratio such as `0.45` based on observed `flow.tcl` behavior before final tuning.

#### Dependencies

- Placement Model geometry.

#### Failure Handling

Handle designs smaller than one full grid by creating at least the intersecting partial grid cells. Avoid division by zero for grids fully excluded by macros.

#### Independent Test Plan

- Verify grid dimensions for a small die and `DBU_Per_Micron = 1000`.
- Add one cell and verify touched-grid area.
- Move a cell and verify decrement/increment deltas.
- Exclude a macro-covered grid from DOR count.
- Verify overflow classification at, below, and above threshold.

#### Open Questions

- The proposal notes `flow.tcl` uses a `norm_factor` for displacement normalization. Candidate scoring needs public-flow calibration before treating a normalization constant as final.
- The assignment text says DOR excludes fixed macros, not necessarily blockages. The estimator should document and test whichever exclusion policy is chosen after checking `flow.tcl`.

### Hybrid Legalizer

#### Responsibility

Own the full legalization process: trial ordering, candidate enumeration, candidate scoring, committing placements, selecting the best trial, and running density smoothing.

#### Inputs and Outputs

Inputs:

- `Design`.
- Row segments.
- `Config` with `alpha` and `threshold`.

Outputs:

- Final legal coordinates for every movable cell.
- Diagnostics: number of placed cells, tall cells, failed candidates, estimated displacement, estimated DOR.

#### Internal Design

Run deterministic trials. Suggested initial trial orders:

- increasing original X, then original Y, then input index;
- decreasing original X, then original Y, then input index;
- large-area or tall cells first, then increasing original X.

For each cell:

1. Enumerate candidate rows or row spans in increasing vertical displacement from original Y.
2. For each feasible segment, determine the order position by original X.
3. Ask the Abacus Row Engine or Multi-Row Placement Layer for a trial placement.
4. Score the candidate:

   ```text
   score =
     alpha * normalized_displacement_delta
     + (1 - alpha) * estimated_DOR_delta
     + deterministic_tie_breakers
   ```

5. Commit the lowest-score feasible candidate.
6. Update row occupancy and density state.

After all cells are legal in a trial, compute estimated total score and keep the best completed trial. Then run a bounded density smoothing pass:

1. Identify worst overflow grids.
2. Select movable cells contributing to those grids.
3. Try nearby lower-density candidate rows/segments.
4. Accept only moves that preserve legality and improve estimated assignment-like score.
5. Stop at a fixed iteration limit, no-improvement limit, or time budget.

#### Dependencies

- Placement Model.
- Row Segment Builder.
- Abacus Row Engine.
- Multi-Row Placement Layer.
- Density Estimator.

#### Failure Handling

If a cell cannot be placed in its local search, broaden to all rows or all feasible row spans before failing. If a deterministic trial fails but another trial succeeds, continue. If all trials fail, report a clear legalization failure and do not write TCL.

#### Independent Test Plan

- Legalize a one-cell fixture.
- Legalize several overlapping cells in one row.
- Legalize cells around one macro-induced gap.
- Verify all trials produce deterministic results.
- Verify density smoothing never accepts an illegal move.
- Verify a too-full design reports failure rather than overlapping cells.

#### Open Questions

- Exact trial set and smoothing budget are tuning choices. The initial implementation should expose them as constants and validate against the public benchmarks.

### Legality Checker

#### Responsibility

Validate final placements before TCL output. It does not repair placements.

#### Inputs and Outputs

Inputs:

- Final `Design`.
- Row segment data.

Outputs:

- Pass/fail.
- Diagnostics for the first or all violations.

Checks:

- Every movable cell has a legal coordinate.
- Cell rectangle is inside the die.
- Cell lower-left X is site-aligned.
- Cell lower-left Y is a legal row Y.
- Cell does not overlap fixed obstacles.
- Movable cells do not overlap each other.

#### Internal Design

Use interval or sweep-line checks for movable overlap to avoid quadratic behavior on large benchmarks. For each row, sort rectangles by X and check adjacent overlap. For fixed obstacles, use row segment membership where possible and direct rectangle overlap checks for diagnostics.

#### Dependencies

- Placement Model.
- Row Segment Builder.

#### Failure Handling

Return failure with actionable diagnostics. The top-level pipeline should stop before writing output.

#### Independent Test Plan

- Pass a legal one-cell placement.
- Fail off-row Y.
- Fail off-site X.
- Fail movable-movable overlap.
- Fail movable-fixed overlap.
- Fail out-of-die placement.

#### Open Questions

- None.

### TCL Writer

#### Responsibility

Emit the final OpenROAD TCL placement script. It does not legalize or validate.

#### Inputs and Outputs

Inputs:

- Final legal placements for all movable cells.
- `DBU_Per_Micron`.
- Output path.

Outputs:

- A TCL file with one `place_cell` command per movable cell.

#### Internal Design

Write movable cells in their original input order:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

Convert DBU to microns:

```text
micron = dbu / DBU_Per_Micron
```

Use enough decimal precision to represent non-integer micron coordinates without excessive noise. Do not emit comments or any command containing `detailed_placement`.

#### Dependencies

- Placement Model.

#### Failure Handling

Fail if any movable cell lacks legal coordinates or if the output file cannot be opened. Write to the target path only after legality has passed.

#### Independent Test Plan

- Verify one output line per movable cell.
- Verify `-orient R0` appears on every line.
- Verify no line contains `detailed_placement`.
- Verify DBU-to-micron conversion for integer and fractional micron origins.
- Verify output order follows input order.

#### Open Questions

- None.

### Tests and Bench Harness

#### Responsibility

Provide unit and integration tests for module contracts and assignment-facing behavior.

#### Inputs and Outputs

Inputs:

- Synthetic `.gp` fixtures.
- Public benchmark LEF/DEF data and generated `.gp` files.

Outputs:

- `make test` result.
- Optional benchmark logs from `flow.tcl`.

#### Internal Design

Use small deterministic fixtures for unit tests and public benchmarks for end-to-end validation. The current `Makefile` already names `tests/test_legalizer` and a one-cell fixture, so implementation should align with that target or update the Makefile consistently.

Suggested independent test groups:

- Parser fixtures.
- Geometry and row segment fixtures.
- Abacus row fixtures.
- Density grid fixtures.
- TCL writer fixtures.
- End-to-end legalizer fixtures.

Benchmark validation should generate `.gp` files through the provided OpenROAD flow, run `./Legalizer`, then use `flow.tcl` to check legality, average displacement, DOR, and final quality for at least one displacement-heavy and one density-heavy parameter set.

#### Dependencies

- All implementation modules.
- OpenROAD only for benchmark validation, not for unit tests.

#### Failure Handling

Unit failures should identify the module and fixture. Benchmark failures should retain generated TCL and logs for debugging.

#### Independent Test Plan

This module is itself the test harness. It should be runnable with:

```sh
make test
```

Bench validation can be documented separately because it depends on OpenROAD availability.

#### Open Questions

- The repository currently has a `Makefile` that references `src/` and `tests/`, but those directories are not present in the visible working tree. Implementation should create them or reconcile the Makefile before relying on `make test`.

## Cross-Module Contracts

- DBU coordinates are signed integers across parser, model, row construction, legality, and writer.
- Rectangles are half-open: `[lx, ux) x [ly, uy)`.
- `CELL` records are movable. `MACRO` and `BLOCKAGE` records are fixed obstacles for legality.
- Movable output order is the movable input order.
- The Row Segment Builder owns obstacle subtraction and site snapping.
- The Abacus Row Engine must support trial evaluation without mutating committed state.
- The Hybrid Legalizer is the only module that commits final cell locations.
- The Density Estimator is advisory for scoring; legality never depends on density alone.
- The Legality Checker must pass before the TCL Writer runs.
- The TCL Writer emits only `place_cell` commands with `-orient R0`.

## Test Strategy

Run local build and unit tests:

```sh
make
make test
```

Run assignment-style executable tests:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

Run public benchmark validation through the provided OpenROAD flow once source implementation exists:

```sh
./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl
```

Required validation outcomes:

- `check_placement -verbose` passes in OpenROAD.
- Every movable cell is inside the die.
- Every movable cell is aligned to row Y and site X.
- Movable cells do not overlap one another.
- Movable cells do not overlap macros or blockages.
- Output contains one `place_cell` command per movable `CELL`.
- Output contains no `detailed_placement`.
- Runtime stays below 30 minutes per benchmark.
- Public runs record average displacement, DOR, and quality for both displacement-oriented and density-oriented parameter settings.

## Risks and Mitigations

- Risk: Density optimization can harm displacement enough to worsen quality.
  Mitigation: Scale candidate scoring with public-flow calibration and keep the best deterministic trial.

- Risk: Rows fragmented by macros and blockages may make greedy placement fail.
  Mitigation: Use Abacus row repacking and broaden candidate search before declaring failure.

- Risk: Multi-row behavior is under-specified by the assignment input.
  Mitigation: Treat multi-row support as geometric row-span legality, log tall-cell counts, and keep all orientations `R0`.

- Risk: Exact DOR estimation may differ from OpenROAD heatmap scoring.
  Mitigation: Use estimator for relative candidate choices and rely on `flow.tcl` for final scoring.

- Risk: Runtime can grow with candidate count and smoothing.
  Mitigation: Search nearest rows first, prune by displacement lower bound, use delta density updates, and bound smoothing by time or iteration count.

- Risk: The visible repository has planning docs and a Makefile but no source tree.
  Mitigation: Treat the source layout as a reconstruction task and keep tests aligned with the Makefile target.

## Open Questions

- Are hidden `.gp` files guaranteed to use `alpha` in `[0, 1]` and `threshold` as a percentage rather than a ratio? The implementation should match `flow.tcl` behavior during tuning.
- Should DOR estimator exclusion ignore only `MACRO` grids, exactly as the assignment states, or also exclude `BLOCKAGE` grids because blockages are fixed placement obstacles? The final scorer should follow `flow.tcl`.
- What smoothing iteration/time budget gives the best public quality without risking the 30 minute timeout?
- Do hidden benchmarks contain movable cells taller than `Site_Height`? The implementation should support and log this case, but public validation may not exercise it.
- Should the Abacus Row Engine use equal weights or area-based weights for cluster target positions? Equal weights are the initial design default unless public benchmark tuning shows a clear benefit.
