# Detailed Design

## Purpose

Design a standalone C++17 placement legalizer for Programming Assignment #3,
"Placement with OpenROAD." The executable is `Legalizer` and must satisfy:

```sh
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

The program reads the `.gp` file emitted by `extract_v2.tcl`, legalizes movable
standard cells onto site rows while avoiding fixed macros and blockages, and
writes an OpenROAD TCL script containing explicit `place_cell` commands only.

Legality is the first priority. Quality improvements are applied only through
moves that preserve legality.

## Source Proposal Summary

`doc/proposal.md` proposes an obstacle-aware density-driven legalizer with these
main stages:

1. Parse the input and normalize geometry in database units.
2. Build obstacle-aware row intervals by splitting rows around macros and
   blockages.
3. Produce a baseline legal placement using Abacus-style interval insertion with
   a Tetris fallback.
4. Compute average displacement and DOR on 10 um by 10 um grids.
5. Run bounded DOR-aware local repair.
6. Validate legality and emit TCL `place_cell` commands.

The assignment PDF confirms the required CLI, Linux C/C++ platform, 30-minute
runtime limit, prohibition on `detailed_placement` in the output TCL, no cell
rotation, site-row legality, and the quality form:

```text
Quality = alpha * AverageDisplacement + (1 - alpha) * DOR
```

The existing `Makefile` names the intended source modules:

- `src/main.cpp`
- `src/placement_model.cpp`
- `src/gp_parser.cpp`
- `src/row_interval_builder.cpp`
- `src/density_estimator.cpp`
- `src/legalizer.cpp`
- `src/tcl_writer.cpp`

Those files are not present yet, so this design treats the implementation as a
greenfield C++17 project using that module layout.

## Design Goals

- Produce legal placements for all supported movable `CELL` instances.
- Preserve each movable cell's original orientation.
- Align every placement to legal site x locations and row y locations.
- Avoid overlap among movable cells, fixed `MACRO` rectangles, and `BLOCKAGE`
  rectangles.
- Keep the implementation deterministic across repeated runs.
- Optimize displacement first through Abacus-style legalization, then improve
  DOR through local legal moves.
- Keep runtime comfortably below the 30-minute grading limit.
- Provide module-level tests that can run without OpenROAD where possible.

## Non-Goals

- Do not emit or call `detailed_placement` in the final output TCL.
- Do not implement a full global placer.
- Do not rotate cells.
- Do not implement network-flow legalization in the initial solution.
- Do not require parallel execution.
- Do not silently support multi-row movable cells. The project decision from the
  proposal and high-level design is to detect them and fail with a clear
  diagnostic unless later benchmark inspection proves support is required.

## Architecture Overview

The executable is a one-way pipeline with explicit validation gates.

```text
CLI/Main
  -> GP Parser
  -> Placement Model
  -> Row Interval Builder
  -> Baseline Legalizer
       -> Abacus Interval Solver
       -> Tetris Fallback
  -> Density / Metric Evaluator
  -> DOR-Aware Local Repair
  -> Legality Validator
  -> TCL Writer
```

All canonical geometry is stored in database units. Conversion to microns is
done only at output and in metric reporting where the assignment definition
requires microns.

The central data model should expose small value types:

```cpp
using Dbu = long long;

struct Point {
  Dbu x;
  Dbu y;
};

struct Rect {
  Dbu lx;
  Dbu ly;
  Dbu ux;
  Dbu uy;
};

enum class ObjectType {
  Cell,
  Macro,
  Blockage
};

struct Cell {
  std::string name;
  Point original;
  Point placed;
  Dbu width;
  Dbu height;
  std::string orient;
  bool placedValid;
};

struct Obstacle {
  std::string name;
  Rect rect;
  ObjectType type;
};

struct Tech {
  int dbuPerMicron;
  Rect die;
  Dbu siteWidth;
  Dbu siteHeight;
};
```

The row model should keep obstacle handling local to row construction:

```cpp
struct RowInterval {
  int rowIndex;
  Dbu y;
  Dbu xMin;
  Dbu xMax;
  Dbu occupiedWidth;
  std::vector<int> cellIds;
};

struct Row {
  int rowIndex;
  Dbu y;
  std::vector<RowInterval> intervals;
};
```

The half-open rectangle convention `[lx, ux) x [ly, uy)` is used internally for
overlap checks.

## Module Designs

### CLI / Main

#### Responsibility

Own process-level behavior. It validates command-line arguments, invokes the
pipeline in order, catches diagnostics, and returns the final exit code. It does
not own parsing, legalization, metrics, or file formatting details.

#### Inputs and Outputs

Inputs:

- `argv[1]`: `alpha`, parsed as a finite double in `[0, 1]`.
- `argv[2]`: `threshold`, parsed as a finite double percentage.
- `argv[3]`: input `.gp` path.
- `argv[4]`: output `.tcl` path.

Outputs:

- Exit code `0` on success.
- Nonzero exit code and a concise diagnostic on malformed input,
  unsupported design content, legalization failure, validation failure, or
  output failure.

#### Internal Design

`main.cpp` should perform only orchestration:

1. Parse arguments.
2. Call `GpParser::parseFile`.
3. Validate global model invariants such as positive DBU, die dimensions, and
   site dimensions.
4. Reject unsupported multi-row movable cells before legalization.
5. Build row intervals.
6. Run the legalizer.
7. Run metric evaluation and local repair.
8. Validate final legality.
9. Write TCL after validation succeeds.

Use typed result objects or exceptions consistently. A simple approach is a
`Status` / `StatusOr<T>` helper with human-readable messages.

#### Dependencies

- GP Parser
- Placement Model
- Row Interval Builder
- Legalizer
- Density / Metric Evaluator
- Legality Validator
- TCL Writer

#### Failure Handling

Fail before writing output if any stage reports an error. If output writing
needs to replace an existing file, write to a same-directory temporary file and
rename it only after the write succeeds.

#### Independent Test Plan

- Build target: `make Legalizer`.
- CLI tests can run the executable against small synthetic `.gp` files:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

- Negative tests should cover wrong argument count, invalid numeric arguments,
  missing input path, and unsupported multi-row cells.

#### Open Questions

None.

### GP Parser

#### Responsibility

Parse the `.gp` file generated by `extract_v2.tcl` into typed technology,
movable cell, macro, and blockage records. It owns input syntax handling but not
legalization decisions.

#### Inputs and Outputs

Input:

- Text file with headers:
  - `DBU_Per_Micron <int>`
  - `DieArea_LL <x> <y>`
  - `DieArea_UR <x> <y>`
  - `Site_Width <int>`
  - `Site_Height <int>`
  - blank line
  - `Name LLX LLY Width Height Orient Type`
  - object records

Outputs:

- `Tech`
- `std::vector<Cell>`
- `std::vector<Obstacle>` for `MACRO` and `BLOCKAGE`

#### Internal Design

Parsing rules:

- Trim empty lines before and after the object header.
- Require all technology fields exactly once.
- Parse `CELL` and `MACRO` records with seven fields:
  `name llx lly width height orient type`.
- Parse `BLOCKAGE` records with six fields:
  `name llx lly width height BLOCKAGE`.
- Treat the PDF's shortened header example as documentation ambiguity; the
  actual extractor emits `Orient Type`, so token count determines record shape.
- Store lower-left coordinates and dimensions as DBU integers.
- Convert dimensions to half-open rectangles by adding width and height.
- Preserve object order for deterministic output.

Validation inside the parser:

- DBU per micron, site width, site height, cell widths, and heights must be
  positive.
- Die upper bounds must exceed lower bounds.
- Object type must be one of `CELL`, `MACRO`, or `BLOCKAGE`.
- `CELL` and `MACRO` records must include an orientation token.

#### Dependencies

- Placement Model value types.

#### Failure Handling

Return line-numbered diagnostics for malformed records, missing headers,
invalid integer fields, nonpositive dimensions, or unknown object types.

#### Independent Test Plan

Create parser-only fixtures under `tests/` and run them through
`tests/test_legalizer`:

- Valid one-cell input.
- Valid macro and blockage input.
- Blockage with six fields.
- Missing technology field.
- Bad object type.
- Negative or zero width.
- Missing orientation for a `CELL`.

#### Open Questions

None.

### Placement Model

#### Responsibility

Own canonical geometry, placement state, coordinate snapping helpers, and shared
geometry predicates. It does not decide which legal position is best.

#### Inputs and Outputs

Inputs:

- Parsed technology data.
- Parsed cells and obstacles.

Outputs:

- Queryable die, site, row, cell, and obstacle data.
- Mutable placed coordinates for each movable cell.
- Geometry helpers for overlap and containment checks.

#### Internal Design

Core helpers:

- `Rect rectForCell(cellId, placedPoint)`.
- `bool overlaps(Rect a, Rect b)` using half-open rectangles.
- `bool contains(Rect outer, Rect inner)`.
- `Dbu snapDownToSite(Dbu x)` and `Dbu snapUpToSite(Dbu x)` relative to the die
  lower-left x-coordinate.
- `bool isSiteAligned(Dbu x)`.
- `bool isRowAligned(Dbu y)`.
- `double dbuToMicron(Dbu value)`.
- `Dbu micronToDbu(double value)` for 10 um grid construction.

Multi-row detection:

- A movable cell is supported only when `cell.height == tech.siteHeight`.
- Unsupported cells are reported before the legalizer mutates placement state.

Coordinate convention:

- A cell origin is the lower-left point.
- A cell occupies `[x, x + width) x [y, y + height)`.
- Rows start at `die.ly + k * siteHeight` while `rowY + siteHeight <= die.uy`.

#### Dependencies

None beyond the C++ standard library.

#### Failure Handling

Provide assertion-like checks for internal invariants in tests, but production
validation should report user-facing diagnostics through the validator.

#### Independent Test Plan

- Unit-test rectangle overlap edge contact versus true overlap.
- Unit-test die containment.
- Unit-test site snapping with nonzero die lower-left x.
- Unit-test row generation bounds with die heights that are not exact multiples
  of site height.
- Unit-test DBU-to-micron conversion.

#### Open Questions

None.

### Row Interval Builder

#### Responsibility

Convert the die and fixed obstacles into legal placement intervals. This module
owns obstacle awareness so later legalization stages can work against obstacle-
free intervals rather than repeatedly checking every fixed rectangle.

#### Inputs and Outputs

Inputs:

- `Tech`
- Fixed `MACRO` and `BLOCKAGE` rectangles.

Outputs:

- `std::vector<Row>` containing site-aligned legal intervals.
- Optional index from row y-coordinate to row index.

#### Internal Design

Algorithm:

1. Generate rows from `die.ly` in increments of `siteHeight`.
2. For each row, start with one raw interval covering `[die.lx, die.ux)`.
3. For each fixed obstacle whose y-range overlaps the row band
   `[rowY, rowY + siteHeight)`, project the obstacle x-range onto that row.
4. Clip the projection to the die x-range.
5. Snap the blocked projection outward to site boundaries:
   - left blocked boundary snaps down,
   - right blocked boundary snaps up,
   so no cell can overlap the obstacle because of site rounding.
6. Subtract blocked spans from the row's legal spans.
7. Drop intervals whose site-aligned capacity is smaller than one site.
8. Store legal interval bounds as site-aligned `[xMin, xMax)` DBU values.

Capacity:

- `intervalCapacitySites = (xMax - xMin) / siteWidth`.
- A cell fits if `cell.width <= xMax - xMin - occupiedWidth` and its width is a
  multiple of site width. If a hidden input contains a non-site-multiple width,
  the model should reject it rather than produce fractional site occupancy.

Determinism:

- Obstacles are sorted by `(ly, lx, name)` before subtraction.
- Intervals are kept sorted by x within each row.

#### Dependencies

- Placement Model.

#### Failure Handling

Return an error if no legal rows exist or if a row interval arithmetic invariant
is violated. Do not fail just because obstacles cover some or all rows.

#### Independent Test Plan

- No obstacles: one interval per row.
- Obstacle outside die: unchanged intervals after clipping.
- Obstacle covering a row middle: split into left and right intervals.
- Obstacle touching a boundary: shrink only the touched side.
- Obstacle covering a full row: no interval for that row.
- Obstacle spanning multiple rows: split all overlapped rows.
- Boundary contact at row top or bottom should not count as overlap under
  half-open geometry.

#### Open Questions

None.

### Baseline Legalizer

#### Responsibility

Produce the first complete legal placement for all supported movable cells. It
owns cell assignment to intervals and commits placement state. It does not own
final TCL formatting or global metric reporting.

#### Inputs and Outputs

Inputs:

- Movable cells with original positions.
- Row intervals.
- Technology and geometry helpers.

Outputs:

- Every cell assigned to one legal interval.
- Placement coordinates in DBU.
- Interval cell lists and occupancy updated consistently.

#### Internal Design

Cell processing order:

- Primary order: original y row proximity bucket, then original x, then name.
- A simpler deterministic order `(original.x, original.y, name)` is acceptable
  for the first implementation if tests cover legality.
- Optional reverse or alternate pass can be used only after preserving a legal
  incumbent placement.

Candidate interval search:

1. Find the nearest row index to the cell's original y.
2. Search rows in increasing vertical distance.
3. In each row, evaluate intervals whose remaining width can fit the cell.
4. Stop after a configurable row radius once at least one feasible candidate has
   been found; expand to all rows only if needed for legality.

Candidate scoring:

- Lower bound: vertical displacement in microns or DBU.
- Tentative insertion score: total displacement delta for the affected interval
  after Abacus repacking.
- Tie-breakers: lower DOR-grid occupancy estimate, lower x displacement, lower
  row index, lower interval x, then cell name.

Commit:

- Replace the affected interval's cell order and x positions only for the
  winning candidate.
- Mark `placedValid = true`.
- Update interval `occupiedWidth`.

Fallback:

- If no Abacus candidate succeeds, call the Tetris Fallback over all intervals.
- If fallback also fails, return a legalization failure naming the cell and its
  dimensions.

#### Dependencies

- Placement Model
- Row Interval Builder output
- Abacus Interval Solver
- Tetris Fallback

#### Failure Handling

Never leave partially committed state after a failed candidate trial. Candidate
trials should operate on copied interval cell lists and proposed coordinates,
then commit only the selected trial.

#### Independent Test Plan

- One cell placed in the nearest row.
- Multiple cells in one row with overlapping original positions.
- Cells forced around a macro-created interval split.
- Case where the nearest row lacks capacity and a farther row is selected.
- Case where Abacus trial is unavailable and fallback finds a gap.
- Case where total row capacity is insufficient and legalization fails clearly.

#### Open Questions

None.

### Abacus Interval Solver

#### Responsibility

Given one interval and an ordered list of cells assigned to that interval,
compute non-overlapping site-aligned x positions that minimize row-local
displacement under interval bounds.

#### Inputs and Outputs

Inputs:

- Interval bounds `[xMin, xMax)`.
- Row y-coordinate.
- Ordered cell IDs.
- Original x-coordinate and width for each cell.

Outputs:

- Proposed x-coordinate for each cell in the interval.
- Row-local displacement cost.

#### Internal Design

Use Abacus-style clustering:

1. Process cells in interval order.
2. Create a cluster with:
   - first cell index,
   - last cell index,
   - total width,
   - weighted target x sum,
   - current cluster x.
3. Initial cluster x is the weighted average target clamped to interval bounds.
4. If a new cluster overlaps the previous cluster, merge them.
5. After each merge, recompute cluster x and clamp it.
6. Expand clusters into cell x positions from left to right.
7. Snap final cluster starts to site boundaries and repair any site-rounding
   overlap by shifting within interval bounds.

Weights:

- Start with uniform weight `1` per cell. This matches a simple displacement
  objective and avoids hidden tuning assumptions.

Ordering:

- The solver accepts the order supplied by the Baseline Legalizer or repair
  module. It does not choose row assignment or cross-row moves.

#### Dependencies

- Placement Model geometry and site snapping helpers.

#### Failure Handling

Return failure if total cell width exceeds interval width or site snapping makes
the expanded cells overflow the interval.

#### Independent Test Plan

- Empty interval.
- One cell clamped left, centered, and clamped right.
- Two overlapping cells that merge into one cluster.
- Multi-cell chain merge.
- Total width exactly equal to interval width.
- Width greater than interval width fails.
- Nonzero interval x-min and nonzero die origin.

#### Open Questions

None.

### Tetris Fallback

#### Responsibility

Provide a simple legality-preserving insertion when Abacus candidate insertion
cannot find a placement for a cell.

#### Inputs and Outputs

Inputs:

- One unplaced cell.
- Current interval occupancy and cell placements.
- Candidate intervals, usually all legal intervals.

Outputs:

- Legal coordinate for the cell.
- Updated interval cell list and occupancy.

#### Internal Design

For each interval:

1. Collect occupied spans from already placed cells in that interval.
2. Sort spans by x.
3. Enumerate legal gaps between `xMin`, occupied spans, and `xMax`.
4. Snap the cell's preferred x to the nearest site inside each gap.
5. Score by Manhattan displacement from original lower-left coordinate.
6. Choose the lowest score with deterministic tie-breakers.

After choosing a gap, insert the cell into the interval's x-ordered cell list.

#### Dependencies

- Placement Model
- Row intervals

#### Failure Handling

Return failure if no interval gap can fit the cell.

#### Independent Test Plan

- Place into an empty interval.
- Place before, between, and after existing cells.
- Respect site alignment in gaps.
- Choose the nearest of two equal-size gaps.
- Fail when no gap can fit the cell.

#### Open Questions

None.

### Density / Metric Evaluator

#### Responsibility

Compute the assignment metrics needed for final reporting and local repair:
average displacement, DOR, and weighted quality.

#### Inputs and Outputs

Inputs:

- Current placement state.
- Technology data.
- Fixed macro rectangles.
- `alpha`.
- `threshold`.

Outputs:

```cpp
struct Metrics {
  double averageDisplacementMicron;
  double dorPercent;
  double quality;
  int totalGrids;
  int overflowGrids;
};
```

The evaluator should also expose affected-grid queries for local repair.

#### Internal Design

Average displacement:

- Use Manhattan distance between original and placed lower-left coordinates.
- Convert DBU to microns before averaging.

DOR grid:

- Grid size is `10 um x 10 um`, converted to DBU with `dbuPerMicron`.
- Enumerate grid cells intersecting the die.
- Exclude grids covered by fixed macros from the denominator, matching the
  assignment statement. A grid is considered macro-covered if its rectangle
  overlaps any `MACRO` rectangle.
- Do not exclude `BLOCKAGE` grids unless later OpenROAD comparison shows that
  TA scoring does so; blockages are placement obstacles but the assignment text
  names fixed macros for DOR exclusion.
- For each included grid, accumulate standard-cell occupied area overlapping
  the grid.
- Density percentage is:

```text
100 * occupiedCellArea / gridArea
```

- A grid overflows when `density > threshold`.

Quality:

- Default design follows the PDF and proposal:

```text
quality = alpha * averageDisplacementMicron + (1 - alpha) * dorPercent
```

The repository's `flow.tcl` contains a debugging normalization factor for
displacement. That script is useful for public-flow comparison, but the PDF and
proposal remain the design source for the implemented quality evaluator.

Incremental support:

- Keep a grid index from cell rectangles to overlapping grid IDs.
- For local repair, compute exact metric deltas by removing and re-adding only
  moved cells' area contributions, then recompute overflow status for touched
  grids.

#### Dependencies

- Placement Model.

#### Failure Handling

If there are no denominator grids after macro exclusion, report DOR as `0` and
total grids as `0`. This avoids division by zero while preserving a clear metric
state.

#### Independent Test Plan

- One cell fully inside one 10 um grid.
- One cell crossing multiple grids.
- Macro-covered grid excluded from denominator.
- Boundary case where density equals threshold is not overflow because the PDF
  and flow use `density > threshold`.
- Empty placement has zero overflow.
- Quality calculation for `alpha = 0`, `alpha = 1`, and a mixed alpha.

#### Open Questions

- The public `flow.tcl` normalizes average displacement by `18.2` before quality
  reporting, while the assignment PDF and proposal do not specify this factor.
  The implementation design follows the PDF. Public benchmarking can still log
  the flow-compatible value for analysis if desired.

### DOR-Aware Local Repair

#### Responsibility

Improve the weighted metric after a complete legal placement exists. It owns
bounded local search but must never accept an illegal move.

#### Inputs and Outputs

Inputs:

- Legal placement from Baseline Legalizer.
- Row intervals and interval cell lists.
- Metric evaluator.
- `alpha` and `threshold`.

Outputs:

- Equal-or-better legal placement according to the accepted move policy.
- Updated row intervals and metrics.

#### Internal Design

Repair loop:

1. Compute current metrics and overflow grids.
2. Rank overflow grids by density descending.
3. For each high-overflow grid, collect movable cells contributing area to that
   grid.
4. Rank candidate cells by contribution area, then by low displacement penalty.
5. Try bounded candidate operations:
   - Reinsert one cell into nearby rows and repack affected source/destination
     intervals with the Abacus solver.
   - Move one cell into a nearby gap using Tetris-style placement.
   - Swap two nearby cells if dimensions and interval repacking make the swap
     legal.
   - Reorder a small window inside one interval and repack it.
6. Accept a candidate if it improves weighted quality.
7. Optionally accept a DOR-improving candidate with no quality improvement only
   when `alpha` is low and the displacement penalty cap is configured.
8. Stop after a fixed iteration limit or one full pass with no accepted move.

Suggested initial limits:

- Inspect at most the top `K` overflow grids per pass.
- Inspect at most `M` candidate cells per overflow grid.
- Search destination rows within a configurable radius first, then expand only
  if the current DOR is high.

These constants should be defined in one implementation location so public-case
tuning does not scatter through the code.

State management:

- Candidate operations must snapshot only affected intervals and cell
  placements.
- On rejection, restore the snapshot.
- On acceptance, update placements, interval lists, interval occupancy, and
  touched density grids.

#### Dependencies

- Placement Model
- Row intervals
- Abacus Interval Solver
- Tetris Fallback gap enumeration
- Density / Metric Evaluator

#### Failure Handling

Repair is optional for legality. If no improving candidate is found, keep the
baseline legal placement. If an internal candidate operation violates legality,
reject that candidate and continue; repeated internal violations should be
diagnosed in debug builds.

#### Independent Test Plan

- A legal placement with no overflow remains unchanged.
- A cell can be moved out of an overflow grid into a legal nearby gap.
- A rejected candidate restores all affected placements and interval lists.
- A swap is accepted only when both affected intervals remain legal.
- Iteration limits terminate on dense synthetic cases.

#### Open Questions

None.

### Legality Validator

#### Responsibility

Validate the final placement before output is written. It owns correctness
checks and diagnostics but does not repair placements.

#### Inputs and Outputs

Inputs:

- Placement state.
- Technology data.
- Fixed obstacles.
- Row intervals.

Outputs:

- Success.
- Or a list of diagnostics precise enough to identify the illegal cell,
  obstacle, row, or coordinate.

#### Internal Design

Checks:

1. Every movable cell has exactly one valid placement.
2. Every cell rectangle is inside the die.
3. Every cell x-coordinate is site-aligned.
4. Every cell y-coordinate is row-aligned.
5. Every supported cell height equals `siteHeight`.
6. Every cell preserves its parsed orientation.
7. No movable cell overlaps a macro or blockage.
8. No pair of movable cells overlaps.
9. Every cell is contained in one legal row interval.

Efficient overlap checking:

- For movable-vs-movable checks, bucket cells by row index and sort by x.
- Adjacent x-overlap checks are sufficient within a single row for single-row
  cells after sorting.
- For obstacle checks, use row-interval containment first. A slower direct
  rectangle check can run in debug or test mode.

#### Dependencies

- Placement Model
- Row intervals

#### Failure Handling

Return all diagnostics found in one pass where practical. The CLI should print
the first several diagnostics and total count if there are many.

#### Independent Test Plan

- Missing placement.
- Out-of-die placement.
- Off-site x.
- Off-row y.
- Movable overlap.
- Obstacle overlap.
- Placement outside every interval.
- Valid placement succeeds.

#### Open Questions

None.

### TCL Writer

#### Responsibility

Write the final OpenROAD placement script after validation succeeds.

#### Inputs and Outputs

Inputs:

- Validated movable cell placements.
- DBU per micron.
- Output path.

Outputs:

- TCL file containing one `place_cell` command per movable cell.

#### Internal Design

Output command:

```tcl
place_cell -inst_name <instName> -orient <orient> -origin {<xMicron> <yMicron>}
```

Rules:

- Preserve parsed orientation.
- Convert DBU coordinates to microns at write time.
- Emit fixed decimal precision high enough to round-trip common DBU values, for
  example six decimal digits.
- Preserve parser order for deterministic output.
- Never emit `detailed_placement`.
- Write only movable cells, not macros or blockages.

File safety:

- Write to a same-directory temporary file.
- Close and flush successfully.
- Rename to the requested output path.

#### Dependencies

- Placement Model.

#### Failure Handling

Return diagnostics for file-open, write, flush, close, or rename failure.

#### Independent Test Plan

- One-cell output command format.
- Orientation preservation.
- DBU-to-micron formatting.
- Absence of `detailed_placement`.
- Deterministic output order.
- Simulated unwritable output path returns failure.

#### Open Questions

None.

### Tests

#### Responsibility

Provide independently runnable checks for parser, geometry, row interval,
legalization, metrics, validation, and output behavior.

#### Inputs and Outputs

Inputs:

- Synthetic `.gp` fixtures under `tests/`.
- Public benchmark-derived smoke inputs when generated by OpenROAD.

Outputs:

- `tests/test_legalizer` pass/fail result.
- CLI smoke output TCL.

#### Internal Design

Use a lightweight C++ test harness in `tests/test_legalizer.cpp` so no external
test framework is required. Group tests by module and keep fixtures tiny enough
to debug by inspection.

The existing `Makefile` already defines:

```sh
make test
```

which should build `tests/test_legalizer`, build `Legalizer`, run unit tests,
and run one CLI smoke case.

#### Dependencies

- All implementation modules.
- No OpenROAD dependency for unit tests.

#### Failure Handling

Each assertion should print the test name, expected value, and actual value.
The test executable should return nonzero on the first failed test or collect
failures and return nonzero at the end.

#### Independent Test Plan

This module is itself the test harness. It should support running all tests with:

```sh
make test
```

OpenROAD flow validation remains a separate integration step because OpenROAD is
not guaranteed to be available in every local environment.

#### Open Questions

None.

## Cross-Module Contracts

### Coordinate Contract

- Internal coordinates are DBU integers.
- Cell origins are lower-left coordinates.
- Rectangles are half-open.
- Micron conversion happens only in metrics and TCL output.

### Object Contract

- `CELL` records are movable.
- `MACRO` records are fixed obstacles and excluded from DOR denominator grids.
- `BLOCKAGE` records are fixed obstacles for placement legality.
- Object order from the input is preserved for deterministic output.

### Legal Interval Contract

- Row intervals are obstacle-free, site-aligned, sorted by x, and half-open.
- A placed cell must be fully contained in exactly one row interval.
- Legalizers update interval cell lists and occupancy whenever placement state
  changes.

### Solver Contract

- Abacus Interval Solver operates on one interval at a time.
- Tetris Fallback may inspect all intervals but commits exactly one cell.
- DOR Repair may mutate only through candidate operations that can be fully
  rolled back.

### Output Contract

- TCL Writer runs only after Legality Validator succeeds.
- Output contains only `place_cell` commands for movable cells.
- Output must not contain `detailed_placement`.

## Test Strategy

Run tests in increasing integration order:

1. Parser and placement model unit tests.
2. Row interval builder unit tests.
3. Abacus interval solver unit tests.
4. Tetris fallback unit tests.
5. Baseline legalizer unit tests.
6. Density / metric evaluator unit tests.
7. DOR repair unit tests.
8. Legality validator unit tests.
9. TCL writer unit tests.
10. CLI smoke test through `make test`.
11. OpenROAD integration on both public benchmarks:

```sh
make
./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl
```

Then source the generated TCL in OpenROAD and run:

```tcl
check_placement -verbose
```

The OpenROAD integration should also verify that the generated TCL is nonempty
and contains no `detailed_placement`.

## Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Hidden benchmarks contain multi-row movable cells. | The initial legalizer rejects the case. | Keep rejection diagnostic explicit and isolate cell-height logic so support can be added later. |
| DOR estimator differs from TA's OpenROAD heatmap. | Repair may optimize a slightly different objective. | Match the PDF definition, compare against public `gui::dump_heatmap` output, and tune only after legality is stable. |
| `flow.tcl` references `extract.tcl` while this repo contains `extract_v2.tcl`. | Local integration flow may need adjustment before use. | Treat `extract_v2.tcl` as the parser contract and update flow separately during implementation if needed. |
| Obstacle projection loses site capacity near boundaries. | Legalizer may fail or create illegal overlaps. | Snap blocked spans outward and unit-test boundary cases. |
| Repair worsens high-alpha displacement quality. | Ranking quality may degrade for displacement-heavy runs. | Accept repair moves only when weighted quality improves, with conservative DOR-only acceptance. |
| Large benchmarks make exact candidate evaluation expensive. | Runtime may approach the 30-minute limit. | Use row-radius search, fixed repair limits, incremental density deltas, and deterministic early stopping. |
| Output precision rounds legal DBU coordinates poorly. | OpenROAD may see off-site positions. | Emit fixed decimal precision sufficient for DBU conversion and validate through OpenROAD public cases. |

## Open Questions

- The assignment PDF and proposal define quality with raw average displacement
  and DOR, while the provided `flow.tcl` debug scorer multiplies average
  displacement by `18.2`. The design follows the PDF/proposal; public-flow
  comparison should record whether the normalization affects tuning choices.
- The assignment text names fixed macros for DOR grid exclusion. The design
  excludes `MACRO` grids and treats `BLOCKAGE` records only as placement
  obstacles unless public OpenROAD heatmap comparison shows the TA flow excludes
  blockages too.
