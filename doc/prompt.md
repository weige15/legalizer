# Vibe Coding Implementation Prompt

## Objective

Implement the C++17 OpenROAD placement legalizer described by this repository's planning docs. The final executable must be named `Legalizer` and support:

```sh
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

It must read an extracted `.gp` file, legalize every movable `CELL` instance inside the die, avoid `MACRO` and `BLOCKAGE` rectangles, snap placements to legal site rows and site columns, and write an OpenROAD TCL file containing one valid `place_cell` command per movable cell. The generated TCL must be safe to `source` from OpenROAD and must never emit `detailed_placement`.

Also harden the development OpenROAD flow so failures like:

```text
[ERROR GUI-0070] Error: flow.tcl, 33 child process exited abnormally
```

are prevented where possible and otherwise become actionable diagnostics. In particular, do not allow malformed `Legalizer` invocations, crashes, missing output files, invalid output TCL, or failed `source <output>.tcl` calls to collapse into an unexplained child-process failure.

## Inputs

Read these first:

- `doc/proposal.md`
- `doc/detailed-design.md`
- `doc/tasks/progress.md`
- Every module checklist in `doc/tasks/*.md`
- `Makefile`
- `README.md`
- `flow.tcl`
- `extract.tcl`
- Public LEF/DEF examples under `public/`

Reference PDFs are present for assignment and algorithm context:

- `p3_placement.pdf`
- `abacus.pdf`
- `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf`
- `openroad_tutorial.pdf`

Use the docs and current repository files as the source of truth. If the PDFs conflict with `flow.tcl` or the detailed design, prefer assignment legality and sourceable OpenROAD output, then document the local resolution in code comments or README notes where useful.

## Current Implementation

The repository is currently a scaffold rather than a complete implementation.

- `Makefile` expects:
  - `src/main.cpp`
  - `src/placement_model.cpp`
  - `src/gp_parser.cpp`
  - `src/row_interval_builder.cpp`
  - `src/density_estimator.cpp`
  - `src/legalizer.cpp`
  - `src/tcl_writer.cpp`
  - `tests/test_legalizer.cpp`
- `src/` and `tests/` are not currently present in the visible source tree, so create them and the matching headers/source files.
- `README.md` describes the intended executable, build command, run command, and test command.
- `Makefile` builds with `g++ -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc` and runs `make test`.
- `flow.tcl` currently:
  - sets `caseName` to `testcase/ispd19_sample`, while the visible sample files are under `public/`;
  - runs OpenROAD global placement;
  - sources `extract.tcl` to write `${design_name}_insts.gp`;
  - records original instance positions;
  - currently calls OpenROAD `detailed_placement` as a debug placeholder;
  - contains commented lines intended to run `make`, invoke `Legalizer`, and `source` the generated output TCL;
  - computes legality, displacement, heatmap, DOR, and quality metrics.
- `extract.tcl` writes the `.gp` format expected by the proposal:
  - metadata: `DBU_Per_Micron`, `DieArea_LL`, `DieArea_UR`, `Site_Width`, `Site_Height`;
  - instance header: `Name LLX LLY Width Height Type`;
  - instance types: `CELL`, `MACRO`, `BLOCKAGE`.
- A preexisting `Legalizer` binary may be present, but do not trust it as source. Implement the source tree required by `Makefile`.

The task checklist is fully unchecked. Implement the code and tests rather than merely updating checkboxes.

## Execution Model

Work autonomously to completion. The main agent owns overall progress, keeps `doc/tasks/progress.md` current as modules become truly complete, decomposes implementation into independent modules, spawns subagents for disjoint workstreams where useful, integrates their changes, and finishes with the repository's real quality gates passing.

When using worker agents, give them disjoint write scopes. Tell each worker they are not alone in the codebase, must not revert edits made by others, and must adapt to concurrent changes. The main agent remains responsible for integration, final behavior, and final verification.

Do not pause for human checkpoints unless blocked by a real ambiguity that cannot be resolved from the docs, source, assignment interface, or public fixtures. Make conservative implementation choices and document them.

## Module Plan

### Workstream 1: Placement Model and Geometry

Write scope:

- `src/placement_model.h`
- `src/placement_model.cpp`
- related tests in `tests/test_legalizer.cpp`

Implement DBU-based geometry types and helpers:

- `Rect` as half-open `[llx, urx) x [lly, ury)` using `long long`;
- movable `Cell` records with original and placed rectangles;
- fixed `Obstacle` records for `MACRO` and `BLOCKAGE`;
- `PlacementModel` metadata: `dbu_per_micron`, die bounds, site width, site height, cells, obstacles;
- overlap, containment, area, site alignment, row alignment, DBU-to-micron conversion, and multi-row-height detection helpers.

### Workstream 2: GP Parser and CLI Configuration

Write scope:

- `src/gp_parser.h`
- `src/gp_parser.cpp`
- `src/main.cpp`
- parser and CLI tests in `tests/test_legalizer.cpp`

Implement strict parsing for the `.gp` format emitted by `extract.tcl`. Reject unreadable files, missing metadata, malformed integers, nonpositive dimensions, invalid site dimensions, unknown instance types, duplicate names, and malformed headers.

Implement `main` to validate exactly four user arguments, parse finite numeric `alpha` and `threshold`, reject `alpha` outside `[0, 1]`, run the full pipeline, print concise diagnostics to `stderr`, and return nonzero on parse, legalization, validation, or write failure.

Important OpenROAD-flow requirement: `Legalizer` must not crash or leave behind misleading output on failure. Write output only after validation succeeds, or write to a temporary path and atomically replace the requested output when safe.

### Workstream 3: Row Interval Builder

Write scope:

- `src/row_interval_builder.h`
- `src/row_interval_builder.cpp`
- row interval tests in `tests/test_legalizer.cpp`

Build full-height site rows from the die and subtract every overlapping `MACRO` and `BLOCKAGE` x-span. Clip obstacles to the die, snap interval starts upward to legal site columns, snap interval ends downward to legal site boundaries, preserve row indexes even for empty rows, and fail early with a clear diagnostic when no legal interval exists.

### Workstream 4: Row Placement, Cell Ordering, and Legalization Engine

Write scope:

- `src/legalizer.h`
- `src/legalizer.cpp`
- legalization tests in `tests/test_legalizer.cpp`

Implement deterministic cell ordering and an ABACUS-inspired reversible row solver:

- forward order by original `llx`, original `lly`, name, and input index;
- reverse-x variant with deterministic ties;
- trial insertion into candidate row intervals without mutating committed state;
- cluster construction, overlap merging, interval clamping, and expansion to per-cell x locations;
- infeasible trials for cells wider than an interval or total interval occupancy over capacity;
- candidate row search around original y with a fallback full-row search;
- movement and density-aware scoring using `alpha` and `threshold`;
- forward and reverse legalization passes, keeping the legal pass with lower computed quality.

If hidden benchmarks contain multi-row-height cells, detect them. Either implement conservative multi-row occupancy or fail during development with a precise diagnostic naming the cell and dimensions. Never emit a final TCL placement that knowingly violates legality.

### Workstream 5: Density Estimator and Metrics

Write scope:

- `src/density_estimator.h`
- `src/density_estimator.cpp`
- metric tests in `tests/test_legalizer.cpp`

Implement 10um by 10um grid density handling:

- grid size is `10 * dbu_per_micron`;
- exact final DOR accumulates movable-cell overlap area into every touched grid;
- fixed macro-covered grids are excluded from the denominator;
- `threshold` is a percentage and comparison uses `density > threshold`, matching `flow.tcl`;
- provide a lightweight trial penalty API for legalization scoring.

Keep blockage exclusion isolated behind a helper or option if needed. Default to excluding fixed macros from DOR, since the docs and `flow.tcl` point there.

### Workstream 6: Legality Checker and TCL Writer

Write scope:

- `src/tcl_writer.h`
- `src/tcl_writer.cpp`
- checker types/functions in appropriate existing headers or `src/legalizer.*` if no separate checker file is added
- writer and checker tests in `tests/test_legalizer.cpp`

Validate final placements before writing:

- every movable cell has a placement;
- each cell is inside the die;
- x is site-column aligned;
- y is row aligned;
- cells do not overlap each other;
- cells do not overlap any `MACRO` or `BLOCKAGE`;
- each cell lies inside a legal row interval;
- average displacement is Manhattan distance in microns, matching `flow.tcl`.

Write TCL with deterministic original input order:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {<x_micron> <y_micron>}
```

Escape or reject instance names if necessary so output remains valid Tcl. Use enough decimal precision to preserve DBU-derived coordinates. The output must contain only placement commands and comments if helpful; it must not contain `detailed_placement`, shell commands, unbalanced braces, or any command that can fail due to missing variables.

### Workstream 7: Tests and OpenROAD Flow Hardening

Write scope:

- `tests/test_legalizer.cpp`
- small fixture files under `tests/`
- `flow.tcl`
- `README.md` if command documentation changes
- `doc/tasks/progress.md`

Create a lightweight test harness using standard C++ assertions or a simple local assertion helper. Do not add external dependencies.

Add fixtures for:

- one legal cell;
- obstacle splitting;
- initially overlapping cells;
- density overflow;
- malformed input;
- TCL writer formatting.

`make test` must build and run `tests/test_legalizer`, then smoke-test:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

Add tests that inspect generated TCL text and fail if it contains forbidden or unsafe content. At minimum verify:

- every nonblank command begins with `place_cell`;
- braces are balanced for `-origin {x y}`;
- no `detailed_placement`;
- no shell commands;
- no empty output when movable cells exist.

Harden `flow.tcl` so OpenROAD failures are readable:

- make `caseName` easy to override and default it to a visible public design path when possible, such as `public/ispd19_sample`;
- keep `detailed_placement` only as an optional debug fallback, not the normal legalizer path;
- run `exec make` and `exec timeout 30m ./Legalizer ...` inside `catch`;
- print the exact command, exit code, and stderr/stdout result when a child process fails;
- after `Legalizer` returns, check that the output TCL exists and is nonempty before `source`;
- source the output TCL inside `catch` and print a clear diagnostic if it fails;
- do not continue to `check_placement`, heatmap dumping, or quality scoring after a failed legalizer run or failed output source;
- keep the final submitted output TCL free of `detailed_placement`.

This flow hardening is specifically required to avoid opaque `[ERROR GUI-0070] ... child process exited abnormally` failures after `source flow.tcl`. The legalizer, writer, tests, and Tcl harness should make such failures either impossible for valid inputs or immediately explain what failed.

## Testing and Quality Gates

Run these commands before finishing:

```sh
make clean
make
make test
```

Because `Makefile` uses `$(RM)` only on individual known build products, do not replace it with batch deletion commands. Respect the repository instruction not to use recursive deletion commands such as `rm -rf`, `rmdir /s`, `Remove-Item -Recurse`, or similar.

If OpenROAD is installed in the environment, also run an optional development smoke test:

```sh
openroad -exit flow.tcl
```

or the local equivalent documented for this assignment. If OpenROAD is not installed, do not block completion; document that the OpenROAD smoke test could not be run and rely on `make test` plus generated-TCL syntax checks.

Keep runtime reasonable. The assignment target is below 30 minutes per benchmark, and public/tiny tests should complete quickly.

## Acceptance Criteria

The implementation is complete when:

- `src/` and `tests/` exist with the modules expected by `Makefile`;
- `make` builds `Legalizer` without warnings under the configured flags;
- `make test` passes;
- `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>` validates arguments, parses input, legalizes all movable cells, validates the final placement, and writes a sourceable TCL file;
- generated placements satisfy die containment, site alignment, row alignment, no cell-cell overlap, no macro/blockage overlap, and deterministic output ordering;
- generated TCL contains one `place_cell -inst_name ... -orient R0 -origin {...}` command per movable `CELL`;
- generated TCL never contains `detailed_placement`;
- density and displacement metrics are computed for development validation;
- `flow.tcl` no longer hides legalizer/build/source failures behind a bare child-process error;
- `doc/tasks/progress.md` accurately reflects completed modules;
- `README.md` remains accurate for build, run, test, and optional OpenROAD smoke workflows.

## Uncertainty Protocol

Make conservative assumptions unless blocked.

- If `.gp` formatting varies slightly from `extract.tcl`, tolerate harmless whitespace but reject structurally ambiguous input with a clear diagnostic.
- If hidden tests include multi-row-height cells and full support is too risky, fail clearly before writing output rather than producing illegal placements.
- If OpenROAD is unavailable, continue with unit tests and TCL syntax validation, and state that the OpenROAD smoke test was skipped.
- If assignment docs, helper scripts, and local tests disagree, prioritize legal placement, deterministic behavior, sourceable TCL, and clear diagnostics.
- Ask the user only for genuine blockers that cannot be resolved from the repository or assignment constraints.
