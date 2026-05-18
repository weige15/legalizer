# Detailed Design

## Purpose

Design a standalone Linux command-line placement legalizer for Programming Assignment #3, "Placement with OpenROAD." The executable, `Legalizer`, reads an OpenROAD-extracted `.gp` file, legalizes movable standard cells onto legal site-row positions, avoids macros, blockages, and other cells, then writes an OpenROAD TCL file containing only `place_cell` commands for movable cells.

Legality is the primary objective. After legality is satisfied, the design should reduce the assignment quality metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of non-macro 10um x 10um density grids whose utilization exceeds the supplied threshold.

## Source Proposal Summary

The source proposal defines a C++ batch legalizer with four main stages:

1. Parse the `.gp` input and preserve original global-placement coordinates.
2. Construct a row/site legality model by subtracting fixed macro and blockage intervals from each placement row.
3. Legalize movable `CELL` records through deterministic candidate search using displacement and density pressure.
4. Improve the solution with legality-preserving local refinement and emit OpenROAD TCL.

The high-level design further separates this pipeline into these modules:

- CLI and Configuration
- GP Parser
- Placement Data Model
- Row and Obstacle Model
- Density Grid Model
- Legalization Engine
- Local Refinement
- TCL Writer
- Validation Helpers

## Design Goals

- Build with `make` and produce an executable named `Legalizer`.
- Support the required command:

```sh
./Legalizer <alpha> <threshold> <input_file> <output_file>
```

- Parse `DBU_Per_Micron`, die bounds, site dimensions, movable cells, macros, and blockages from the assignment `.gp` format.
- Use integer DBU coordinates internally for all legality decisions.
- Place only objects with type `CELL`.
- Treat objects with type `MACRO` and `BLOCKAGE` as fixed obstacles.
- Keep every placed cell inside the die, aligned to legal site columns and rows, and non-overlapping.
- Emit one `place_cell -inst_name <name> -orient R0 -origin {X Y}` command per movable cell, with `X` and `Y` in microns.
- Keep the output TCL free of `detailed_placement`.
- Keep the algorithm deterministic for reproducible benchmark comparisons.
- Bound candidate search and refinement work so hidden tests can finish within the 30-minute grading limit.

## Non-Goals

- Do not call OpenROAD from inside `Legalizer`.
- Do not emit commands for macros or blockages.
- Do not support cell rotation; every output command uses `-orient R0`.
- Do not modify LEF, DEF, benchmark, or OpenROAD database files.
- Do not attempt to exactly reproduce OpenROAD detailed placement.
- Do not make the internal DOR estimator the source of truth for grading; it is only a placement heuristic.

## Architecture Overview

The program is a single-process batch pipeline:

```text
CLI -> GP Parser -> Placement Data Model -> Row and Obstacle Model
    -> Density Grid Model -> Legalization Engine -> Local Refinement
    -> Validation Helpers -> TCL Writer
```

All geometric modules use half-open rectangles `[llx, urx) x [lly, ury)` in DBU. A cell placement is represented by the cell's lower-left origin. A candidate is legal only if its full rectangle fits within one or more compatible placement rows and does not intersect unavailable row intervals.

The Row and Obstacle Model is the geometric legality authority. The Density Grid Model is advisory: it estimates whether placing a cell in a region will increase overflow pressure, but it cannot override row/site legality.

## Module Designs

### CLI and Configuration

#### Responsibility

Own command-line parsing and immutable runtime options. It validates the required argument count, parses `alpha` and `threshold`, stores input and output paths, and reports user-facing usage errors.

It does not parse `.gp` contents, build geometry, place cells, or write TCL commands.

#### Inputs and Outputs

Inputs:

- `argc`, `argv`
- Required command shape: `./Legalizer <alpha> <threshold> <input_file> <output_file>`

Outputs:

- `Config { double alpha; double threshold; string inputPath; string outputPath; }`
- Nonzero exit code and diagnostic text on invalid arguments.

Validation:

- `alpha` must parse as a finite floating-point value.
- `threshold` must parse as a finite floating-point value.
- The input and output file names are accepted as paths; filename convention is enforced by the grading flow rather than by the program.

#### Internal Design

- Parse numeric fields with `std::stod`.
- Reject trailing nonnumeric characters in `alpha` and `threshold`.
- Keep the accepted range for `alpha` and `threshold` as an open question unless the implementation later chooses to clamp or reject values outside expected grading ranges.

#### Dependencies

- C++ standard library only.

#### Failure Handling

- Missing arguments: print usage and return failure.
- Malformed numeric argument: print the offending argument and return failure.

#### Independent Test Plan

- Build a small test executable or unit test that calls the parser helper with valid and invalid argument vectors.
- Manual smoke command:

```sh
./Legalizer 0.7 45 missing.gp out.tcl
```

Expected result for missing input should be a parser-stage file error after CLI parsing succeeds.

#### Open Questions

- Should `alpha` be restricted to `[0, 1]`, or should the program accept any finite value because the assignment caller is expected to pass valid values?
- Should `threshold` be restricted to a percentage range, or simply treated as the same density scale used by the OpenROAD heatmap CSV?

### GP Parser

#### Responsibility

Read the assignment `.gp` format and convert text records into typed design data. It owns file syntax validation, required metadata detection, numeric field parsing, and object type classification.

It does not snap coordinates, subtract obstacles, legalize cells, or compute quality.

#### Inputs and Outputs

Inputs:

- Input `.gp` path from `Config`.
- File format:

```text
DBU_Per_Micron <integer>
DieArea_LL <x> <y>
DieArea_UR <x> <y>
Site_Width <integer>
Site_Height <integer>

Name LLX LLY Width Height Type
<instName> <llx> <lly> <width> <height> <CELL|MACRO|BLOCKAGE>
```

Outputs:

- `ParsedDesign`
  - `int dbuPerMicron`
  - `Point dieLL`
  - `Point dieUR`
  - `int siteWidth`
  - `int siteHeight`
  - `vector<ObjectRecord> records`

Each `ObjectRecord` stores:

- `string name`
- `Rect originalRect`
- `ObjectType type`

#### Internal Design

- Read line by line with `std::getline`.
- Ignore empty lines before the `Name LLX LLY Width Height Type` header.
- Require the five metadata lines before instance records.
- Treat coordinates and dimensions as signed 64-bit integers to avoid overflow on hidden cases.
- Convert width/height to a rectangle: `urx = llx + width`, `ury = lly + height`.
- Reject non-positive width or height for all records.
- Reject unknown object types.
- Preserve input order as `inputIndex` for deterministic tie-breaking.

#### Dependencies

- File I/O and string parsing from the C++ standard library.
- Placement Data Model type definitions.

#### Failure Handling

- Missing file: fail with a clear diagnostic.
- Missing required metadata: fail before legalization.
- Malformed record: include line number in the diagnostic.
- Unknown object type: fail because silently treating it as fixed or movable can break grading.

#### Independent Test Plan

- Parser-only fixture with the sample format from the PDF.
- Fixtures for missing metadata, bad numeric values, unknown type, and negative dimensions.
- Expected command target if a test harness is added:

```sh
make test_parser
```

#### Open Questions

- The assignment shows a mandatory blank line before the header, but generated files may omit it. The parser design tolerates either form.

### Placement Data Model

#### Responsibility

Provide shared value types and normalized design containers used by all downstream modules. It owns object identity, original positions, final positions, rectangle helpers, and unit conversion utilities.

It does not perform file I/O, choose placements, or mutate row occupancy by itself.

#### Inputs and Outputs

Inputs:

- `ParsedDesign` from the GP Parser.

Outputs:

- `Design`
  - metadata: DBU, die rectangle, site dimensions
  - `vector<Cell> cells`
  - `vector<Obstacle> obstacles`

`Cell` fields:

- `name`
- `width`, `height`
- `origX`, `origY`
- `placedX`, `placedY`
- `inputIndex`

`Obstacle` fields:

- `name`
- `Rect rect`
- `ObjectType type`

#### Internal Design

- Split `CELL` records into `cells`.
- Split `MACRO` and `BLOCKAGE` records into `obstacles`.
- Use `int64_t` for DBU geometry.
- Use helper functions:
  - `rectAt(cell, x, y)`
  - `manhattanDisplacement(cell, x, y)`
  - `dbuToMicron(value, dbuPerMicron)`
  - `overlaps(a, b)` with half-open rectangle semantics
  - `ceilToSite(x, origin, siteWidth)`
  - `floorToSite(x, origin, siteWidth)`

#### Dependencies

- GP Parser output.

#### Failure Handling

- Invalid die dimensions or non-positive site dimensions cause a fatal validation error.
- A cell taller than the die or wider than the die is reported before legalization attempts.

#### Independent Test Plan

- Unit tests for rectangle overlap, site snapping, DBU-to-micron conversion, and object splitting.
- Test edge cases where rectangles touch at an edge; touching should not count as overlap under half-open semantics.

#### Open Questions

- The assignment forbids rotation but does not state whether all cells have exactly one-row height. The data model supports multi-row heights; the Row and Obstacle Model decides legality for all intersected rows.

### Row and Obstacle Model

#### Responsibility

Derive legal site rows and maintain available row intervals after subtracting fixed obstacles and committed movable cells. This module owns geometric legality queries and occupancy updates.

It does not score density, choose global ordering, or write output.

#### Inputs and Outputs

Inputs:

- Die rectangle.
- `siteWidth`, `siteHeight`.
- Fixed obstacles.

Outputs:

- `vector<Row> rows`
- Legality methods:
  - `bool canPlace(cell, x, y)`
  - `vector<CandidateSlot> enumerateSlots(cell, targetX, targetRow, radiusRows, maxPerRow)`
  - `void commit(cellId, x, y)`
  - `void uncommit(cellId)` for refinement

`Row` fields:

- `int rowIndex`
- `int64_t y`
- `vector<Interval> freeIntervals`
- `vector<PlacedInterval> occupiedIntervals`

Intervals are half-open `[xStart, xEnd)`.

#### Internal Design

1. Derive row origins:
   - `rowY = dieLL.y + rowIndex * siteHeight`
   - Include rows while `rowY + siteHeight <= dieUR.y`.
2. Initialize each row with one free interval `[dieLL.x, dieUR.x)`.
3. For each obstacle, find rows whose vertical span intersects the obstacle rectangle.
4. Subtract the obstacle's horizontal span from each intersected row.
5. Snap resulting free intervals:
   - left boundary up to the next legal site column.
   - right boundary down to the last site-aligned end that can contain a cell origin.
6. Store intervals sorted and merged.
7. During placement, either split free intervals on commit or maintain occupied intervals separately and query both. The implementation should prefer interval splitting for fast candidate enumeration.

For multi-row cells, `canPlace` checks every row touched by `[y, y + cell.height)` and requires the same `[x, x + cell.width)` span to be available in each touched row.

#### Dependencies

- Placement Data Model.

#### Failure Handling

- If no legal rows exist, fail before placement.
- If a specific cell cannot fit into any row interval even with no movable occupancy, record it as an unrecoverable legalization failure.
- If a commit request is illegal, return failure rather than mutating occupancy.

#### Independent Test Plan

- Build small synthetic row models without reading files.
- Test obstacle subtraction for:
  - obstacle outside die,
  - obstacle crossing one row,
  - obstacle crossing multiple rows,
  - obstacle touching interval boundaries,
  - blockage partly outside die.
- Test `canPlace` with one-row and multi-row cells.
- Expected command target if added:

```sh
make test_rows
```

#### Open Questions

- The proposal derives rows from die bounds and site height because the `.gp` file does not include explicit row start/count/orientation. If hidden cases contain partial or irregular rows that cannot be represented this way, extra input data would be needed.

### Density Grid Model

#### Responsibility

Estimate 10um x 10um density-grid pressure during legalization and refinement. It provides a candidate cost term that helps reduce DOR, but does not decide legality.

It does not parse OpenROAD heatmap CSV files and does not replace the final `flow.tcl` scoring flow.

#### Inputs and Outputs

Inputs:

- Die rectangle.
- `dbuPerMicron`.
- `threshold`.
- Fixed macros.
- Cell trial placements and committed placements.

Outputs:

- `double densityCost(cell, x, y)`
- `void addCell(cell, x, y)`
- `void removeCell(cell, x, y)`
- Optional `double estimateDOR()`

#### Internal Design

- Grid bin size is `10 * dbuPerMicron` DBU in both dimensions.
- Build grid bounds over the die area.
- Mark bins overlapped by fixed macros as excluded from DOR denominator. Blockages are obstacles for legality but are not specified as macro-excluded density regions.
- Maintain approximate occupied area per non-excluded bin from placed cells.
- For a candidate, compute overlapped bins and estimate added utilization:

```text
utilization = occupiedCellAreaInBin / binArea * 100
```

- Candidate density cost should increase when a placement pushes any non-excluded bin above `threshold`.
- Normalize or scale the density cost locally before combining with displacement, because displacement is in DBU/microns while DOR pressure is percentage-like.

#### Dependencies

- Placement Data Model.
- CLI threshold.

#### Failure Handling

- If the die is smaller than one 10um bin, still create at least one bin covering the die.
- If all bins are macro-excluded, density cost returns zero and `estimateDOR()` returns zero to avoid division by zero.

#### Independent Test Plan

- Synthetic die with known DBU, one cell, and one macro.
- Verify bin indexing, macro-excluded bins, area accumulation, removal, and overflow count.
- Test threshold comparisons at below, equal, and above threshold.
- Expected command target if added:

```sh
make test_density
```

#### Open Questions

- OpenROAD heatmap's exact density scaling may differ from this area-based estimate. The design treats the internal model as a heuristic and validates final DOR with `flow.tcl`.
- Whether bins partially overlapped by macros should be fully excluded or area-adjusted is not specified by the PDF; the design uses overlap-based exclusion unless later benchmark validation suggests another approximation.

### Legalization Engine

#### Responsibility

Assign every movable cell to a legal site-row origin. It owns cell ordering, candidate search, cost evaluation, fallback expansion, and final initial placement.

It does not perform input parsing, obstacle preprocessing, or TCL writing.

#### Inputs and Outputs

Inputs:

- `Design.cells`
- Row and Obstacle Model
- Density Grid Model
- `alpha`

Outputs:

- All cells have `placedX`, `placedY`.
- Row occupancy and density occupancy are committed consistently.

#### Internal Design

Cell ordering:

- Sort cells deterministically by:
  1. original row index from snapped original `origY`,
  2. original `origX`,
  3. descending width as a congestion tie-breaker,
  4. input index.

Candidate generation:

- Snap the original global-placement coordinate to candidate legal row/site positions.
- Search rows in increasing vertical distance from the original row.
- For each searched row, inspect free intervals that can contain the cell.
- Generate a bounded set of candidate X positions per interval:
  - snapped original X clamped into the interval,
  - interval left boundary,
  - interval rightmost legal origin,
  - nearby sites around the clamped original X.
- Increase row radius if no candidate is found.
- Use a full-design fallback only after bounded local windows fail.

Candidate cost:

```text
dispCost = manhattan displacement in microns
densityCost = estimated local overflow pressure
score = alpha * dispCost + (1 - alpha) * densityCost
```

Tie-breaking:

1. Lower score.
2. Lower displacement.
3. Lower row distance.
4. Lower X.
5. Lower row index.

Commit:

- Verify the candidate through Row and Obstacle Model.
- Split or update row free intervals.
- Add the cell to the Density Grid Model.
- Store placed origin in the cell.

#### Dependencies

- Placement Data Model.
- Row and Obstacle Model.
- Density Grid Model.

#### Failure Handling

- If a cell has no legal position in the full fallback search, stop and report the cell name, size, and original coordinate.
- If density update fails after row commit, rollback the row commit and stop.
- All failures return nonzero process status.

#### Independent Test Plan

- Synthetic cases:
  - no obstacles, cells already legal;
  - overlapping cells in one row;
  - fixed macro splitting a row;
  - high-density region where density-aware mode moves cells away;
  - no local candidate requiring fallback expansion.
- Check every result with Validation Helpers.
- Expected command target if added:

```sh
make test_legalizer
```

#### Open Questions

- The exact candidate radius schedule and maximum number of candidate sites per interval are tuning parameters.
- The exact density-cost scaling relative to displacement is a tuning parameter because the PDF specifies final quality, not the internal heuristic.

### Local Refinement

#### Responsibility

Improve displacement and density after a complete legal placement exists while preserving legality. Refinement is optional for correctness but important for score quality.

It does not introduce new legality rules and must use the same Row and Obstacle Model commit/uncommit checks as initial legalization.

#### Inputs and Outputs

Inputs:

- Initial legal placement.
- Row occupancy.
- Density occupancy.
- `alpha`.

Outputs:

- Refined legal placement with no worse accepted local cost.

#### Internal Design

Use bounded passes so runtime remains predictable:

1. Single-cell relocation pass:
   - Visit cells with highest displacement or highest density contribution first.
   - Temporarily uncommit the cell.
   - Search a small neighborhood around its original row and current row.
   - Commit the best legal move only if total local cost improves.
2. Row-local compaction pass:
   - For each row interval, sort cells by X.
   - Try shifting cells toward original X within available whitespace without changing order.
   - Accept shifts that reduce displacement and preserve non-overlap.
3. Optional pair-swap pass:
   - Consider nearby cells of compatible height.
   - Swap locations only if both remain legal and combined cost improves.

Each pass has a maximum iteration count. If no pass makes changes, refinement stops early.

#### Dependencies

- Legalization Engine placement result.
- Row and Obstacle Model.
- Density Grid Model.
- Validation Helpers.

#### Failure Handling

- If a trial move cannot be committed, restore the original placement immediately.
- If validation fails after a pass, rollback the last accepted move if possible and fail loudly.

#### Independent Test Plan

- Start from hand-constructed legal placements and verify each pass preserves legality.
- Test that rejected moves restore original row and density occupancy.
- Test early stop when no moves improve cost.
- Expected command target if added:

```sh
make test_refine
```

#### Open Questions

- Which refinement passes should be enabled in the first implementation milestone remains a project scheduling choice. The minimum legal solution can ship without swaps, while score tuning can add them later.

### TCL Writer

#### Responsibility

Write the final placement script in the required OpenROAD TCL format.

It does not run OpenROAD, validate placement, or emit commands for fixed objects.

#### Inputs and Outputs

Inputs:

- Final `Design.cells`.
- `dbuPerMicron`.
- Output path.

Outputs:

- Output `.tcl` file with one command per movable cell:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {<xMicron> <yMicron>}
```

#### Internal Design

- Preserve a deterministic output order, preferably original input order.
- Convert DBU to microns using decimal output:

```text
micron = dbu / DBU_Per_Micron
```

- Use enough precision to avoid rounding a legal DBU origin to a visibly different micron coordinate. Six decimal digits is a reasonable default for DBU values used in the public data.
- Never write `detailed_placement`.

#### Dependencies

- Placement Data Model.

#### Failure Handling

- If the output file cannot be opened, fail with a diagnostic.
- If any cell has not been assigned a placement, fail before writing partial output.

#### Independent Test Plan

- Golden-file test for a small set of placed cells.
- Assert no output line contains `detailed_placement`.
- Assert macro and blockage names are absent.
- Expected command target if added:

```sh
make test_writer
```

#### Open Questions

- The PDF examples use compact decimal formatting. The exact decimal precision is not specified, so the writer should choose stable enough precision for DBU-to-micron conversion.

### Validation Helpers

#### Responsibility

Provide internal checks for final and intermediate placements. This module is a defensive layer before output and after refinement.

It does not replace OpenROAD `check_placement -verbose`, which remains the final external legality oracle.

#### Inputs and Outputs

Inputs:

- Design metadata.
- Final cell placements.
- Obstacles.
- Row/site dimensions.

Outputs:

- Pass/fail result.
- Diagnostic list for bounds, alignment, obstacle overlap, and cell overlap failures.

#### Internal Design

- Check every cell:
  - placement exists,
  - lower-left is inside die,
  - upper-right is inside die,
  - X is aligned to `dieLL.x + k * siteWidth`,
  - Y is aligned to `dieLL.y + k * siteHeight`.
- Check fixed obstacle overlap using row-index filtering to avoid full obstacle scans when possible.
- Check movable overlap row by row:
  - bucket cells by touched rows,
  - sort intervals by X in each row,
  - report adjacent interval overlap.
- Count cells and emitted commands to ensure every `CELL` is represented once.

#### Dependencies

- Placement Data Model.
- Row and Obstacle Model.

#### Failure Handling

- Validation failure stops TCL writing unless the caller is explicitly running a diagnostic-only mode.
- Diagnostics include cell names and coordinates.

#### Independent Test Plan

- Synthetic valid placement.
- One fixture per failure type:
  - out of bounds,
  - X misalignment,
  - Y misalignment,
  - obstacle overlap,
  - cell-cell overlap,
  - missing placement.
- Expected command target if added:

```sh
make test_validation
```

#### Open Questions

- No unresolved design questions.

## Cross-Module Contracts

- All internal geometry uses DBU integer coordinates.
- All rectangles use half-open bounds: `[llx, urx) x [lly, ury)`.
- Site alignment is relative to `DieArea_LL`, not absolute zero, because public data can have nonzero die lower-left coordinates.
- `CELL` records are the only movable objects.
- `MACRO` and `BLOCKAGE` records are fixed obstacles for placement legality.
- Macros are excluded from density-grid count; blockages are not explicitly excluded by the PDF and are treated only as placement obstacles.
- The Row and Obstacle Model is the only authority for legal placement and occupancy mutation.
- The Density Grid Model can influence cost but cannot legalize an otherwise illegal candidate.
- The TCL Writer receives only validated final placements.
- OpenROAD `check_placement -verbose` remains the external source of truth for final legality.

## Test Strategy

Module-level tests should be independently runnable wherever practical:

- `make test_parser`: parser syntax and error handling.
- `make test_model`: geometry, object splitting, snapping, and unit conversion.
- `make test_rows`: row generation, obstacle subtraction, and occupancy.
- `make test_density`: bin indexing, macro exclusion, utilization, and overflow.
- `make test_legalizer`: deterministic placement and fallback behavior on synthetic designs.
- `make test_refine`: legality-preserving local moves and rollback.
- `make test_writer`: TCL formatting and movable-only output.
- `make test_validation`: bounds, alignment, and overlap detection.

End-to-end validation should use the assignment flow:

1. Generate `.gp` files with `flow.tcl` and `extract.tcl`.
2. Build:

```sh
make
```

3. Run:

```sh
./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl
```

4. Source the generated TCL in OpenROAD after global placement.
5. Run `check_placement -verbose`.
6. Use `flow.tcl` to compute total displacement, average displacement, max displacement, DOR, normalized displacement, and final quality score.
7. Test at least one displacement-focused parameter profile and one DOR-focused parameter profile on both public benchmarks.

## Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Row derivation from die/site metadata misses irregular OpenROAD row details | Internally legal placement could fail OpenROAD on unusual hidden cases | Keep site alignment relative to die lower-left, validate with public extracted files, and treat this as an open input-format limitation |
| Obstacle interval snapping has off-by-one errors | Cells may overlap macros or blockages | Use half-open rectangles, integer DBU math, and focused row-subtraction tests |
| Greedy ordering blocks later large cells | Some cells get large displacement or no legal slot | Place wider/harder cells earlier in ties and use full-design fallback search |
| Density estimate differs from OpenROAD heatmap | DOR-focused quality may not improve as expected | Use density only as a heuristic and tune against `flow.tcl` results |
| Candidate search is too broad | Runtime may exceed 30 minutes | Use bounded local windows first, cap candidates per row, and expand only on failure |
| Refinement corrupts occupancy state | A legal initial placement becomes illegal | Route every trial through commit/uncommit APIs and validate after passes |
| Output precision rounds origins poorly | OpenROAD places cells at unintended coordinates | Emit enough decimal precision and keep DBU as the source of truth until writing |

## Open Questions

1. Student ID and final submission folder name are not specified.
2. The acceptable numeric range policy for `alpha` and `threshold` is not specified. The likely grading inputs are valid, but implementation should decide whether to reject out-of-range values or accept any finite values.
3. The exact internal density-cost scaling is not specified by the assignment. It should be tuned empirically against the provided OpenROAD flow.
4. The exact refinement pass set is a scheduling and tuning choice. A legal first implementation can start with single-cell relocation and add compaction/swaps later.
5. The `.gp` format does not include explicit DEF row orientation/count data. The design derives rows from die bounds and `Site_Height`; if hidden cases use irregular rows, the input format would need additional data.
