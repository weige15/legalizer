# Detailed Design

## Purpose

This document turns `doc/proposal.md` and the assignment handout `p3_placement.pdf` into an implementation-ready design for a standalone C++17 placement legalizer. The executable must be built by `make` and invoked as:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

The program reads an OpenROAD-extracted `.gp` file, legalizes all movable `CELL` instances onto legal sites and rows, avoids fixed `MACRO` and `BLOCKAGE` rectangles, validates the result, and writes only explicit OpenROAD `place_cell` commands.

## Source Proposal Summary

The proposal defines an Abacus-based, density-aware legalizer with the following major responsibilities:

- Parse and normalize the `.gp` placement model in integer DBU.
- Build legal row intervals by subtracting macro and blockage rectangles from every placement row.
- Use Abacus-style row assignment and cluster collapse as the primary legalization engine.
- Adapt candidate scoring to `alpha` and `threshold` by combining displacement and DOR pressure.
- Run bounded local repair for high-displacement cells and density-overflow bins.
- Run multiple deterministic pass variants and keep the best validated result.
- Validate all legality constraints and recompute exact metrics before writing output TCL.

The existing `Makefile` expects these production modules:

- `placement_model`
- `gp_parser`
- `row_interval_builder`
- `density_estimator`
- `legalizer`
- `tcl_writer`
- `main`

Unit tests are expected under `tests/`.

## Design Goals

- Produce legal placements for public and hidden benchmarks within 30 minutes per benchmark.
- Minimize the assignment quality function while matching the local `flow.tcl` scorer:

  ```text
  flow_quality = alpha * (average_displacement_um * 18.2) + (1 - alpha) * DOR
  ```

- Also report the handout's raw average-displacement form for debugging:

  ```text
  handout_quality = alpha * average_displacement_um + (1 - alpha) * DOR
  ```

- Keep all geometry in integer DBU until serializing TCL coordinates.
- Generate output with one `place_cell -inst_name ... -orient R0 -origin {...}` command per movable cell.
- Never emit `detailed_placement`.
- Keep modules independently testable with small synthetic fixtures.

## Non-Goals

- Do not perform timing, routing, pin-access, or wirelength optimization except indirectly through displacement and density.
- Do not rotate cells; every output command uses `R0`.
- Do not depend on libraries beyond the C++17 standard library.
- Do not invoke OpenROAD from inside the `Legalizer` executable.
- Do not implement a full mixed-height detailed placer unless the unresolved multi-row hidden-case risk is explicitly promoted to required scope.

## Architecture Overview

The executable is a deterministic pipeline:

```text
main
  -> gp_parser
  -> placement_model
  -> row_interval_builder
  -> density_estimator
  -> legalizer
       -> Abacus row trial solver
       -> density-aware scoring
       -> local repair
       -> deterministic pass selection
  -> final validator
  -> tcl_writer
```

`PlacementModel` owns immutable input geometry and mutable placement results. Modules exchange typed DBU geometry objects rather than raw strings or micron coordinates. The writer is reachable only after validation succeeds.

## Module Designs

### CLI / Configuration

#### Responsibility

Own command-line validation, run configuration, end-to-end orchestration, diagnostics, and process exit status. It does not parse `.gp` records, solve placement, or write individual placement commands.

#### Inputs and Outputs

Inputs:

- `argc` and `argv`.
- Required arguments: `alpha`, `threshold`, `input.gp`, `output.tcl`.

Outputs:

- `RunConfig { double alpha; double threshold; string input_path; string output_path; }`.
- Exit code `0` on success and nonzero on invalid arguments, input failure, unsupported cases, legalization failure, validation failure, or write failure.

#### Internal Design

- Require exactly four user arguments.
- Parse `alpha` as finite `double`; accept the assignment's expected range `[0, 1]`.
- Parse `threshold` as finite `double`; interpret it as a percentage density threshold in `[0, 100]`.
- Instantiate the pipeline in a narrow `run(config)` function so integration tests can exercise the same path.
- Print concise diagnostics to `stderr`, including the failing module and reason.

#### Dependencies

- `gp_parser`
- `row_interval_builder`
- `legalizer`
- final validation helpers from `placement_model` / `density_estimator`
- `tcl_writer`

#### Failure Handling

- Invalid argument count or parse failure exits before reading files.
- Any exception or explicit error result becomes a nonzero process exit.
- The output path is not touched unless a validated placement exists.

#### Independent Test Plan

- Run `./Legalizer` with missing arguments and malformed numeric values; expect nonzero exit.
- Run `./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl`; expect success.
- Verify the generated TCL is nonempty and contains no `detailed_placement`.

#### Open Questions

- None.

### Placement Model

#### Responsibility

Own canonical geometry, input instances, row/site metadata, placement state, and shared metric structs. It does not choose legal positions or parse files.

#### Inputs and Outputs

Inputs from parser:

- `DBU_Per_Micron`
- die lower-left and upper-right coordinates
- site width and site height
- instance records: `Name LLX LLY Width Height Type`

Core data contracts:

```text
Point { int64 x, y }
Rect { int64 x0, y0, x1, y1 }        // half-open [x0, x1) x [y0, y1)
Instance {
  string name
  Rect original
  InstanceType type                 // CELL, MACRO, BLOCKAGE
  optional<Point> placed_ll
}
RowInterval { int row_index; int64 y; int64 x0; int64 x1 }
Metrics {
  double avg_displacement_um
  double normalized_displacement
  double dor_percent
  double flow_quality
  double handout_quality
}
```

Outputs:

- Vectors of movable cells and fixed obstacles.
- Legal row coordinate helpers.
- Placement lookup by cell id/name for validation and writing.

#### Internal Design

- Store all dimensions and coordinates as signed 64-bit DBU integers.
- Treat rectangles as half-open ranges to make overlap checks exact.
- Preserve each movable cell's original lower-left coordinate for displacement.
- Create stable integer cell ids so legalization can sort and move cells without invalidating references.
- Provide geometry utilities:
  - rectangle overlap and overlap area;
  - site-alignment checks;
  - snap up/down to site columns;
  - row-index conversion from `die_ll.y`.
- Track whether a movable cell is single-row height: `cell.height == site_height`.

#### Dependencies

- Standard library only.

#### Failure Handling

- Reject nonpositive DBU, site width, site height, die width, or die height.
- Reject zero or negative instance dimensions.
- Reject unknown instance type strings.
- Flag multi-row movable cells as unsupported unless later implementation scope changes.

#### Independent Test Plan

- Construct synthetic models directly in unit tests without parsing text.
- Test half-open overlap behavior at touching edges and true intersections.
- Test row/site snapping for coordinates below, inside, and above the die.
- Test movable/fixed partitioning.
- Test multi-row detection.

#### Open Questions

- Hidden benchmarks may include multi-row movable cells. The current design treats them as unsupported with a clear diagnostic.

### GP Parser

#### Responsibility

Strictly parse assignment `.gp` files into `PlacementModel` seed data. It does not repair malformed records or infer omitted fields.

#### Inputs and Outputs

Input:

- Text file path.

Expected header:

```text
DBU_Per_Micron <int>
DieArea_LL <x> <y>
DieArea_UR <x> <y>
Site_Width <int>
Site_Height <int>

Name LLX LLY Width Height Type
```

Outputs:

- Fully populated `PlacementModel` on success.
- Parse diagnostic containing line number and reason on failure.

#### Internal Design

- Read line by line with `std::getline`.
- Trim whitespace and ignore only the one optional blank line before the column header.
- Parse required metadata in exact order.
- Accept the exact assignment column header.
- Parse every instance as six whitespace-separated fields.
- Convert instance width/height into `Rect {llx, lly, llx + width, lly + height}`.
- Preserve instance order for deterministic tie-breaks.

#### Dependencies

- `placement_model` types.

#### Failure Handling

- Reject missing metadata, missing header, extra or missing instance fields, nonnumeric geometry, overflow during coordinate arithmetic, unknown type, and empty input.
- Report the line number for record-level failures.

#### Independent Test Plan

- Parse a valid one-cell fixture.
- Parse fixtures with macros and blockages.
- Reject missing blank/header sections, unknown type, negative width, and truncated instance lines.
- Verify DBU and coordinates are stored exactly.

#### Open Questions

- None.

### Row Interval Builder

#### Responsibility

Build the legal placement capacity for every site row by subtracting fixed macros and blockages from the die. It does not place cells.

#### Inputs and Outputs

Inputs:

- Die bounds.
- Site width and site height.
- Fixed obstacle rectangles from `MACRO` and `BLOCKAGE`.
- Minimum movable cell width.

Outputs:

- `vector<RowInterval>` grouped by row index.
- Optional capacity summary per row and total legal area.

#### Internal Design

- Generate rows from `die_ll.y` to `die_ur.y - site_height` in `site_height` steps.
- Start each row with one interval `[die_ll.x, die_ur.x)`.
- For each obstacle intersecting the row's vertical span, subtract its horizontal overlap from that row's intervals.
- Snap each interval start upward to the next legal site column.
- Snap each interval end downward to a legal site boundary.
- Discard intervals whose width is less than the minimum movable cell width.
- Keep intervals sorted by `(row_index, x0, x1)`.

#### Dependencies

- `placement_model` geometry helpers.

#### Failure Handling

- If no intervals remain, return an insufficient-capacity diagnostic.
- If total interval capacity is smaller than total movable area, return a likely-impossible diagnostic before legalization.

#### Independent Test Plan

- One empty row produces one full-width interval.
- A centered macro splits a row into left and right intervals.
- A blockage touching a row edge clips only the overlapping span.
- Obstacles outside a row do not affect it.
- Snapping removes partial-site fragments.
- Narrow intervals are discarded.

#### Open Questions

- The assignment treats fixed macros as excluded from DOR grid count; blockages are legal obstacles but are not explicitly stated as excluded DOR grids. This design excludes only macros from DOR grid count to match `flow.tcl`.

### Density Estimator

#### Responsibility

Maintain density-grid state for candidate scoring and compute exact DOR for final validation. It does not decide legal placement by itself.

#### Inputs and Outputs

Inputs:

- Die bounds and DBU.
- Threshold percentage.
- Macro rectangles.
- Movable cell placements.

Outputs:

- Grid dimensions using 10um by 10um bins.
- Estimated candidate density penalty.
- Exact `DOR = overflow_grids / total_non_macro_grids * 100`.

#### Internal Design

- Convert grid size to DBU: `grid_size = 10 * dbu_per_micron`.
- Cover the die with ceil-divided bins, clipping edge bins to die bounds.
- Exclude a bin from DOR count if it has any overlap with a `MACRO`, matching the local `flow.tcl` fallback density writer.
- Do not exclude blockage-covered bins unless flow behavior changes.
- For exact recomputation:
  - zero all non-excluded bins;
  - add overlap area from every movable cell;
  - compute density as `movable_area / bin_area * 100`;
  - count bins where density is strictly greater than `threshold`.
- For candidate scoring:
  - inspect bins overlapped by the trial placement;
  - estimate added overflow pressure from post-placement density;
  - avoid full-grid recomputation inside every row trial.

#### Dependencies

- `placement_model` geometry helpers.

#### Failure Handling

- If `dbu_per_micron` is invalid, density construction fails.
- If no non-macro bins remain, report DOR as `0` and include a warning diagnostic.

#### Independent Test Plan

- Build a 20um by 10um synthetic die and verify two 10um bins.
- Verify macro-overlapped bins are excluded.
- Verify density threshold comparison is strict `density > threshold`.
- Verify exact DOR on one partially filled and one overflowing bin.
- Verify candidate penalty increases for already-over-threshold bins.

#### Open Questions

- Hidden evaluation may use OpenROAD's native heatmap instead of the provided fallback routine. The design follows the repository's `flow.tcl` behavior because it is the executable local scorer.

### Abacus Row Solver

#### Responsibility

Given one legal row interval and an ordered set of cells assigned to that interval, compute legal X positions using Abacus cluster collapse. It does not search rows, compute DOR, or mutate global placement until the caller accepts the trial.

#### Inputs and Outputs

Inputs:

- `RowInterval { y, x0, x1 }`.
- Existing cells assigned to the interval.
- Optional candidate cell to insert.
- Original X positions and widths.

Outputs:

- Trial ordered placement for that interval.
- Trial displacement delta.
- Failure if total cell width exceeds interval width.

#### Internal Design

- Sort interval cells by target/original X with stable id tie-breaks.
- Insert the candidate into that order.
- Build clusters in order:
  - a new cluster stores total width, weighted target sum, and first/last cells;
  - the cluster ideal X is the weighted average of target positions;
  - clamp cluster X to `[interval.x0, interval.x1 - cluster.width]`;
  - when a new cluster overlaps the previous one, merge and recompute;
  - repeat merging until no cluster overlap remains.
- Expand final clusters into cell lower-left X positions by cumulative width.
- Keep Y fixed to the interval row coordinate.

#### Dependencies

- `placement_model` geometry helpers.

#### Failure Handling

- Reject intervals whose total assigned width exceeds available width.
- Reject cells wider than the interval.
- Return trial failure rather than partially placed rows.

#### Independent Test Plan

- One cell snaps/clamps into the interval.
- Two overlapping cells merge into one cluster.
- A cluster clamps to the left interval boundary.
- A cluster clamps to the right interval boundary.
- An over-capacity interval fails.
- Stable tie-breaks produce repeatable positions.

#### Open Questions

- None.

### Legalization Engine

#### Responsibility

Assign every movable cell to a legal row interval, evaluate candidate placements, run deterministic variants, perform bounded repair, and return the best validated placement candidate. It owns optimization policy but not parsing or final file writing.

#### Inputs and Outputs

Inputs:

- `PlacementModel`.
- Legal row intervals.
- `alpha` and `threshold`.
- Density estimator.

Outputs:

- Complete placement map for all movable cells.
- Best exact metrics among successful variants.

#### Internal Design

- Precompute candidate intervals by row distance from each cell's original Y.
- For each deterministic variant:
  - clear placement state;
  - choose an ordering:
    - left-to-right by original X;
    - right-to-left by original X;
    - density-first for low `alpha`;
    - large-cell-first tie-breaks in dense regions;
  - insert cells one at a time into candidate intervals;
  - use the Abacus row solver to evaluate each candidate interval trial;
  - score trial placements using the candidate scoring contract below;
  - commit the lowest-cost feasible trial.
- Candidate search pruning:
  - vertical displacement alone is a lower bound;
  - after a feasible candidate is found, skip farther rows whose vertical displacement already exceeds the current best score envelope;
  - keep a hard maximum row search window configurable in code, then expand if no candidate is found.
- Local repair:
  - identify top displacement outliers and cells in overflow bins;
  - remove one cell from its current interval;
  - try reinsertion into nearby intervals;
  - optionally try pairwise swaps within a small neighborhood;
  - recompute exact metrics for accepted moves;
  - commit only if flow quality improves, or if DOR improves with bounded displacement increase in low-`alpha` mode;
  - stop when no improvement is found or the time budget is reached.

#### Dependencies

- `placement_model`
- `row_interval_builder`
- `density_estimator`
- `Abacus Row Solver`
- validator helpers

#### Failure Handling

- If any cell cannot fit in any interval, return the cell name and reason.
- If a deterministic variant fails, continue with other variants.
- If all variants fail, exit with a legalization-failed diagnostic.
- Repair is optional; failure during repair falls back to the best pre-repair legal placement.

#### Independent Test Plan

- Legalize a single-cell fixture.
- Legalize two overlapping cells on one row.
- Legalize cells around one macro-split row.
- Verify left-to-right and right-to-left variants both produce legal outputs.
- Verify low-`alpha` ordering prefers reducing overflow in a synthetic dense case.
- Verify local repair never worsens exact flow quality when configured for strict-improvement mode.

#### Open Questions

- The exact number of deterministic variants and repair budget should be tuned after public benchmark measurements. The design requires deterministic, bounded behavior but does not prescribe final constants.

### Candidate Scoring

#### Responsibility

Convert a trial insertion into a comparable scalar cost that reflects displacement and density priorities for the current run. This module can be implemented as helper functions inside `legalizer` or as a small separate policy type.

#### Inputs and Outputs

Inputs:

- Trial placements changed by an Abacus row insertion.
- Original positions of affected cells.
- Current and estimated density-grid state.
- `alpha` and `threshold`.

Output:

- `double candidate_cost`, lower is better.

#### Internal Design

Use the flow-compatible direction:

```text
candidate_cost =
  alpha * normalized_displacement_delta
  + (1 - alpha) * estimated_dor_delta
  + local_penalties
```

Where:

- `normalized_displacement_delta` is based on micron displacement multiplied by `18.2`.
- `estimated_dor_delta` is based on bins touched by affected cells.
- `local_penalties` are deterministic tie-breakers for:
  - moving into already-over-threshold bins;
  - placing near interval edges when alternatives are otherwise equal;
  - large movement away from original Y.

High-`alpha` runs keep density as a tie-breaker or mild penalty. Low-`alpha` runs weight overflow-bin pressure more aggressively.

#### Dependencies

- `density_estimator`
- `placement_model`

#### Failure Handling

- If density estimates cannot be computed, fall back to displacement-only candidate scoring and rely on exact validation for final metrics.

#### Independent Test Plan

- Same displacement but higher overflow pressure yields higher cost.
- Same density but lower displacement yields lower cost.
- `alpha = 1.0` ignores DOR except deterministic tie-breaks.
- `alpha = 0.0` prioritizes estimated DOR.

#### Open Questions

- Final penalty constants require benchmark tuning. They should be kept in named constants, not scattered literals.

### Final Validator

#### Responsibility

Gate output generation by checking legality and recomputing exact metrics. It does not move cells.

#### Inputs and Outputs

Inputs:

- Final placement map.
- Placement model.
- Legal row intervals.
- Density estimator settings.
- `alpha`, `threshold`.

Outputs:

- Pass/fail.
- Diagnostics for every violated constraint category.
- Exact metrics.

#### Internal Design

Legality checks:

- Every movable cell has a placement.
- Every placed rectangle is inside the die.
- X is aligned to the site grid.
- Y is aligned to a legal row.
- Cell height equals one site row for supported scope.
- Every cell rectangle lies within one legal row interval.
- No two movable cell rectangles overlap.
- No movable cell overlaps any macro or blockage.

Metric checks:

- Average Manhattan displacement in microns.
- Normalized displacement using `norm_factor = 18.2`.
- Exact DOR over 10um bins with macro-overlapped bins excluded.
- Flow-compatible and handout-form quality values.

Implementation notes:

- Use row/interval grouping to check cell-cell overlap efficiently by sorting cells in each row by X.
- Use interval membership to avoid repeated obstacle checks for normal cases, but still run direct obstacle overlap checks as a defensive validation.

#### Dependencies

- `placement_model`
- `density_estimator`

#### Failure Handling

- Return all easily collected violations rather than stopping at the first one.
- Refuse to write output if any legality check fails.

#### Independent Test Plan

- Detect unplaced cells.
- Detect off-site X and off-row Y.
- Detect cell-cell overlap.
- Detect cell-macro and cell-blockage overlap.
- Detect out-of-die placement.
- Verify exact metrics on small fixtures.

#### Open Questions

- None.

### TCL Writer

#### Responsibility

Serialize a validated placement into the assignment's OpenROAD TCL command format. It does not validate legality beyond basic preconditions.

#### Inputs and Outputs

Inputs:

- Validated movable-cell placement map.
- DBU per micron.
- Requested output path.

Output:

- TCL file containing only `place_cell` commands.

#### Internal Design

- Write to a temporary sibling path first.
- Emit one line per movable cell in deterministic input order:

  ```tcl
  place_cell -inst_name <name> -orient R0 -origin {<x_micron> <y_micron>}
  ```

- Convert DBU to microns as decimal values using `dbu / DBU_Per_Micron`.
- Use enough precision to round-trip site coordinates cleanly in OpenROAD.
- Scan generated text before rename to ensure it does not contain `detailed_placement`.
- Rename the temporary file to the requested output path only after the write succeeds.

#### Dependencies

- `placement_model`

#### Failure Handling

- Fail if the output stream cannot be opened, written, flushed, or renamed.
- Fail if any cell lacks a placement.
- Leave any existing output file untouched on failure before final rename.

#### Independent Test Plan

- Write a one-cell placement and compare exact command shape.
- Verify micron conversion for integer and fractional micron coordinates.
- Verify no `detailed_placement` appears.
- Verify deterministic ordering.

#### Open Questions

- None.

### Tests

#### Responsibility

Provide fast, independently runnable checks for parser, geometry, intervals, density, Abacus placement, validation, writer, and executable smoke behavior.

#### Inputs and Outputs

Inputs:

- Synthetic `.gp` fixtures under `tests/`.
- Public benchmark inputs for optional end-to-end flow checks.

Outputs:

- `tests/test_legalizer` process status.
- Generated smoke-test TCL files.

#### Internal Design

- Use a lightweight C++ test executable with `assert` or a tiny local check macro.
- Keep fixtures small enough to inspect manually.
- Separate unit tests from OpenROAD-dependent flow tests.
- Make `make test` build and run unit tests plus one executable smoke test.

#### Dependencies

- All production modules.
- No external test framework.

#### Failure Handling

- Any failed assertion or nonzero smoke run fails `make test`.
- Tests should not require OpenROAD.

#### Independent Test Plan

- `make test`
- Optional manual flow:

  ```sh
  openroad flow.tcl
  CASE_NAME=public/ispd15_mgc_matrix_mult_a openroad flow.tcl
  ```

#### Open Questions

- OpenROAD may not be installed in every development environment, so OpenROAD flow tests remain manual unless the environment is confirmed.

## Cross-Module Contracts

- Geometry is DBU integer internally; only `tcl_writer` converts to microns.
- Rectangles are half-open ranges.
- `CELL` means movable; `MACRO` and `BLOCKAGE` mean fixed obstacles.
- Legal row intervals are the only legal horizontal capacity for movable cells.
- The legalizer must not place a cell outside row intervals and must not rely on the writer to fix placements.
- The final validator is mandatory before writing.
- DOR uses 10um by 10um bins and excludes bins touched by macros, matching `flow.tcl`.
- DOR overflow comparison is strict: `density > threshold`.
- Local benchmark selection should use `avg_displacement_um * 18.2` to match `flow.tcl`, while diagnostics should also print raw average displacement.

## Test Strategy

Run tests in layers:

1. Unit tests for pure geometry and parser behavior.
2. Unit tests for row interval construction around synthetic obstacles.
3. Unit tests for exact density and DOR on tiny designs.
4. Unit tests for Abacus row solving and over-capacity failure.
5. Integration tests for full legalization on small `.gp` fixtures.
6. `make test` smoke run for the required CLI.
7. Manual OpenROAD flow runs on:

   ```sh
   openroad flow.tcl
   CASE_NAME=public/ispd15_mgc_matrix_mult_a openroad flow.tcl
   ```

For public benchmarking, run at least one displacement-focused setting and one density-focused setting, for example high `ALPHA` and low `ALPHA` runs with the assignment threshold.

## Risks and Mitigations

- Hidden multi-row movable cells may fail if explicit rejection is kept. Mitigation: keep the diagnostic clear and isolate the height check so a later multi-row module can be added without rewriting parser or writer.
- Abacus insertion can be slow if every cell tries every row. Mitigation: row-distance pruning, interval capacity checks, and bounded expansion.
- Density scoring can become expensive. Mitigation: use local estimates during insertion and exact DOR only after full passes or accepted repair moves.
- Flow scoring differs from the handout's simple quality equation. Mitigation: optimize/select using the repository `flow.tcl` normalized displacement and report both metrics.
- Output precision can cause OpenROAD site drift if rounded too coarsely. Mitigation: emit sufficient decimal precision from exact DBU values.
- Local repair can damage a good Abacus baseline. Mitigation: accept only exact quality improvements or narrowly defined DOR improvements in low-`alpha` mode.

## Open Questions

1. Should hidden multi-row movable cells remain an explicit unsupported case, or should multi-row legalization be added to required scope?
2. If TA evaluation differs from this repository's `flow.tcl`, should final optimization target the handout's raw average displacement or the local normalized displacement? This design selects the local normalized scorer because it is the executable evaluator in the repository.
3. What final constants should be used for candidate row window size, density penalties, deterministic pass count, and repair time budget? They should be tuned empirically on public cases after the baseline legalizer is implemented.
