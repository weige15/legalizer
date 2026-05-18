# Vibe Coding Prompt: Placement Legalizer for OpenROAD

## Objective

Implement the full C++17 solution for Programming Assignment #3, "Placement with OpenROAD," in this repository. The final project must build a root-level `Legalizer` executable with `make`, run as:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

and produce an OpenROAD TCL script with direct `place_cell` commands that legalize all movable standard cells. The placement must keep cells inside the die, aligned to site rows, non-overlapping with cells/macros/blockages, and competitive on the assignment quality metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

The generated TCL must never invoke `detailed_placement`, and every emitted placement must keep orientation `R0`.

## Inputs

Read these files first, in this order:

- `p3_placement.pdf`: official assignment specification, including input format, output format, CLI contract, grading policy, and prohibited commands.
- `doc/proposal.md`: project objective, assumptions, proposed approach, validation plan.
- `doc/high-level-design.md`: architecture, module boundaries, data flow, contracts.
- `doc/detailed-design.md`: detailed module designs, cross-module contracts, test strategy, risks, and open questions.
- `doc/tasks/progress.md`: overall module checklist.
- `doc/tasks/cli-driver.md`
- `doc/tasks/gp-parser.md`
- `doc/tasks/placement-model.md`
- `doc/tasks/row-interval-builder.md`
- `doc/tasks/legalizer.md`
- `doc/tasks/density-estimator.md`
- `doc/tasks/tcl-writer.md`
- `doc/tasks/test-fixtures.md`
- `README.md`: expected build, run, and test commands.
- `extract.tcl`: authoritative `.gp` extraction format used by OpenROAD.
- `flow.tcl`: public validation flow and scoring script.
- `public/ispd19_sample/` and `public/ispd15_mgc_matrix_mult_a/`: public LEF/DEF benchmarks for final OpenROAD validation.

## Current Implementation

The checkout currently contains planning docs, assignment/support files, public benchmarks, and an existing root-level `Legalizer` binary, but it does not contain the source tree or `Makefile` described by `README.md`. Treat the binary as an old build artifact only. Reconstruct the source project from the planning docs.

Observed repository state:

- `p3_placement.pdf` defines the assignment due May 25, 2026.
- `extract.tcl` emits `.gp` files with:
  - `DBU_Per_Micron <integer>`
  - `DieArea_LL <x> <y>`
  - `DieArea_UR <x> <y>`
  - `Site_Width <integer>`
  - `Site_Height <integer>`
  - blank line
  - `Name LLX LLY Width Height Type`
  - instance rows where `CELL` is movable, `MACRO` is fixed, and `BLOCKAGE` is fixed.
- `flow.tcl` currently demonstrates the validation/scoring flow. It performs global placement, sources `extract.tcl`, records original locations, then should be adapted during local validation to run `make`, execute `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`, source the generated TCL, run `check_placement -verbose`, and compute displacement/DOR.
- `README.md` expects:
  - `make`
  - `./Legalizer <alpha> <threshold> <input.gp> <output.tcl>`
  - `make test`
- `.gitignore` already ignores `Legalizer`, objects, test binaries, generated `.gp`, logs, reports, and local cache directories.
- There is no current `src/`, `tests/`, or `Makefile`; create them.

Assignment constraints to preserve exactly:

- Language/platform: C or C++ preferred; use C++17 on Linux.
- TA command:

  ```sh
  make
  ./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
  ```

- Output format:

  ```tcl
  place_cell -inst_name <instName> -orient R0 -origin {X Y}
  ```

- Output coordinates are in microns; internal geometry should remain in DBU until TCL writing.
- No cell rotation.
- No `detailed_placement` in generated output TCL.
- Runtime must stay below 30 minutes per benchmark.
- DOR uses 10 micron by 10 micron grids; grids occupied by fixed macros are excluded from the grading grid count.
- `flow.tcl` uses `norm_factor 18.2` when calculating normalized displacement for quality.

Important workspace constraint:

- Do not delete files or directories in batch. Do not use `rm -rf`, `rmdir /s`, `Remove-Item -Recurse`, `rd /s`, or `del /s`. If cleanup is needed, delete only one explicit file path at a time, or ask the user to delete batch artifacts manually.

## Execution Model

Operate autonomously and finish the implementation end to end. The main agent owns overall progress, updates the task checklists, decomposes work by module, spawns worker subagents where independent implementation slices are useful, integrates their changes, resolves build/test issues, and completes the repository without human-in-the-loop checkpoints unless truly blocked.

When spawning worker agents, give them disjoint write scopes. Tell every worker that they are not alone in the codebase, must not revert edits made by others, and must adapt to concurrent changes. The main agent remains responsible for final integration, quality gates, and consistency.

Use conservative engineering judgment when details are ambiguous. Prefer legal, deterministic placements over risky quality tuning. Ask a concise question only if implementation cannot safely proceed from the assignment docs and repository facts.

## Module Plan

### Workstream 1: Build System and CLI Driver

Owned files:

- `Makefile`
- `src/main.cpp`
- `src/cli.*` if a separate driver abstraction is useful
- related CLI tests in `tests/`

Implement:

- C++17 build that creates `./Legalizer` in the repository root.
- `make test` target that builds and runs `tests/test_legalizer`.
- Exact CLI argument contract: four user arguments, `alpha`, `threshold`, input path, output path.
- Full-string numeric parsing for `alpha` and `threshold`.
- Non-zero exits with concise `stderr` diagnostics for bad arguments, parse failures, legalization failures, and output failures.
- Orchestration: parse GP, validate model, build rows, create density estimator, legalize, write TCL.

### Workstream 2: Placement Model and Geometry

Owned files:

- `src/placement_model.hpp`
- `src/placement_model.cpp`
- model/geometry tests in `tests/`

Implement:

- `Rect`, `InstanceType`, `Cell`, `Obstacle`, `Design`, final placement state, and shared legal row types.
- Signed 64-bit DBU coordinates.
- Half-open rectangle convention: `[x_min, x_max) x [y_min, y_max)`.
- Helpers for width, height, intersection, containment, clipping, site snapping, row index/Y conversion, and displacement.
- Model validation for die/site dimensions, duplicate movable names, invalid dimensions, and unsupported cell heights.
- Support standard single-height cells first. If multi-height movable cells appear, either extend row occupancy across all covered rows or fail explicitly with a clear unsupported-cell diagnostic. Prefer implementing robust multi-row occupancy if feasible without destabilizing the solution.

### Workstream 3: GP Parser

Owned files:

- `src/gp_parser.hpp`
- `src/gp_parser.cpp`
- parser fixtures/tests in `tests/`

Implement:

- Strict parsing of `DBU_Per_Micron`, `DieArea_LL`, `DieArea_UR`, `Site_Width`, and `Site_Height`.
- Accept blank lines before the `Name LLX LLY Width Height Type` header.
- Parse `CELL` as movable and `MACRO`/`BLOCKAGE` as fixed obstacles.
- Preserve movable-cell input order for deterministic TCL output.
- Reject malformed integers, missing metadata/header, non-positive dimensions, and unknown instance types with line context.
- Allow obstacle coordinates outside the die; row interval construction will clip them.

### Workstream 4: Row Interval Builder

Owned files:

- `src/row_interval_builder.hpp`
- `src/row_interval_builder.cpp`
- row interval tests in `tests/`

Implement:

- Generate site rows from `die.y_min` to `die.y_max` by `site_height`.
- Initialize each row with legal X capacity inside the die.
- Clip every fixed `MACRO` and `BLOCKAGE` to the die.
- For each obstacle intersecting a row span, subtract its horizontal span conservatively.
- Snap interval boundaries inward to valid site-aligned starts.
- Remove intervals too small for placement and merge only truly contiguous site-aligned intervals.
- Represent empty rows safely.

### Workstream 5: Density Estimator

Owned files:

- `src/density_estimator.hpp`
- `src/density_estimator.cpp`
- density tests in `tests/`

Implement:

- 10 micron grid size: `10 * dbu_per_micron`.
- Per-grid macro-covered area, movable occupied area, and optional blockage occupancy for scoring only.
- Exclude fully macro-covered grids from overflow scoring when practical.
- Candidate scoring by intersecting the candidate rectangle with affected grids and estimating threshold overflow.
- Commit update after final placement.
- Normalize penalty to a percentage-like scale compatible with displacement scoring.
- Avoid excessive memory on large designs; switch to sparse storage or bounded dense allocation when needed.

### Workstream 6: Legalizer

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- legalizer tests in `tests/`

Implement:

- Deterministic placement order: harder/larger/constrained cells first, stable by original coordinates and input order.
- Candidate row generation in increasing vertical distance from original Y.
- Candidate X generation inside legal intervals around original X, snapped to site width.
- Row occupancy structure for committed cells.
- Rejection of candidates outside die, outside row intervals, overlapping already placed cells, or conflicting with fixed obstacles.
- Score candidates with `alpha`-weighted displacement and density penalty. Use `norm_factor 18.2` as a useful scale for displacement in candidate scoring because `flow.tcl` uses it in final quality.
- Fallback exhaustive search across rows/intervals if local search cannot place a cell.
- Clear failure diagnostic naming the unplaceable cell and dimensions.
- Commit chosen placement by updating cell rectangle, row occupancy, and density estimator.

### Workstream 7: TCL Writer

Owned files:

- `src/tcl_writer.hpp`
- `src/tcl_writer.cpp`
- writer tests in `tests/`

Implement:

- Open requested output path and report I/O errors.
- Require every movable cell to have a final placement.
- Convert DBU to microns using `DBU_Per_Micron`.
- Emit exactly one command per movable `CELL`, in input order:

  ```tcl
  place_cell -inst_name <instName> -orient R0 -origin {X Y}
  ```

- Format numeric values without unnecessary floating-point noise.
- Validate or safely format instance names. Public examples use raw names; if bracing `-inst_name` is confirmed accepted by OpenROAD, bracing may be used for Tcl safety.
- Ensure output never contains `detailed_placement`.

### Workstream 8: Test Fixtures and Validation

Owned files:

- `tests/test_legalizer.cpp`
- `tests/fixtures/*.gp`
- optional validation notes under `doc/`

Implement:

- Lightweight C++ tests with no OpenROAD dependency.
- Fixtures for:
  - one-cell valid parse/legalization
  - macro splitting a row
  - boundary blockage
  - density threshold behavior
  - overfull failure
  - malformed parser cases
- End-to-end smoke test that runs the module pipeline on a tiny `.gp`.
- `make test` must run all tests.
- Document public OpenROAD validation commands for both public benchmarks and at least two parameter settings.

## Testing and Quality Gates

The final implementation must pass the repository's actual gates:

```sh
make
make test
```

Also run focused executable smoke tests on tiny fixtures:

```sh
./Legalizer 0.7 45 tests/fixtures/one_cell.gp /tmp/one_cell.tcl
```

Validate generated TCL content:

```sh
grep -n "detailed_placement" /tmp/one_cell.tcl
```

The grep should find nothing.

If OpenROAD is available in the environment, validate with public benchmarks through `flow.tcl`. Use both public cases:

- `public/ispd19_sample`
- `public/ispd15_mgc_matrix_mult_a`

Run at least two parameter configurations:

- displacement-heavy, for example higher `alpha`
- density-heavy, for example lower `alpha`

The OpenROAD validation must check:

- `check_placement -verbose` reports legality pass.
- No cell overlaps.
- No cell outside the die.
- All movable cells aligned to legal rows/sites.
- All emitted orientations are `R0`.
- Generated TCL contains no `detailed_placement`.
- Runtime is below 30 minutes per benchmark.
- `flow.tcl` reports total displacement, average displacement, DOR, normalized displacement, and final quality score.

If OpenROAD is not installed or cannot run in the current environment, report that explicitly and still complete `make`, `make test`, and fixture smoke tests.

## Acceptance Criteria

The implementation is complete when:

- `Makefile`, `src/`, and `tests/` exist and are coherent.
- `make` creates a root-level `./Legalizer`.
- `make test` passes.
- The executable accepts the exact TA CLI and uses the exact filenames provided by the caller.
- `.gp` parsing supports the assignment format produced by `extract.tcl`.
- Every movable `CELL` receives one legal final placement or the executable fails with a clear diagnostic.
- Final placements are inside the die, site-aligned, fixed-obstacle-free, and mutually non-overlapping under the internal legality checks.
- Output TCL contains exactly one `place_cell` command per movable `CELL`, uses `-orient R0`, converts DBU to microns, and contains no `detailed_placement`.
- Unit tests cover parser, geometry/model validation, row intervals, density accounting, writer formatting, and simple legalizer behavior.
- `doc/tasks/progress.md` and the individual task files are updated to reflect completed work.
- Any public benchmark validation results or inability to run OpenROAD are documented in the final report.

## Uncertainty Protocol

Known uncertainties from `doc/detailed-design.md`:

- Hidden benchmarks may include movable cells taller than one site row. Prefer supporting multi-row cells if practical; otherwise detect them early and fail clearly rather than producing illegal placements.
- Tcl-special instance names may require braced `-inst_name` values. Confirm against OpenROAD if possible; otherwise use the public sample-compatible raw format and validate names.
- DOR excludes fixed macro regions. Treat `MACRO` regions as excluded in density scoring; treat `BLOCKAGE` as a legality obstacle and optional density penalty, not necessarily an excluded grid category.

When uncertain, make conservative assumptions that preserve legality, determinism, and the assignment contract. Do not stop for user input unless the ambiguity blocks implementation or risks violating the assignment rules.
