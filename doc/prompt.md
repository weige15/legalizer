# Vibe Coding Implementation Prompt

## Objective

Implement the Programming Assignment 3 placement legalizer end to end as a standalone C++17 executable named `Legalizer`.

The executable must support:

```sh
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

It must read an OpenROAD-extracted `.gp` placement file, legalize all supported movable single-row `CELL` instances onto legal site rows, avoid fixed `MACRO` and `BLOCKAGE` rectangles, preserve orientation, and write an OpenROAD TCL script containing explicit `place_cell` commands only. Legality is the first priority. Quality improvements must preserve legality.

The assignment quality metric is:

```text
Quality = alpha * AverageDisplacement + (1 - alpha) * DOR
```

where average displacement is Manhattan displacement in microns and DOR is the percentage of included 10 um by 10 um density grids whose standard-cell density is greater than `threshold`.

## Inputs

Read these project artifacts first:

- `doc/proposal.md`: project objective, assumptions, three-phase legalizer approach, metric and validation plan.
- `doc/high-level-design.md`: architecture, module list, contracts, confirmed design decisions, and requirements summary.
- `doc/detailed-design.md`: authoritative module-level behavior, data model sketches, algorithms, failure handling, and tests.
- `doc/tasks/progress.md`: full implementation checklist.
- `doc/tasks/*.md`: per-module task checklists and "Done When" criteria.
- `README.md`: expected user-facing build, run, test, and OpenROAD flow descriptions.
- `Makefile`: current build shape and quality-gate entry points.
- `extract_v2.tcl`: authoritative `.gp` extractor and input record format.
- `flow.tcl`: public OpenROAD validation scaffold and metric-reporting reference.
- `public/`: public benchmark collateral for manual or OpenROAD-backed smoke validation when OpenROAD is available.

Also honor the repository instruction: do not batch-delete files or directories. If deletion is needed, delete only one clearly named file at a time; if batch deletion is needed, stop and ask the user to do it manually.

## Current Implementation

The repository is currently docs-first. The `Makefile` expects these implementation files, but `src/` and `tests/` do not exist yet:

```text
src/main.cpp
src/placement_model.cpp
src/gp_parser.cpp
src/row_interval_builder.cpp
src/density_estimator.cpp
src/legalizer.cpp
src/tcl_writer.cpp
tests/test_legalizer.cpp
```

Create the necessary headers, source files, test fixtures, and directories. Keep the module layout compatible with the existing `Makefile`, updating it only when necessary to add headers, helper sources, fixtures, or stricter validation targets.

The current `Makefile` uses:

```make
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc
```

It defines:

```sh
make
make test
```

`make` must build `./Legalizer`. `make test` must build and run a lightweight C++ test binary plus a CLI smoke test.

`extract_v2.tcl` emits the `.gp` format:

```text
DBU_Per_Micron <int>
DieArea_LL <x> <y>
DieArea_UR <x> <y>
Site_Width <int>
Site_Height <int>

Name LLX LLY Width Height Orient Type
<name> <llx> <lly> <width> <height> <orient> CELL
<name> <llx> <lly> <width> <height> <orient> MACRO
<name> <llx> <lly> <width> <height> BLOCKAGE
```

`CELL` records are movable. `MACRO` and `BLOCKAGE` records are fixed obstacles for legality. `MACRO` records are excluded from the DOR denominator; `BLOCKAGE` records are placement obstacles but should not be excluded from DOR unless later OpenROAD comparison proves otherwise.

Important conflicts or caveats:

- `README.md` and `Makefile` describe `src/` and `tests/`, but those directories are absent. Treat this as a greenfield implementation using that intended layout.
- `flow.tcl` appears to be a tutorial/debug scaffold: it references `testcase/ispd19_sample`, sources `extract.tcl`, and calls `detailed_placement`. The implementation output must never contain `detailed_placement`. If you update `flow.tcl`, make it match the README/public-case flow and `extract_v2.tcl`; otherwise document it as a manual integration scaffold.
- `flow.tcl` contains a debug displacement normalization factor. The implementation metric should follow `doc/proposal.md` and `doc/detailed-design.md`: `alpha * averageDisplacementMicron + (1 - alpha) * dorPercent`.

## Execution Model

Run autonomously without human-in-the-loop checkpoints. The main agent owns overall progress, updates `doc/tasks/progress.md`, decomposes work by module, spawns worker subagents for independent modules where useful, reviews and integrates their changes, and keeps going until the executable, tests, and validation gates pass.

When spawning workers, give each worker a disjoint write scope. Tell every worker they are not alone in the codebase, must not revert edits made by others, and must adapt to concurrent changes. The main agent remains responsible for integration, API consistency, final validation, and resolving cross-module issues.

Prefer conservative, deterministic implementation choices that match the docs. If hidden benchmark behavior is unknown, prioritize legality, explicit diagnostics, and stable output over aggressive optimization.

## Module Plan

### Workstream 1: Placement Model

Owned files:

- `src/placement_model.h`
- `src/placement_model.cpp`
- related placement-model tests in `tests/test_legalizer.cpp`

Implement:

- `using Dbu = long long;`
- `Point`, `Rect`, `Cell`, `Obstacle`, `Tech`, `ObjectType`, `PlacementModel` or equivalent shared model types.
- Half-open rectangle overlap and containment helpers.
- `rectForCell`, DBU-to-micron conversion, site snapping, site alignment, row alignment, row-index helpers, supported-cell checks, and diagnostics for unsupported dimensions.
- Single-row movable-cell support only: `cell.height == tech.siteHeight`; reject multi-row movable cells clearly.
- Site-compatible widths; reject or fail clearly on non-site-multiple movable widths instead of silently producing fractional placement.

### Workstream 2: GP Parser

Owned files:

- `src/gp_parser.h`
- `src/gp_parser.cpp`
- parser fixtures under `tests/fixture_*.gp`
- parser tests in `tests/test_legalizer.cpp`

Implement:

- Required technology headers exactly once.
- Seven-field `CELL` and `MACRO` records with orientation.
- Six-field `BLOCKAGE` records without orientation.
- Positive DBU dimensions and valid die/site values.
- Deterministic object order preservation.
- Line-numbered diagnostics for malformed records, missing headers, bad integers, nonpositive dimensions, and unknown object types.

### Workstream 3: Row Interval Builder

Owned files:

- `src/row_interval_builder.h`
- `src/row_interval_builder.cpp`
- row interval tests in `tests/test_legalizer.cpp`

Implement:

- Generate rows from `die.ly + k * siteHeight` while `rowY + siteHeight <= die.uy`.
- Split each row into obstacle-free, site-aligned `[xMin, xMax)` intervals.
- Project overlapping `MACRO` and `BLOCKAGE` rectangles onto row bands using half-open geometry.
- Clip obstacle spans to the die and snap blocked spans outward to site boundaries.
- Store row index, row y, interval bounds, occupied width, and assigned cell IDs.
- Keep intervals sorted and deterministic.

### Workstream 4: Abacus Interval Solver

Owned files:

- Abacus declarations in `src/legalizer.h` or a dedicated `src/abacus_solver.h`
- implementation in `src/legalizer.cpp` or `src/abacus_solver.cpp`
- Abacus tests in `tests/test_legalizer.cpp`

Implement:

- Pure candidate solver for one interval and ordered cell IDs.
- Cluster construction, overlap detection, merge, target recomputation, clamping, site snapping, and expansion into non-overlapping x positions.
- Uniform weights.
- Failure when total width exceeds interval width or snapping cannot keep placements in bounds.
- Return proposed x positions and row-local displacement cost without mutating global placement state.

If adding a new `.cpp` file, update `MODULE_SRCS` in `Makefile`.

### Workstream 5: Tetris Fallback

Owned files:

- fallback declarations in `src/legalizer.h` or dedicated helper headers
- fallback implementation in `src/legalizer.cpp`
- fallback tests in `tests/test_legalizer.cpp`

Implement:

- Enumerate occupied spans in candidate intervals.
- Derive legal gaps between interval bounds and occupied spans.
- Snap preferred x to the nearest legal site inside each fitting gap.
- Score by Manhattan displacement and deterministic tie-breakers.
- Commit exactly one legal placement and update interval cell order and occupancy.
- Report no-fit failure clearly.

### Workstream 6: Baseline Legalizer

Owned files:

- `src/legalizer.h`
- `src/legalizer.cpp`
- baseline legalization tests in `tests/test_legalizer.cpp`

Implement:

- Deterministic cell processing order, preferably `(original.x, original.y, name)` unless a better documented row-bucket order is implemented.
- Candidate interval search from nearest original row outward, using remaining capacity and bounded radius before expanding if needed.
- Trial insertion on copied interval state, solved by Abacus.
- Candidate scoring by displacement delta with deterministic tie-breakers.
- Commit only the selected candidate.
- Use Tetris fallback if Abacus trials fail.
- Never leave partially committed state after failed trials.

### Workstream 7: Density / Metric Evaluator

Owned files:

- `src/density_estimator.h`
- `src/density_estimator.cpp`
- metric tests in `tests/test_legalizer.cpp`

Implement:

- Average Manhattan displacement in microns.
- 10 um by 10 um DBU grid construction over the die.
- Macro-covered grid exclusion from the denominator.
- Movable-cell overlap area accumulation per included grid.
- Overflow count where `density > threshold`; equality is not overflow.
- DOR percent and weighted quality.
- Affected-grid helpers or exact recomputation suitable for local repair.
- Zero-denominator behavior: report DOR as `0` and total grids as `0`.

### Workstream 8: DOR-Aware Local Repair

Owned files:

- repair implementation in `src/legalizer.cpp` or a dedicated module if added to the Makefile
- repair tests in `tests/test_legalizer.cpp`

Implement:

- Compute current metrics and rank overflow grids by density.
- Rank contributing cells by area and displacement penalty.
- Try bounded reinsertion, gap-move, swap, or small-window reorder candidates.
- Snapshot affected intervals and cell placements.
- Accept only candidates that improve weighted quality, or a documented low-alpha DOR improvement with bounded displacement penalty.
- Roll back every rejected candidate completely.
- Stop after fixed iteration limits or a full pass with no accepted moves.

Keep the first implementation modest if needed, but include the architecture and tests necessary to prove repair never makes a legal baseline illegal.

### Workstream 9: Legality Validator

Owned files:

- validator declarations in `src/legalizer.h` or a dedicated header
- validator implementation in `src/legalizer.cpp` or dedicated module
- validator tests in `tests/test_legalizer.cpp`

Implement checks for:

- Every movable cell has exactly one valid placement.
- Die containment.
- Site alignment.
- Row alignment.
- Supported single-row height.
- Orientation preservation.
- Movable-vs-movable non-overlap.
- Movable-vs-`MACRO` and movable-vs-`BLOCKAGE` non-overlap.
- Placement contained in one legal row interval.

Return multiple diagnostics where practical. The CLI should print concise diagnostics and a nonzero exit code on failure.

### Workstream 10: TCL Writer

Owned files:

- `src/tcl_writer.h`
- `src/tcl_writer.cpp`
- writer tests in `tests/test_legalizer.cpp`

Implement:

```tcl
place_cell -inst_name <instName> -orient <orient> -origin {<xMicron> <yMicron>}
```

Requirements:

- Preserve parsed orientation.
- Convert DBU to microns with fixed decimal precision, for example six decimal digits.
- Preserve parser order.
- Emit movable cells only.
- Never emit `detailed_placement`.
- Write to a same-directory temporary file, flush and close successfully, then rename to the requested output path.

### Workstream 11: CLI / Main

Owned files:

- `src/main.cpp`
- CLI fixtures/tests in `tests/test_legalizer.cpp`

Implement:

- Parse exactly four positional arguments after the program name.
- Validate finite `alpha` in `[0, 1]` and finite `threshold`.
- Run parser, model invariant checks, unsupported-cell checks, row interval build, baseline legalization, metric evaluation, DOR repair, final legality validation, and TCL writing in that order.
- Fail before writing output if any pre-output stage fails.
- Print concise diagnostics for malformed input, unsupported design content, legalization failure, validation failure, and output failure.

### Workstream 12: Tests and Fixtures

Owned files:

- `tests/test_legalizer.cpp`
- `tests/fixture_*.gp`
- optional test helper headers under `tests/`
- `Makefile` test target updates if needed

Implement a lightweight test harness without external dependencies. Group tests by module. Failures should print the test name plus expected and actual values where useful.

Cover at least:

- Parser: valid cells/macros/blockages, missing headers, bad object types, bad dimensions, missing orientation.
- Placement model: edge-touch non-overlap, containment, snapping with nonzero die origin, row bounds, micron conversion, unsupported cell dimensions.
- Row intervals: no obstacles, outside obstacles, middle split, boundary touch, full-row coverage, multi-row obstacle, row-boundary contact.
- Abacus: empty input, one-cell clamping, two-cell merge, chain merge, exact fit, over-capacity failure, nonzero interval origin.
- Tetris fallback: empty intervals, before/between/after placement, site snapping, tie-breaking, no-fit failure.
- Baseline legalizer: nearest-row placement, overlapping originals, macro-forced splits, farther-row selection, fallback placement, insufficient capacity failure.
- Validator: missing placement, out-of-die, off-site, off-row, movable overlap, obstacle overlap, outside interval, valid placement.
- Metrics: single-grid and multi-grid cells, macro exclusion, threshold equality, empty placement, alpha edge cases.
- Writer: one-cell formatting, orientation, coordinate conversion, deterministic order, forbidden command absence.
- CLI: success smoke, bad argument count, invalid numbers, missing input, unsupported multi-row cell.

## Testing and Quality Gates

At minimum, the final repository must pass:

```sh
make
make test
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

Also run targeted compile or smoke commands as needed while developing. Treat all warnings from `-Wall -Wextra -pedantic` as issues to fix.

If OpenROAD is available, run a public-case integration smoke using `public/ispd15_mgc_matrix_mult_a` and `public/ispd19_sample`. If `flow.tcl` is used, first make sure it references `extract_v2.tcl`, the public case paths, and the generated `Legalizer` output, and does not use OpenROAD `detailed_placement` as a substitute for this legalizer. If OpenROAD is not available, state that limitation in the final report and rely on the unit and CLI gates.

No external test framework is required. Do not introduce network dependency or package-manager dependency unless absolutely necessary.

## Acceptance Criteria

The implementation is complete when:

- `src/` and `tests/` exist with a coherent C++17 implementation matching the module plan.
- `make` builds `./Legalizer` from the repository root.
- `make test` builds and runs `tests/test_legalizer`, then runs the CLI smoke test.
- The CLI interface is exactly `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>`.
- Valid single-row movable cells are placed legally inside the die, on site x positions and legal row y positions.
- Fixed `MACRO` and `BLOCKAGE` rectangles are avoided.
- Movable cells do not overlap one another.
- Multi-row movable cells and non-site-compatible cells are rejected with clear diagnostics.
- Final validation runs before writing TCL.
- Output TCL contains deterministic `place_cell` commands for movable cells only, preserves orientation, uses micron origins, and never contains `detailed_placement`.
- Average displacement, DOR, and weighted quality are computed according to the project docs.
- DOR repair never makes the placement illegal and rolls back rejected candidates.
- Tests cover all module-level behavior listed above.
- `doc/tasks/progress.md` is updated as modules are completed.
- Any updated documentation accurately reflects the final build, run, test, and OpenROAD smoke workflow.

## Uncertainty Protocol

Make conservative assumptions unless blocked. Legality, determinism, and clear diagnostics outrank quality tuning.

Known decisions that are already settled:

- Support single-row movable cells only.
- Reject multi-row movable cells with a named diagnostic.
- Include DOR-aware repair in the architecture.
- Use fixed decimal micron precision in TCL output.
- Treat `extract_v2.tcl` as the authoritative `.gp` format source.
- Exclude macro-covered grids from DOR; do not exclude blockage grids unless OpenROAD evidence requires a change.

Known non-blocking uncertainty:

- Hidden benchmarks may include multi-row movable cells. The current project decision is still to reject them clearly.
- `flow.tcl` may require cleanup before it can be used as an integration gate. Do not let that block `make`, `make test`, or CLI completion.
- Public scoring may normalize displacement externally. Implement the metric from the docs and optionally log flow-compatible comparison values only as extra diagnostics.

If a true blocker appears that cannot be resolved from the docs or code, ask one concise question before implementation. Otherwise continue autonomously through implementation, integration, testing, and final reporting.
