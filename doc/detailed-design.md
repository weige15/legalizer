# Detailed Design

## Purpose

This document turns `doc/proposal.md` into a module-level design for Programming Assignment #3, "Placement with OpenROAD." The implementation target is a Linux C++17 command-line legalizer named `Legalizer` that reads an extracted `.gp` placement file, legalizes all movable `CELL` instances, and writes an OpenROAD TCL script containing one `place_cell` command per movable cell.

The design is constrained by `p3_placement.pdf`: placements must be inside the die, non-overlapping, aligned to site rows, avoid unmovable macros and blockages, keep orientation `R0`, and avoid using OpenROAD `detailed_placement` in the final output TCL.

## Source Proposal Summary

The proposal defines an ABACUS-inspired legalization pipeline:

1. Parse the assignment `.gp` input into integer DBU geometry.
2. Build legal row intervals by subtracting `MACRO` and `BLOCKAGE` rectangles from each site row.
3. Sort movable cells by original x-coordinate, with optional reverse-order legalization as a quality variant.
4. Trial-insert each cell into nearby row intervals using an ABACUS-style row cluster solver.
5. Score candidate placements with both displacement and density-overflow pressure using `alpha` and `threshold`.
6. Verify final legality, compute displacement and DOR metrics, and optionally repair overflow regions.
7. Write assignment-compliant TCL with `place_cell -inst_name ... -orient R0 -origin {X Y}`.

The proposal explicitly names these modules: Input Model and Coordinate Normalization, Row Interval Construction, Cell Ordering, ABACUS Row Trial, Density-Aware Cost, Overflow Repair and Detailed Improvement, and Output Writer. The repository `Makefile` also names concrete source modules expected under `src/`.

## Design Goals

- Match the required executable interface:

```sh
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

- Use integer DBU coordinates for all legality decisions.
- Convert DBU coordinates to microns only when writing TCL and reporting development metrics.
- Produce placements that satisfy die containment, site-column alignment, row alignment, cell-cell non-overlap, and obstacle avoidance.
- Minimize the assignment quality objective:

```text
Quality = alpha * Average_Displacement + (1 - alpha) * DOR
```

- Keep the main legalization path deterministic for easier debugging and repeatable grading.
- Keep every core module independently testable with small fixtures.
- Finish each benchmark within the assignment's 30-minute limit.

## Non-Goals

- Do not emit `detailed_placement` in the final output TCL.
- Do not rotate, flip, or otherwise change cell orientation.
- Do not optimize timing, wirelength, or netlist-aware placement because the input format does not include netlist connectivity.
- Do not require OpenROAD at runtime for the submitted `Legalizer`; OpenROAD helper scripts remain development and evaluation tools.
- Do not silently invent full support for multi-row-height movable cells. The base design targets single-row-height cells and records multi-row support as an open question and risk.

## Architecture Overview

The legalizer is a staged pipeline with shared immutable input geometry and localized mutable placement state:

```text
CLI
  -> GP Parser
  -> Placement Model
  -> Row Interval Builder
  -> Legalization Engine
       -> Cell Ordering
       -> Row Placement / Cluster Solver
       -> Density Estimator
  -> Legality and Metric Checker
  -> Optional Overflow Repair
  -> TCL Writer
```

`PlacementModel` stores the canonical design data. Row intervals are derived from that model. The legalization engine owns committed row state and candidate trials. The density estimator provides lightweight trial penalties and exact final DOR calculations. The checker validates a completed placement before the writer emits TCL.

The concrete source layout should follow the existing `Makefile` contract:

```text
src/main.cpp
src/placement_model.{h,cpp}
src/gp_parser.{h,cpp}
src/row_interval_builder.{h,cpp}
src/density_estimator.{h,cpp}
src/legalizer.{h,cpp}
src/tcl_writer.{h,cpp}
tests/test_legalizer.cpp
```

## Module Designs

### CLI / Configuration

#### Responsibility

Own command-line parsing, argument validation, top-level orchestration, and process exit status. It does not parse `.gp` content, make placement decisions, or write placement commands directly.

#### Inputs and Outputs

Inputs:

- `argc`, `argv`.
- Required positional arguments: `alpha`, `threshold`, `input.gp`, `output.tcl`.

Outputs:

- `Config { double alpha; double threshold; string input_path; string output_path; }`.
- Exit status `0` on successful legal placement and TCL write; nonzero on parse, legalization, validation, or write failure.

#### Internal Design

- Validate exactly four user arguments after the executable name.
- Parse `alpha` and `threshold` as finite numeric values.
- Treat `alpha` as a scoring weight. The assignment does not explicitly state bounds, but the quality formula implies `[0, 1]`; values outside that range should be rejected unless implementation requirements later say otherwise.
- Run the pipeline in this order: parse, build rows, legalize, check, write.
- Print concise diagnostics to `stderr` for development failures.

#### Dependencies

- `GPParser`
- `RowIntervalBuilder`
- `Legalizer`
- `LegalityChecker`
- `TclWriter`

#### Failure Handling

- Missing or malformed arguments: print usage and return nonzero.
- Parser or writer file errors: return nonzero with path context.
- Illegal final placement: return nonzero and do not write a misleading output if validation is performed before writing.

#### Independent Test Plan

- Unit-test argument count and numeric parsing through a small `parseConfig` function.
- Test rejection of `NaN`, infinities, and malformed numeric strings.
- Smoke-test the executable with a tiny fixture through `make test`, matching the existing `Makefile`.

#### Open Questions

- Should `alpha` outside `[0, 1]` be rejected or clamped? The design recommends rejection because the assignment defines it as a weight.

### GP Parser

#### Responsibility

Parse the assignment `.gp` format into typed placement records. It owns lexical and structural input validation only; it does not snap coordinates, legalize cells, or compute density.

#### Inputs and Outputs

Inputs:

- Input file path.
- Text format:

```text
DBU_Per_Micron <int>
DieArea_LL <llx> <lly>
DieArea_UR <urx> <ury>
Site_Width <int>
Site_Height <int>

Name LLX LLY Width Height Type
<instName> <llx> <lly> <width> <height> <CELL|MACRO|BLOCKAGE>
```

Outputs:

- `PlacementModel` containing technology data, die bounds, movable cells, and fixed obstacles.

#### Internal Design

- Read required metadata keys in assignment order.
- Allow the mandatory blank line between metadata and header; tolerate extra surrounding whitespace.
- Require the header fields to match the assignment names.
- Parse each instance row into integer DBU coordinates and dimensions.
- Classify `CELL` as movable and `MACRO` / `BLOCKAGE` as fixed obstacles.
- Preserve input order for deterministic output when needed.

#### Dependencies

- `PlacementModel` data types.
- Standard C++ file and string parsing facilities.

#### Failure Handling

- Report missing metadata, malformed integers, nonpositive site dimensions, nonpositive instance dimensions, unknown instance types, duplicate instance names, and unreadable files.
- Reject files with no site dimensions because row alignment cannot be defined.

#### Independent Test Plan

- Parse a one-cell fixture.
- Parse a fixture with a macro and blockage.
- Reject malformed headers, unknown types, negative dimensions, and duplicate names.
- Verify all parsed coordinates remain integer DBU values with no micron conversion.

#### Open Questions

- None for the assignment format currently described by `p3_placement.pdf` and `extract.tcl`.

### Placement Model

#### Responsibility

Provide canonical in-memory geometry and placement records. It owns coordinate helpers and immutable original placement data. It does not own row construction decisions or legalization algorithms.

#### Inputs and Outputs

Inputs:

- Parsed metadata and instance rows from `GPParser`.

Outputs:

- Shared data structures for all downstream modules.
- Coordinate helper methods such as DBU-to-micron conversion, row-index conversion, and site-index conversion.

Recommended data shapes:

```cpp
struct Rect { long long llx, lly, urx, ury; };
struct Cell {
  std::string name;
  Rect original;
  Rect placed;
  bool has_placement;
};
struct Obstacle {
  std::string name;
  Rect rect;
  ObstacleType type; // Macro or Blockage
};
struct PlacementModel {
  int dbu_per_micron;
  Rect die;
  int site_width;
  int site_height;
  std::vector<Cell> cells;
  std::vector<Obstacle> obstacles;
};
```

#### Internal Design

- Store rectangles as half-open intervals `[llx, urx) x [lly, ury)` for simpler overlap checks.
- Derive `urx = llx + width` and `ury = lly + height` at parse time.
- Use `long long` for accumulated coordinates and areas to avoid integer overflow on large designs.
- Keep original and placed rectangles separate so displacement can always be computed against the extracted global placement.

#### Dependencies

- None beyond standard C++ containers and strings.

#### Failure Handling

- Geometry helper methods should expose boolean validity checks rather than silently snapping invalid coordinates.
- Multi-row-height cells should be detectable by `cell_height != site_height`.

#### Independent Test Plan

- Test rectangle overlap semantics, containment, and area.
- Test row and site coordinate conversions.
- Test DBU-to-micron conversion formatting through writer-facing helpers.
- Test detection of cells whose dimensions do not fit the site grid.

#### Open Questions

- Hidden benchmarks may include multi-row-height movable cells. If yes, model support is straightforward, but row interval occupancy and legality checks must be extended beyond the single-row case.

### Row Interval Builder

#### Responsibility

Convert die rows and fixed obstacles into legal horizontal placement intervals. It owns row slicing and site-grid snapping. It does not decide which cells enter which intervals.

#### Inputs and Outputs

Inputs:

- `PlacementModel` die bounds, site dimensions, and obstacles.

Outputs:

- `std::vector<Row>` where each row has a y-coordinate and legal intervals.
- Each `RowInterval` stores row index, y DBU, snapped x start, snapped x end, capacity in sites or DBU width, and committed cell ids.

#### Internal Design

1. Compute row count from die height and site height. If the die height is not divisible by site height, only full legal rows starting at `die.lly + k * site_height` are usable unless requirements later say otherwise.
2. Initialize every row with one interval spanning `[die.llx, die.urx)`.
3. For each `MACRO` or `BLOCKAGE`, find rows whose y-span overlaps the obstacle y-span.
4. Subtract the obstacle x-span from each overlapped row's current intervals.
5. Snap interval starts upward to the next legal site column and interval ends downward to a legal site boundary.
6. Drop intervals with no legal site or with width less than the minimum cell width when that information is useful for pruning.

#### Dependencies

- `PlacementModel` geometry helpers.

#### Failure Handling

- If no legal intervals exist, report a legalization failure early.
- Preserve empty rows in row indexing so vertical distance remains easy to compute.
- Treat obstacles partially outside the die by clipping their subtraction region to the die rectangle.

#### Independent Test Plan

- Build rows for an empty die and verify full-width intervals.
- Subtract one middle macro and verify two intervals.
- Subtract edge blockages and verify clipped intervals.
- Verify site snapping when obstacle boundaries are not site-aligned.
- Verify obstacles spanning multiple rows affect every overlapping row.

#### Open Questions

- The assignment excludes fixed macro regions from DOR grid counts, but is less explicit about fixed blockage exclusion. Row legality should avoid both; DOR exclusion behavior is recorded under Density Estimator open questions.

### Cell Ordering

#### Responsibility

Provide deterministic movable-cell orderings for the legalization engine. It does not place cells or mutate row state.

#### Inputs and Outputs

Inputs:

- Movable cell list from `PlacementModel`.
- Direction mode: increasing x or decreasing x.

Outputs:

- Ordered vector of cell ids.

#### Internal Design

- Primary key: original `llx`.
- Tie breakers: original `lly`, then name, then original input index if needed.
- Support forward and reverse variants. Reverse mode should reverse the primary x direction while keeping deterministic tie-breaking.

#### Dependencies

- `PlacementModel`.

#### Failure Handling

- Empty cell lists are legal and should produce an empty TCL file.

#### Independent Test Plan

- Verify stable ordering with equal x values.
- Verify reverse ordering is deterministic.
- Verify names provide deterministic tie-breaking.

#### Open Questions

- None.

### Row Placement / Cluster Solver

#### Responsibility

Implement the ABACUS-style `PlaceRow` operation for one row interval. Given an ordered sequence of cells assigned to an interval, compute non-overlapping x positions that minimize movement within that fixed order.

#### Inputs and Outputs

Inputs:

- A row interval with x bounds and y coordinate.
- Current committed cells in that interval.
- Optional candidate cell insertion.
- Cell widths and original x positions.

Outputs:

- Trial result containing legal x positions, changed cell ids, displacement delta, and feasibility.
- Final commit that updates interval cell order and placed cell x/y positions.

#### Internal Design

- Maintain cells in interval order by target x coordinate.
- Insert the candidate cell into the ordered sequence for a trial.
- Build clusters from left to right.
- For each cluster, maintain:
  - first and last cell indices,
  - total width,
  - weighted target sum `q`,
  - total weight `e`,
  - proposed cluster x origin.
- Use unit weights initially unless tuning later introduces cell-specific weights.
- Clamp cluster origin to interval bounds.
- Merge with the predecessor cluster while overlap remains.
- Expand final cluster positions into per-cell x origins.
- Preserve cell order. ABACUS row placement does not swap cells inside an interval after order is chosen.

#### Dependencies

- `PlacementModel` cell dimensions and original positions.
- Row interval state owned by the legalization engine.

#### Failure Handling

- Mark a trial infeasible when total cell width exceeds interval width.
- Mark infeasible if a cell width is larger than the interval width.
- Avoid mutating committed state in trial mode.

#### Independent Test Plan

- Place one cell in one interval.
- Place two non-overlapping cells and verify original x positions are preserved when legal.
- Place overlapping cells and verify cluster spreading removes overlap.
- Place a cluster near the left and right interval boundaries and verify clamping.
- Verify infeasibility when total width exceeds interval capacity.
- Verify trial mode leaves committed row state unchanged.

#### Open Questions

- Whether to use Manhattan displacement delta or squared displacement inside cluster scoring. ABACUS uses quadratic placement, while `flow.tcl` measures Manhattan displacement for reporting. The design can use ABACUS quadratic row placement and Manhattan candidate scoring, but this should be tuned during implementation.

### Legalization Engine

#### Responsibility

Own global legalization decisions: candidate row search, row trial scoring, final insertion commits, direction variants, and selection of the best complete solution.

#### Inputs and Outputs

Inputs:

- `PlacementModel`.
- Row intervals.
- `Config { alpha, threshold }`.
- Density estimator.

Outputs:

- Final placed coordinates for all movable cells.
- Development metrics for selected solution.

#### Internal Design

For each legalization pass:

1. Initialize empty row interval states.
2. Obtain a deterministic cell order.
3. For each cell:
   - Determine the nearest legal row from original y.
   - Search candidate rows upward and downward.
   - For each row, test every interval wide enough for the cell.
   - Run `PlaceRow` in trial mode.
   - Compute trial score from displacement delta, vertical movement, density penalty, and feasibility.
   - Commit the best feasible row interval by rerunning or applying the selected trial.
4. After all cells are placed, validate the full pass and compute metrics.
5. Optionally run a reverse x-order pass and keep the legal solution with lower quality.

Candidate search pruning:

- Stop exploring a vertical direction when the vertical displacement lower bound is already worse than the best candidate by a configured margin.
- Always keep a fallback that searches all rows if no candidate is found under the pruning rule.

Recommended scoring:

```text
trial_score =
  alpha * normalized_movement_delta
  + (1 - alpha) * density_penalty
  + infeasibility_penalty
```

The movement term should include the inserted cell and changed row cells when `PlaceRow` shifts already committed cells.

#### Dependencies

- Cell ordering.
- Row placement / cluster solver.
- Density estimator.
- Legality checker for final pass validation.

#### Failure Handling

- If a cell cannot fit any interval, return a legalization failure and include the cell name and dimensions in diagnostics.
- If a committed pass fails final legality, discard that pass and try another configured variant if available.
- If all variants fail, return nonzero through the CLI.

#### Independent Test Plan

- Legalize one cell in an empty die.
- Legalize two initially overlapping cells in one row.
- Legalize cells around a macro-created interval gap.
- Verify fallback full-row search finds a legal row when pruning would otherwise miss candidates.
- Verify reverse-order pass can be run independently and compared.

#### Open Questions

- The exact normalization factor for displacement in the assignment grader is not specified in the PDF. `flow.tcl` includes `norm_factor 18.2`, but the PDF only defines the symbolic quality formula. The implementation should keep scoring normalization configurable or isolated.

### Density Estimator

#### Responsibility

Estimate density impact during candidate trials and compute final DOR for development validation. It does not decide final placement by itself.

#### Inputs and Outputs

Inputs:

- Die bounds and `DBU_Per_Micron`.
- Fixed macro rectangles, and possibly blockage rectangles depending on confirmed grader behavior.
- Movable cell rectangles for trial or final placement.
- `threshold`.

Outputs:

- Trial density penalty.
- Final DOR:

```text
DOR = overflow_grid_count_excluding_macros / total_grid_count_excluding_macros * 100
```

#### Internal Design

- Grid size is `10 * dbu_per_micron` in both x and y.
- Build a grid over the die area.
- Mark grids excluded from the DOR denominator when they are occupied by fixed macros, matching the assignment statement.
- Maintain movable-cell area contribution per grid for committed placements.
- For exact final DOR, compute overlap area between each movable cell rectangle and each touched grid.
- Convert density to a percentage or fraction consistently with the input `threshold`. The assignment examples use threshold values such as `45`, so the design treats `threshold` as a percentage.
- For trial scoring, start with an approximation based on affected grid bins or row bins. The exact final DOR remains the validation metric.

#### Dependencies

- `PlacementModel`.
- Final cell placements from `LegalizationEngine`.

#### Failure Handling

- If no non-excluded grids exist, report DOR as `0` for metric calculation and emit a diagnostic in development mode.
- Guard against zero-area grids or invalid DBU-per-micron metadata by rejecting the input earlier.

#### Independent Test Plan

- Compute DOR for an empty movable-cell placement.
- Compute one overflow grid with a known cell area and threshold.
- Verify macro-covered grids are excluded from the denominator.
- Verify cells spanning grid boundaries distribute area across all touched grids.
- Verify threshold comparison uses `>` as shown by `flow.tcl`.

#### Open Questions

- Should `BLOCKAGE` regions also be excluded from DOR denominator? The PDF explicitly says fixed macros are excluded; `flow.tcl` removes instances named like macros before dumping density, while blockages are not instances. The design should exclude macros by default and keep blockage exclusion isolated if later evidence confirms it.

### Overflow Repair and Detailed Improvement

#### Responsibility

Improve a complete legal placement when density overflow remains high. It owns local improvement loops after the first legal solution. It does not create illegal intermediate output files and does not call OpenROAD `detailed_placement`.

#### Inputs and Outputs

Inputs:

- Legal placement from `LegalizationEngine`.
- Exact or approximate overflow grid information from `DensityEstimator`.
- Row interval state.

Outputs:

- Modified legal placement with equal or lower quality when successful.

#### Internal Design

- Identify overflow grids after exact DOR computation.
- Collect movable cells contributing to the most overflowed grids.
- For selected cells, temporarily remove the cell from its current interval and trial-insert it into nearby underused intervals.
- Score the move with the same `alpha` and `threshold` logic.
- Commit only moves that preserve legality and improve the estimated quality.
- Limit iterations and candidate radius to protect the 30-minute runtime cap.

#### Dependencies

- Legalization engine row state.
- Row placement / cluster solver.
- Density estimator.
- Legality checker.

#### Failure Handling

- If no improving move is found within the iteration budget, keep the original legal solution.
- If local repair creates a validation failure, discard the repair move and restore the previous state.

#### Independent Test Plan

- Build a fixture with one deliberately dense area and one empty interval; verify a candidate move is discovered.
- Verify non-improving moves are rejected.
- Verify repair can be disabled without affecting base legalization correctness.

#### Open Questions

- The proposal marks overflow repair as optional. Implementation can first ship without it if base legality and runtime are at risk, then add repair as a quality improvement.

### Legality and Metric Checker

#### Responsibility

Verify final placements and compute development metrics. It owns diagnostics, not placement decisions.

#### Inputs and Outputs

Inputs:

- `PlacementModel` with placed cell coordinates.
- Row intervals and fixed obstacles.
- Density estimator.

Outputs:

- Boolean legality result.
- Diagnostics for every violation category.
- Average displacement, optional max displacement, and final DOR.

#### Internal Design

Checks:

- Every movable cell has a placement.
- Cell rectangle is inside the die.
- Cell lower-left x is aligned to `die.llx + k * site_width`.
- Cell lower-left y is aligned to `die.lly + k * site_height`.
- Cell has orientation `R0` in output by writer contract.
- Cell does not overlap any other movable cell.
- Cell does not overlap any `MACRO` or `BLOCKAGE`.
- Cell lies in a row interval that can legally contain it.

Metric calculations:

- Average displacement should be computed from original to placed lower-left coordinates.
- `flow.tcl` uses Manhattan displacement `abs(dx) + abs(dy)` and converts DBU to microns. The PDF does not define the norm, so this checker should expose the chosen norm clearly.
- DOR is delegated to `DensityEstimator`.

#### Dependencies

- `PlacementModel`.
- `DensityEstimator`.

#### Failure Handling

- Return structured diagnostics rather than stopping at the first violation when practical.
- Keep metric computation separate from legality so invalid placements can still be debugged.

#### Independent Test Plan

- Detect out-of-die placement.
- Detect x and y site misalignment.
- Detect cell-cell overlap.
- Detect macro and blockage overlap.
- Validate a known legal tiny placement.
- Verify Manhattan average displacement on a known fixture.

#### Open Questions

- The assignment PDF does not define whether average displacement is Manhattan or Euclidean. The helper `flow.tcl` uses Manhattan distance; this design recommends matching `flow.tcl` for development metrics while keeping the norm isolated.

### TCL Writer

#### Responsibility

Emit assignment-compliant OpenROAD TCL placement commands. It does not validate legality or choose placements.

#### Inputs and Outputs

Inputs:

- `PlacementModel` with final placed cell lower-left coordinates.
- Output path.

Outputs:

- TCL file containing one `place_cell` command per movable cell.

#### Internal Design

- Emit only movable `CELL` instances.
- Preserve deterministic order, preferably original input order unless quality tooling requires another order.
- Convert DBU to microns with enough precision to preserve DBU-derived coordinates.
- Always emit `-orient R0`.
- Never emit `detailed_placement`.

Example:

```tcl
place_cell -inst_name inst6050 -orient R0 -origin {49.4 35.8}
```

#### Dependencies

- `PlacementModel`.

#### Failure Handling

- Report output file open or write errors.
- Refuse to write a cell without a final placement.

#### Independent Test Plan

- Write one placed cell and compare exact command structure.
- Verify DBU-to-micron formatting for integer and fractional micron coordinates.
- Verify macros and blockages are not emitted.
- Verify output text does not contain `detailed_placement`.

#### Open Questions

- None.

### Tests

#### Responsibility

Provide focused unit tests and assignment-interface smoke tests. Tests do not own production logic.

#### Inputs and Outputs

Inputs:

- Small hand-written `.gp` fixtures.
- Public benchmark extracts when available.

Outputs:

- `tests/test_legalizer` result.
- Smoke-test TCL output files under `tests/` or a temporary path.

#### Internal Design

- Keep unit tests small enough to run quickly under `make test`.
- Use deterministic fixtures that do not require OpenROAD.
- Use public benchmark smoke tests for end-to-end runtime and formatting validation when extracted `.gp` files are available.

#### Dependencies

- All production modules.
- Existing `Makefile` test target.

#### Failure Handling

- Tests should fail fast and print the failing module behavior.
- Generated test output should be kept narrow and predictable.

#### Independent Test Plan

- The test module is itself the independent runner.
- Suggested command:

```sh
make test
```

#### Open Questions

- Public `.gp` fixtures are not currently visible in the repository tree, only LEF/DEF public examples. End-to-end `.gp` fixtures may need to be generated through OpenROAD or hand-written for unit coverage.

## Cross-Module Contracts

- Coordinates inside core modules are integer DBU.
- Rectangles use half-open bounds `[llx, urx) x [lly, ury)`.
- `CELL` instances are movable; `MACRO` and `BLOCKAGE` instances are fixed obstacles.
- Legal x origins satisfy `(x - die.llx) % site_width == 0`.
- Legal y origins satisfy `(y - die.lly) % site_height == 0`.
- A row interval is legal only after obstacle subtraction and site snapping.
- Trial row placement must not mutate committed state.
- Final row placement must update both row state and the corresponding placed cell rectangles.
- Density trial penalties are advisory; exact final DOR is computed after a complete placement.
- The TCL writer assumes validation has already succeeded and emits no OpenROAD legalization commands.

## Test Strategy

The implementation should be testable in layers:

1. Parser and model tests verify input decoding and geometry primitives.
2. Row interval tests verify obstacle subtraction and site snapping.
3. Row solver tests verify ABACUS cluster behavior in isolation.
4. Density tests verify 10um grid accounting, macro exclusion, and threshold comparison.
5. Checker tests verify every legality failure category.
6. Writer tests verify TCL syntax and coordinate conversion.
7. End-to-end smoke tests run:

```sh
make
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

The existing `Makefile` already defines `make test`, expecting `tests/test_legalizer` and a one-cell fixture. Because `src/` and `tests/` were not present during design inspection, implementation should create them before relying on this target.

## Risks and Mitigations

- Risk: Hidden benchmarks contain multi-row-height movable cells.
  Mitigation: Detect `cell.height != site_height` early. Either implement consecutive-row interval placement before submission or fail during development rather than emitting illegal TCL.

- Risk: Density trial scoring is too slow if every trial recomputes exact grid overlap.
  Mitigation: Use approximate affected bins for trial scoring and exact DOR only for completed placements.

- Risk: ABACUS fixed-order placement misses better legal placements that require swapping.
  Mitigation: Run both increasing and decreasing x-order passes, and add local overflow repair if runtime allows.

- Risk: Displacement normalization used by the grader differs from local scoring.
  Mitigation: Keep normalization isolated in scoring code and report raw average displacement and DOR separately.

- Risk: Repository structure currently has a `Makefile` that references missing `src/` and `tests/` directories.
  Mitigation: Implementation milestone one should create the expected structure without changing the public executable interface.

- Risk: Final output accidentally includes forbidden OpenROAD commands.
  Mitigation: Keep TCL writer narrowly scoped to `place_cell` lines and add a writer test that searches for `detailed_placement`.

## Open Questions

1. Do hidden benchmarks include movable cells taller than one site row? The base design targets single-row-height cells, but the implementation should detect multi-row cells immediately.
2. Should assignment scoring use Manhattan displacement, Euclidean displacement, or another norm? `flow.tcl` uses Manhattan distance, while the PDF only says average displacement.
3. Should fixed `BLOCKAGE` regions be excluded from DOR grid counts like fixed macros? The PDF explicitly mentions fixed macros; blockages are fixed obstacles for legality but not explicitly named in the DOR denominator rule.
4. Should `alpha` values outside `[0, 1]` be rejected, clamped, or accepted literally? The quality formula implies a weight, so this design recommends rejection.
5. Should overflow repair be required for the first implementation milestone, or treated as a quality-tuning phase after base legality passes?
