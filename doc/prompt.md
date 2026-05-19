# Vibe Coding Implementation Prompt

## Objective

Implement the Programming Assignment #3 placement legalizer end to end as a deterministic Linux C++17 program named `Legalizer`. The program must read the `.gp` file emitted by `extract.tcl`, legalize every movable `CELL` instance onto legal site rows while avoiding fixed `MACRO` and `BLOCKAGE` rectangles, and write an OpenROAD TCL file containing one direct `place_cell` command per movable cell.

Legality is the first priority. After legality is satisfied, optimize the assignment metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of 10 micron by 10 micron heatmap grids whose density exceeds the command-line `threshold`. `flow.tcl` uses `norm_factor = 18.2`, treats `threshold = 45` as a percentage value, and computes DOR from column 5 of the OpenROAD placement heatmap CSV after removing macro instances named like `h*`.

## Inputs

Read these project artifacts first:

- `doc/proposal.md`: overall algorithm, assignment constraints, validation plan, and quality target.
- `doc/detailed-design.md`: module contracts, data flow, test strategy, risks, and open questions.
- `doc/high-level-design.md`: architecture overview and cross-module relationships.
- `doc/tasks/progress.md`: full module checklist.
- `doc/tasks/*.md`: executable task lists for each module.
- `README.md`: intended build, run, test, and source layout.
- `Makefile`: actual configured build and test commands.
- `extract.tcl`: exact `.gp` generation format.
- `flow.tcl`: assignment-facing OpenROAD validation, displacement, DOR, and quality scoring behavior.
- `public/ispd19_sample/*` and `public/ispd15_mgc_matrix_mult_a/*`: public LEF/DEF benchmark inputs.
- Reference PDFs in the repository root: `p3_placement.pdf`, `abacus.pdf`, and `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf` when algorithm details are needed.

## Current Implementation

The repository currently contains planning docs, benchmark data, OpenROAD helper scripts, and a `Makefile`, but no visible `src/` or `tests/` directories. Treat implementation as a C++17 source reconstruction that must satisfy the existing `Makefile` intent.

The current `Makefile` expects:

```text
src/main.cpp
src/placement_model.cpp
src/gp_parser.cpp
src/row_interval_builder.cpp
src/density_estimator.cpp
src/legalizer.cpp
src/tcl_writer.cpp
tests/test_legalizer.cpp
tests/fixture_one_cell.gp
```

It builds with:

```sh
make
```

and tests with:

```sh
make test
```

The `test` target runs:

```sh
./tests/test_legalizer
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

Preserve the assignment CLI:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

The `.gp` input from `extract.tcl` has this shape:

```text
DBU_Per_Micron <int>
DieArea_LL <x> <y>
DieArea_UR <x> <y>
Site_Width <int>
Site_Height <int>

Name LLX LLY Width Height Type
<name> <llx> <lly> <width> <height> <CELL|MACRO|BLOCKAGE>
```

`CELL` records are movable. `MACRO` and `BLOCKAGE` records are fixed placement obstacles. Coordinates and sizes are DBU integers. The generated TCL must contain only direct placement commands:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

Origins must be lower-left coordinates converted from DBU to microns. Do not emit `detailed_placement`.

## Execution Model

Work autonomously until the full implementation is complete. The main agent owns overall progress tracking, integration, final quality gates, and updates to `doc/tasks/progress.md`. Decompose the implementation into modules and spawn worker subagents for independent modules where useful.

Workers are not alone in the codebase. Each worker must stay inside its assigned write scope, must not revert edits made by others, and must adapt its implementation to concurrent changes. The main agent integrates workers' results, resolves interface mismatches, adds missing coverage, and runs the final build and test gates without human-in-the-loop checkpoints.

Make conservative implementation choices when docs leave tuning details open. Ask the user only if implementation is truly blocked by missing requirements that cannot be inferred from the docs, code, or assignment scripts.

## Module Plan

### Workstream 1: Placement Model and Geometry

Own these files:

- `src/placement_model.h`
- `src/placement_model.cpp`
- geometry-related sections of `tests/test_legalizer.cpp`

Implement DBU-based types and helpers:

- `Rect` as half-open `[lx, ux) x [ly, uy)`.
- `InstanceType` for `CELL`, `MACRO`, and `BLOCKAGE`.
- movable cell records preserving original input order.
- fixed obstacle records for macros and blockages.
- `Design` metadata: DBU per micron, die rectangle, site width, site height.
- explicit unset state for legal coordinates before legalization.
- helpers for width, height, area, overlap, containment, center, Manhattan displacement, site alignment, and final rectangle construction.

Reject invalid metadata, non-positive die dimensions, and non-positive instance dimensions. Use signed 64-bit integers for legality-sensitive coordinates and areas.

### Workstream 2: GP Parser and CLI

Own these files:

- `src/gp_parser.h`
- `src/gp_parser.cpp`
- `src/main.cpp`
- parser and CLI tests in `tests/test_legalizer.cpp`
- parser fixtures under `tests/`

Implement strict `.gp` parsing from `extract.tcl`:

- parse all required metadata fields before the instance table.
- require the `Name LLX LLY Width Height Type` header.
- tolerate optional blank lines around the header.
- preserve instance names exactly as input tokens.
- classify `CELL` as movable and `MACRO`/`BLOCKAGE` as fixed.
- fail with line-numbered diagnostics for malformed metadata, missing metadata, unknown types, short rows, non-integer fields, or invalid dimensions.

Implement the top-level CLI:

- require exactly four user arguments after the program name.
- parse `alpha` and `threshold` as finite `double` values.
- call parser, row segment builder, legalizer, legality checker, and TCL writer in order.
- return non-zero with concise `stderr` diagnostics on failure.
- avoid leaving a partial output TCL after parse, legalize, check, or write failure.

### Workstream 3: Row Segment Builder and Legality Checker

Own these files:

- `src/row_interval_builder.h`
- `src/row_interval_builder.cpp`
- legality-checker declarations/implementation, either in `src/legalizer.{h,cpp}` or a small dedicated pair if the `Makefile` is updated consistently.
- row and legality tests in `tests/test_legalizer.cpp`

Build legal rows from:

```text
row_y = DieArea_LL.y + row_index * Site_Height
```

Start each row with the full die X interval, subtract clipped X projections of every fixed obstacle whose half-open Y span intersects the row, merge blocked intervals, and snap free segment bounds inward to the site grid from `DieArea_LL.x`. Preserve rows with empty segment lists.

Implement final legality checks:

- every movable cell has legal coordinates.
- every movable rectangle is inside the die.
- lower-left X is site-aligned.
- lower-left Y is a legal row Y.
- movable cells do not overlap fixed macros or blockages.
- movable cells do not overlap one another.

Use row grouping or sweep-line sorting for movable overlap instead of a naive global quadratic check on large designs.

### Workstream 4: Abacus Row Engine

Own the row-engine portions of:

- `src/legalizer.h`
- `src/legalizer.cpp`
- Abacus tests in `tests/test_legalizer.cpp`

Implement a non-mutating trial engine for ordered single-row cells inside one row segment:

- preserve original-X order with input-index tie breaks.
- detect capacity infeasibility before committing.
- create Abacus-style clusters.
- merge overlapping clusters recursively.
- compute cluster target positions from equal weights unless benchmark tuning proves another weighting works better.
- clamp clusters to segment bounds.
- expand clusters into non-overlapping site-aligned cell origins.
- support explicit commit only after the Hybrid Legalizer selects a candidate.

Tests must cover non-overlapping placement, forced cluster merging, left and right clamping, non-mutating candidate insertion, site alignment, infeasible capacity, and deterministic tie breaking.

### Workstream 5: Density Estimator

Own these files:

- `src/density_estimator.h`
- `src/density_estimator.cpp`
- density tests in `tests/test_legalizer.cpp`

Implement a 10 micron grid:

```text
grid_size_dbu = 10 * DBU_Per_Micron
```

Track movable occupied area per grid with integer rectangle-grid intersection. Support committed add/remove operations and candidate delta queries. Compute estimated DOR against the CLI threshold as a percentage, matching `flow.tcl` behavior for values like `45`.

For DOR counting, follow `flow.tcl` as closely as possible. Document the chosen fixed-obstacle exclusion policy in tests. The proposal says fixed macro regions should be excluded where practical; `flow.tcl` removes macro instances before dumping heatmap density and does not explicitly remove blockages from the grid count.

### Workstream 6: Multi-Row Placement Layer

Own multi-row portions of:

- `src/legalizer.h`
- `src/legalizer.cpp`
- multi-row tests in `tests/test_legalizer.cpp`

Detect tall movable cells with:

```text
row_span = ceil(cell.height / Site_Height)
```

Bypass this layer for one-row cells. For tall cells, enumerate consecutive row spans near the original Y first, intersect free X intervals across all covered rows, reject common intervals too narrow for the cell, and evaluate candidate X positions near original X with deterministic tie breaks.

Because `.gp` does not include rail phase or legal orientation metadata, enforce pure geometric row-span legality with `R0` orientation. If local insertion around the target position fails, expand the search, then fall back to a full row-span segment search before declaring infeasible. Repack affected single-row neighbors through row-engine trials where needed.

### Workstream 7: Hybrid Legalizer and TCL Writer

Own these files:

- remaining portions of `src/legalizer.h`
- remaining portions of `src/legalizer.cpp`
- `src/tcl_writer.h`
- `src/tcl_writer.cpp`
- end-to-end and writer tests in `tests/test_legalizer.cpp`

Implement deterministic legalization trials:

- increasing original X, then original Y, then input index.
- decreasing original X, then original Y, then input index.
- large-area or tall cells first, then increasing original X.

For each cell, enumerate candidate rows or row spans by increasing vertical displacement. For each feasible segment, ask the Abacus Row Engine or Multi-Row Placement Layer for a trial placement. Score candidates with:

```text
score =
  alpha * normalized_displacement_delta
  + (1 - alpha) * estimated_DOR_delta
  + deterministic_tie_breakers
```

Use `flow.tcl`'s `norm_factor = 18.2` as the initial displacement normalization reference. Broaden local search to all feasible rows or row spans before declaring trial failure. Keep the best completed trial.

After the best legal trial, run a bounded density smoothing pass:

- identify worst estimated overflow grids.
- select movable cells contributing to those grids.
- try nearby lower-density rows or segments.
- accept only moves that preserve legality and improve the estimated assignment-like score.
- stop at a fixed iteration count, no-improvement count, or time budget that cannot threaten the 30 minute benchmark timeout.

The TCL writer must:

- emit one line per movable `CELL` in original input order.
- convert DBU lower-left origins to microns using `DBU_Per_Micron`.
- emit `-orient R0` on every line.
- never emit comments or `detailed_placement`.
- fail if any legal coordinate is missing.

### Workstream 8: Tests, Fixtures, Bench Harness, and Documentation

Own:

- `tests/test_legalizer.cpp`
- all `tests/fixture_*.gp` fixtures.
- any benchmark notes or small documentation updates.
- `doc/tasks/progress.md` status updates.

Create synthetic fixtures for parser, geometry, row construction, Abacus, density, writer, and end-to-end legalization. Ensure `make test` does not require OpenROAD.

Add or preserve `tests/fixture_one_cell.gp` so the existing `Makefile` target succeeds. The test executable should be self-contained and should fail with clear module-oriented messages.

Document an optional OpenROAD validation sequence for public benchmarks using `flow.tcl` once OpenROAD is available. Record, when available, legality, average displacement, DOR, quality, and runtime for at least one high-alpha and one low-alpha or stricter-threshold run.

## Testing and Quality Gates

The final implementation must pass:

```sh
make
make test
```

Also run at least one direct assignment-style fixture command:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

Inspect the generated fixture TCL and verify:

- one `place_cell` command per movable cell.
- every line uses `-orient R0`.
- no line contains `detailed_placement`.
- coordinates are in microns, not DBU.

When OpenROAD is available, validate public benchmarks through `flow.tcl` or an equivalent command sequence:

- generate `.gp` from `extract.tcl`.
- run `timeout 30m ./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`.
- source the generated TCL.
- run `check_placement -verbose`.
- dump the placement heatmap.
- report total displacement, average displacement, max displacement, DOR, normalized displacement, final quality score, and runtime.

Use configured compiler warnings from the `Makefile` as quality gates:

```text
-std=c++17 -O2 -Wall -Wextra -pedantic -Isrc
```

If adding new source files, update the `Makefile` deliberately and keep `make` and `make test` as the canonical local gates. There is no configured formatter, linter, type checker, or static analyzer beyond the C++ compiler warnings; do not invent mandatory external dependencies unless the repository is updated to use them.

## Acceptance Criteria

The work is complete when:

- `src/` and `tests/` exist and match the build layout.
- `make` builds `./Legalizer`.
- `make test` builds and runs `tests/test_legalizer` and the one-cell assignment-style fixture.
- `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl` accepts the required four-argument CLI.
- valid `.gp` files are parsed into DBU-based model data.
- invalid `.gp` files fail deterministically with useful diagnostics.
- all movable `CELL` instances receive legal final coordinates when a legal solution exists in test fixtures.
- final placements are inside the die, row/site aligned, non-overlapping with fixed obstacles, and non-overlapping with other movable cells.
- generated TCL contains exactly one direct `place_cell` command per movable cell in deterministic input order.
- generated TCL uses `-orient R0`, micron coordinates, and no `detailed_placement`.
- density estimation supports candidate scoring and bounded smoothing.
- tall cells are detected; geometric multi-row placement is implemented or clear infeasibility is reported.
- all module task checkboxes in `doc/tasks/*.md` and `doc/tasks/progress.md` are updated accurately.
- public benchmark validation is documented, and benchmark results are recorded when OpenROAD is available.

## Uncertainty Protocol

No user clarification is required before starting implementation. The remaining uncertainties are tuning and scorer-calibration issues, not blockers:

- Hidden benchmarks may or may not contain movable cells taller than `Site_Height`; implement detection and geometric support.
- Exact density behavior can differ between the estimator and OpenROAD heatmap; use the estimator for relative candidate choices and trust `flow.tcl` for final benchmark scoring.
- Use threshold values as percentages, matching `flow.tcl` (`45` means density greater than 45).
- Use `norm_factor = 18.2` from `flow.tcl` as the initial displacement normalization constant.
- Use equal Abacus cluster weights initially unless public benchmark tuning clearly favors another deterministic weighting.
- Choose conservative bounded smoothing budgets that cannot risk the 30 minute timeout.

If a future agent becomes truly blocked, ask concise questions before implementation. Otherwise, make conservative assumptions, document them in code comments or tests where useful, and continue through final build and test verification.
