# Autonomous Vibe Coding Prompt: Abacus-Based Density-Aware Placement Legalizer

## Objective

Implement the C++17 placement legalizer described by the planning documents. The finished repository must build a standalone executable:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

The executable must parse an OpenROAD-extracted `.gp` file, legalize all movable `CELL` instances onto legal site rows, avoid all `MACRO` and `BLOCKAGE` regions, validate legality and metrics, and write only explicit OpenROAD `place_cell` commands. Optimize for the repository flow quality:

```text
flow_quality = alpha * (average_displacement_um * 18.2) + (1 - alpha) * DOR
```

DOR is the percentage of non-macro-overlapped 10um by 10um density grids whose density is strictly greater than `threshold`.

## Inputs

Read these files first, in this order:

- `doc/proposal.md`: project objective, algorithm proposal, assumptions, milestones, validation plan, and open questions.
- `doc/detailed-design.md`: authoritative module contracts, cross-module data contracts, scoring rules, tests, and risks.
- `doc/high-level-design.md`: higher-level architecture and module relationships.
- `doc/tasks/progress.md`: overall task list. All listed implementation tasks are currently unchecked.
- `doc/tasks/cli-configuration.md`
- `doc/tasks/placement-model.md`
- `doc/tasks/gp-parser.md`
- `doc/tasks/row-interval-builder.md`
- `doc/tasks/density-estimator.md`
- `doc/tasks/abacus-row-solver.md`
- `doc/tasks/legalization-engine.md`
- `doc/tasks/candidate-scoring.md`
- `doc/tasks/final-validator.md`
- `doc/tasks/tcl-writer.md`
- `doc/tasks/tests.md`
- `README.md`: expected user-facing build, run, test, and OpenROAD flow behavior.
- `Makefile`: required source layout, compile flags, executable name, and `make test` behavior.
- `flow.tcl` and `extract.tcl`: local OpenROAD integration, scoring, timeout, output checks, and `.gp` extraction behavior.
- Assignment/reference materials when needed: `p3_placement.pdf`, `abacus.pdf`, `openroad_tutorial.pdf`, `Legalization_algorithm_for_multiple-row_height_standard_cell_design.pdf`, and notes under `literature/`.

## Current Implementation

The repository is currently a planning-heavy C++ project. The `Makefile` is present and expects this production layout:

```text
src/placement_model.cpp
src/gp_parser.cpp
src/row_interval_builder.cpp
src/density_estimator.cpp
src/legalizer.cpp
src/tcl_writer.cpp
src/main.cpp
```

It also expects:

```text
tests/test_legalizer.cpp
tests/fixture_one_cell.gp
```

The current checkout does not contain `src/` or `tests/` directories. Recreate the source and test tree from the planning docs instead of relying on the existing root `Legalizer` binary. Do not treat the binary as source of truth.

The root `Makefile` compiles with:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc
```

The `test` target must build `tests/test_legalizer`, run it, then smoke-test:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

`flow.tcl` runs `make`, requires `./Legalizer`, extracts a `.gp`, invokes:

```sh
timeout 30m ./Legalizer $alpha $threshold $gp_file $legalizer_tcl
```

It rejects empty output and output containing `detailed_placement`, sources the generated TCL, runs `check_placement`, computes average displacement in microns, computes 10um density bins, excludes bins overlapped by macros, counts overflow bins using `density > threshold`, and reports the final normalized quality score.

Repository constraints:

- Use only C++17 standard library dependencies.
- Keep all internal geometry in integer DBU.
- Convert to microns only when writing TCL or reporting metrics.
- Use half-open rectangles `[x0, x1) x [y0, y1)`.
- `CELL` is movable. `MACRO` and `BLOCKAGE` are fixed obstacles.
- Output TCL must contain one deterministic `place_cell -inst_name <name> -orient R0 -origin {<x> <y>}` command per movable cell.
- Never emit or call `detailed_placement`.
- Do not rotate cells.
- Support single-row movable cells robustly. If a movable cell height is not exactly one site height, reject it with a clear unsupported-case diagnostic unless you choose to implement multi-row support fully.
- Respect repository deletion constraints: do not use recursive/batch deletion commands such as `rm -rf`, `rmdir /s`, or `Remove-Item -Recurse`.

## Execution Model

Work autonomously until the implementation is complete. The main agent owns overall progress, task tracking, integration, validation, and final quality gates. Decompose the project into independent modules and spawn subagents where useful for disjoint workstreams. Complete the full coding run without human-in-the-loop checkpoints unless there is a true blocker that cannot be resolved from the docs or code.

When spawning worker subagents, give each one explicit file ownership. Tell every worker that they are not alone in the codebase, must not revert others' edits, and must adapt to concurrent changes. Keep worker write scopes disjoint and have the main agent integrate results.

Update `doc/tasks/progress.md` and the individual task files as implementation tasks are completed. Preserve the planning docs unless an implementation fact requires a small correction.

## Module Plan

### Workstream A: Core Model and Parser

Own these files:

```text
src/placement_model.h
src/placement_model.cpp
src/gp_parser.h
src/gp_parser.cpp
```

Implement:

- `Point`, `Rect`, `Instance`, `RowInterval`, `Metrics`, and `InstanceType`.
- Stable movable-cell ids and deterministic input ordering.
- Half-open rectangle overlap and overlap-area helpers.
- Site snapping, row conversion, alignment checks, and single-row support checks.
- Strict `.gp` parsing with metadata in exact order:

  ```text
  DBU_Per_Micron <int>
  DieArea_LL <x> <y>
  DieArea_UR <x> <y>
  Site_Width <int>
  Site_Height <int>

  Name LLX LLY Width Height Type
  ```

- Line-numbered parser diagnostics for malformed metadata, header, fields, dimensions, overflow, and unknown types.

### Workstream B: Legal Capacity, Density, and Validation

Own these files:

```text
src/row_interval_builder.h
src/row_interval_builder.cpp
src/density_estimator.h
src/density_estimator.cpp
```

Also add validator helpers in an appropriate module, either `placement_model` or a small validation type used by `legalizer`.

Implement:

- Row intervals for every site row from die lower Y through the last full site-height row.
- Obstacle subtraction for every `MACRO` and `BLOCKAGE` intersecting each row.
- Site snapping for interval starts and ends.
- Capacity diagnostics for no legal intervals and insufficient total capacity.
- 10um by 10um density bins using `10 * DBU_Per_Micron`, clipped at die edges.
- DOR exact recomputation, excluding bins with any `MACRO` overlap and keeping blockage-covered bins countable.
- Strict overflow comparison: `density > threshold`.
- Local candidate density pressure estimates for trial scoring.
- Final validation for all placed cells: placed, in die, site-aligned, legal row, one-row height, contained in a legal interval, no cell-cell overlap, no macro/blockage overlap, exact average displacement, normalized displacement, DOR, flow quality, and handout quality.

### Workstream C: Abacus Solver, Scoring, and Legalization

Own these files:

```text
src/legalizer.h
src/legalizer.cpp
```

Implement:

- Abacus row trial solver for one `RowInterval`, existing assigned cells, and an optional inserted candidate.
- Stable ordering by target/original X with id tie-breaks.
- Cluster creation, merge, weighted ideal X, clamping to interval bounds, and expansion into non-overlapping lower-left X positions.
- Candidate cost:

  ```text
  alpha * normalized_displacement_delta
    + (1 - alpha) * estimated_dor_delta
    + deterministic_local_penalties
  ```

- Use `average_displacement_um * 18.2` for flow-compatible normalized displacement.
- Named, centralized constants for density pressure, interval-edge penalties, row-window sizes, deterministic pass count, and repair budget.
- Deterministic variants:
  - left-to-right by original X;
  - right-to-left by original X;
  - density-first for low `alpha`;
  - large-cell-first tie-breaks in dense regions.
- Candidate interval search by row distance, interval capacity, vertical-displacement lower-bound pruning, bounded row window, and expandable fallback if no feasible interval is initially found.
- Best-result selection across successful variants using exact validation metrics.
- Bounded local repair for displacement outliers and overflow-bin cells. Accept moves only when exact flow quality improves, or in low-`alpha` mode when DOR improves with a bounded displacement increase.
- Failure diagnostics naming the cell and reason if no legal placement can be found.

### Workstream D: CLI, Writer, and Tests

Own these files:

```text
src/main.cpp
src/tcl_writer.h
src/tcl_writer.cpp
tests/test_legalizer.cpp
tests/*.gp
```

Implement:

- `RunConfig { alpha, threshold, input_path, output_path }`.
- Required argument validation: exactly four user arguments, finite `alpha` in `[0, 1]`, finite `threshold` in `[0, 100]`.
- A narrow `run(config)` pipeline: parse, build intervals, legalize, validate, write.
- Concise `stderr` diagnostics and nonzero exits for argument, parse, capacity, unsupported, legalization, validation, and write failures.
- Writer that emits only deterministic `place_cell` commands in movable input order.
- DBU-to-micron conversion with enough precision for OpenROAD site coordinates.
- Temporary sibling write and final rename only after flush/content checks succeed.
- A content check that rejects any generated text containing `detailed_placement`.
- A lightweight C++ test executable using `assert` or a tiny local check macro.
- Fixtures for one-cell, two-overlap, macro-split, blockage, malformed parser cases, and density examples.

## Testing and Quality Gates

The final implementation must pass every configured local gate:

```sh
make
make test
```

The build must be clean under the existing flags:

```text
-std=c++17 -O2 -Wall -Wextra -pedantic -Isrc
```

No separate configured formatter, linter, type checker, or static-analysis command currently exists in the repository. Do not add mandatory external tooling. Keep code warning-clean under the existing compiler flags.

Unit and integration coverage must include:

- Parser success and malformed-input failures.
- Geometry overlap at touching edges and true intersections.
- Site snapping and row conversion.
- Movable/fixed partitioning and multi-row rejection.
- Row interval construction around macros and blockages.
- Density grid sizing, macro exclusion, strict threshold comparison, exact DOR, and candidate penalty monotonicity.
- Abacus one-cell clamp, two-cell merge, left/right boundary clamp, over-capacity failure, and stable tie behavior.
- Full legalization on small fixtures: one cell, two overlapping cells, macro-split row, blockage case, and at least one low-`alpha` density-pressure case.
- Validator failures for unplaced cells, off-site X, off-row Y, out-of-die placement, cell-cell overlap, macro overlap, and blockage overlap.
- Writer command shape, fractional micron conversion, deterministic ordering, and prohibited-command absence.
- CLI invalid argument failures and successful one-cell smoke run.

When OpenROAD is available, also run the manual public flow checks:

```sh
openroad flow.tcl
CASE_NAME=public/ispd15_mgc_matrix_mult_a openroad flow.tcl
```

Benchmark both a displacement-focused and density-focused configuration when practical, for example by setting high and low `ALPHA` values with the assignment threshold. OpenROAD-dependent checks are optional only if OpenROAD is unavailable; record that limitation in the final report.

## Acceptance Criteria

The implementation is complete when:

- `src/` and `tests/` exist with the modular C++17 layout expected by `Makefile`.
- `make` produces `./Legalizer` from source.
- `make test` passes without OpenROAD.
- `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl` validates arguments, parses input, legalizes cells, validates final legality and metrics, and writes output only after validation passes.
- Generated TCL contains only explicit `place_cell` commands and never contains `detailed_placement`.
- All movable cells in supported inputs are placed inside the die, aligned to sites and rows, non-overlapping, and outside macros and blockages.
- Exact metrics are recomputed before writing: average displacement in microns, normalized displacement using `18.2`, DOR, flow quality, and handout quality.
- Candidate scoring reacts to `alpha` and `threshold`, prioritizing displacement for high `alpha` and overflow reduction for low `alpha`.
- Deterministic variants and bounded repair are implemented with repeatable behavior and a clear runtime budget.
- Unsupported multi-row movable cells fail with a clear diagnostic rather than producing illegal TCL.
- Task progress docs are updated to reflect completed implementation work.
- No recursive or batch deletion commands are used.

## Uncertainty Protocol

Make conservative implementation decisions when the docs leave constants open. Keep tunable constants named and centralized. Prefer correctness and deterministic legal output over aggressive quality tuning.

Known uncertainties:

- Hidden benchmarks may include multi-row movable cells. The current documented scope allows clear rejection of unsupported multi-row movable cells unless you implement full multi-row legalization.
- The assignment handout's raw quality formula differs from `flow.tcl`, which multiplies average displacement by `18.2`. Optimize and select final candidates using the flow-compatible score, and report both flow-compatible and handout-form metrics.
- Candidate window sizes, density penalty constants, deterministic pass count, and repair budget require tuning. Start with safe deterministic values, verify correctness on unit fixtures, then tune with public cases if OpenROAD is available.

If a true blocker remains after reading the docs and code, ask one concise question before implementing that blocked part. Otherwise, proceed autonomously and complete the repository.
