# Vibe Coding Prompt: Reduce Max Displacement Tail

## Objective

Improve the existing C++17 OpenROAD placement legalizer for the current benchmark/parameter setting by reducing the high-displacement tail after greedy placement. Focus on maximum displacement and high-percentile displacement, not DOR. Preserve all existing legality guarantees, CLI behavior, deterministic output, and build/test workflow.

The most useful next tuning target is the max displacement tail: after the first greedy placement, add a local repair pass for cells with very high displacement, or change ordering/candidate selection so early cells do not occupy near-original slots that later constrained cells need.

Do not spend this run retuning density overflow reduction. Keep DOR behavior from regressing badly, but treat average/max displacement as the main optimization target.

## Inputs

Read these files first:

- `doc/proposal.md`: original assignment goal and quality metric.
- `doc/high-level-design.md`: module boundaries and data flow.
- `doc/detailed-design.md`: detailed legalizer, density, row, parser, and writer contracts.
- `doc/tasks/progress.md`: confirms all baseline modules are already implemented.
- `doc/tasks/legalizer.md`: current legalizer task scope and completed baseline requirements.
- `README.md`: build, run, and test commands.
- `Makefile`: actual C++17 build and `make test` target.
- `src/legalizer.hpp`
- `src/legalizer.cpp`
- `src/density_estimator.hpp`
- `src/density_estimator.cpp`
- `src/row_interval_builder.hpp`
- `src/row_interval_builder.cpp`
- `src/placement_model.hpp`
- `src/placement_model.cpp`
- `tests/test_legalizer.cpp`
- `flow.tcl`: authoritative OpenROAD legality and scoring flow when OpenROAD is available.

Useful public benchmark folders:

- `public/ispd19_sample/`
- `public/ispd15_mgc_matrix_mult_a/`

## Current Implementation

The repo already contains a working C++17 legalizer. Do not reconstruct the project from scratch.

Actual source layout:

- `Makefile` builds a root-level `Legalizer` executable with `g++ -std=c++17 -O2 -Wall -Wextra -pedantic -Isrc`.
- `make test` builds `tests/test_legalizer` and runs a one-cell CLI smoke test.
- `src/main.cpp` parses `Legalizer <alpha> <threshold> <input.gp> <output.tcl>`, loads a `Design`, validates it, builds row intervals, creates `DensityEstimator`, runs `Legalizer::legalize`, and writes TCL.
- `src/placement_model.*` stores DBU geometry, half-open rectangles, site snapping helpers, displacement, and design validation.
- `src/gp_parser.*` parses `.gp` metadata and `CELL`/`MACRO`/`BLOCKAGE` instances.
- `src/row_interval_builder.*` creates per-row legal X intervals and subtracts fixed obstacles.
- `src/density_estimator.*` tracks 10 micron grid occupancy and scores density overflow candidates.
- `src/tcl_writer.*` emits one `place_cell -inst_name <name> -orient R0 -origin {X Y}` command per movable cell, converting DBU to microns.
- `src/legalizer.*` owns greedy row-based placement.

Current `Legalizer` behavior:

- `placementOrder()` sorts cells by descending area, descending height, original Y, original X, and input index.
- `rowOrderForCell()` sorts legal starting rows by vertical distance from the cell's original Y.
- `evaluateCandidatesForRow()` computes common free intervals across the cell's row span, subtracts committed occupancy, and tries a small set of site-aligned X candidates:
  - snapped preferred X,
  - snapped-up preferred X,
  - interval start,
  - interval end,
  - plus/minus 1 through 5 site widths around preferred X.
- Candidate cost is `alpha * displacement_cost + (1 - alpha) * density_cost`, with displacement measured in microns and scaled by `kNormFactor = 18.2`.
- `legalize()` searches a row budget of 32/64/96 feasible rows based on `alpha`, commits each chosen placement immediately, and fails with a named cell diagnostic if no candidate is found.
- Row occupancy is tracked as sorted horizontal segments per row.
- Multi-height cells are supported when height is a multiple of site height by intersecting row intervals and updating occupancy across all covered rows.

Known tuning issue:

- Greedy placement can create a long max-displacement tail. Early cells sometimes take near-original legal slots that later cells with tighter row/interval constraints need, forcing those later cells far away.
- The current candidate generator is local and sparse, so a cell may miss a slightly farther slot that would lower global displacement pressure.
- There is no post-placement repair or swap pass.

Workspace constraint:

- Do not delete files or directories in batch. Do not use `rm -rf`, `rmdir /s`, `Remove-Item -Recurse`, `rd /s`, or `del /s`. If cleanup is needed, delete only one explicit file path at a time, or ask the user to delete batch artifacts manually.

## Execution Model

Operate autonomously and complete the tuning implementation end to end. The main agent owns overall progress, decomposes work into bounded modules, spawns subagents where useful for independent analysis or test work, integrates all results, and completes without human-in-the-loop checkpoints unless truly blocked.

When spawning worker agents, use disjoint write scopes. Tell workers they are not alone in the codebase, must not revert edits made by others, and should adapt to concurrent changes. The main agent remains responsible for final design consistency, legality, benchmark validation, and quality gates.

Prefer conservative changes that improve displacement tail while preserving the simple deterministic architecture. Do not introduce hidden benchmark-specific constants tied to cell names or absolute design dimensions.

## Module Plan

### Workstream 1: Measure and Characterize Displacement Tail

Owned files:

- `tests/test_legalizer.cpp`
- optional small fixtures under `tests/`
- optional documentation notes in `doc/`

Implement or run:

- Add test/helper logic that computes per-cell Manhattan displacement in DBU or microns after legalization.
- Create a focused in-memory fixture that reproduces the tail pattern: an early flexible cell can take a near-original slot, while a later constrained cell then gets pushed far away.
- Assert that the repair/order improvement reduces the constrained cell's displacement or max displacement without violating legality.
- Keep tests deterministic and independent of OpenROAD.

Do not make tests brittle by depending on exact placements for unrelated cells unless needed to prove the tail fix.

### Workstream 2: Local Repair Pass After Greedy Placement

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- legalizer-focused tests in `tests/test_legalizer.cpp`

Implement a local displacement-tail repair pass after the first greedy pass. Recommended shape:

- Track or reconstruct enough occupancy to temporarily remove and reinsert cells.
- Identify tail cells by displacement, for example max displacement cells or cells above a high percentile / multiple of median or average.
- For each tail cell, try bounded local improvements:
  - remove the tail cell from occupancy and density state if density state can be rebuilt safely,
  - search a wider candidate set near its original location,
  - optionally move one blocking cell to another nearby legal slot if that reduces max displacement and does not increase total displacement excessively,
  - commit only improvements that preserve legality and deterministic tie-breaking.
- Keep the pass bounded by a small iteration count and candidate budget so runtime remains safely below the 30 minute assignment timeout.

If `DensityEstimator` cannot easily support rollback, prefer a repair design that rebuilds density from scratch after accepted moves, or run the repair with displacement-dominant scoring and then rebuild final density state before any later candidate scoring. Do not leave density state inconsistent with final placements.

The repair pass should optimize these priorities in order:

1. Reduce maximum displacement.
2. Reduce high-percentile or top-K displacement.
3. Avoid increasing average displacement materially.
4. Avoid major DOR regression.

### Workstream 3: Candidate Selection and Ordering Improvements

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- tests in `tests/test_legalizer.cpp`

Improve the first greedy pass so it avoids creating the tail where practical:

- Add a cell difficulty/constrainedness signal to `placementOrder()`, such as number/width of feasible row intervals near the original row, local obstacle pressure, cell width, row span, or available nearby capacity.
- Consider placing constrained cells earlier than flexible cells even when area is similar.
- Expand X candidate generation beyond plus/minus 5 sites in a controlled way. For example, sample nearest legal starts around the preferred X from interval boundaries and occupancy gaps, or grow the step budget based on cell width / site width.
- Preserve deterministic tie-breaking by cost, displacement, Y, X, and input index where relevant.

Keep this workstream focused on displacement. Do not make density penalty dominate when `alpha` is high or when the user is tuning max displacement.

### Workstream 4: Legality and Regression Protection

Owned files:

- `tests/test_legalizer.cpp`
- optionally `src/legalizer.cpp` internal validation helpers if useful

Add focused regression checks:

- Every placed cell remains inside `design.die`.
- X is aligned to `site_width` from `die.x_min`.
- Y is aligned to `site_height` from `die.y_min`.
- Movable cells do not overlap each other.
- Movable cells do not overlap fixed `MACRO` or `BLOCKAGE` rectangles.
- Multi-row occupancy remains correct for multi-height cells.
- Output behavior remains unchanged: `TclWriter` still emits no `detailed_placement` command.

Prefer reusable test helper functions for legality checks so new repair tests stay readable.

## Testing and Quality Gates

The final implementation must pass:

```sh
make
make test
```

Also run at least one CLI smoke test:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp /tmp/one_cell_tail_repair.tcl
```

Verify the smoke-test output does not contain the prohibited command:

```sh
grep -n "detailed_placement" /tmp/one_cell_tail_repair.tcl
```

The grep should produce no matches.

If OpenROAD is available, run `flow.tcl` or the repository's established OpenROAD validation flow on the specific benchmark/setting the user is tuning. Record before/after metrics:

- total displacement,
- average displacement,
- maximum displacement if available or computed from original/final locations,
- top-K or high-percentile displacement if available,
- DOR,
- final quality.

The key success metric is lower max/top-tail displacement for the same benchmark/setting. DOR does not need to improve.

## Acceptance Criteria

- `doc/prompt.md` remains a standalone implementation prompt and accurately reflects the current source tree.
- `src/legalizer.*` implements a deterministic displacement-tail improvement through local repair, better ordering/candidate selection, or both.
- The implementation does not rely on hard-coded benchmark cell names, absolute coordinates, or hidden benchmark constants.
- All existing legality guarantees are preserved.
- Focused tests cover the displacement-tail scenario and prove the new logic improves it.
- `make` and `make test` pass.
- The CLI smoke test passes and generated TCL contains only direct `place_cell` commands.
- If OpenROAD is available, benchmark notes show max/top-tail displacement improved or explain why the local unit proxy passed but OpenROAD validation could not be run.

## Uncertainty Protocol

Make conservative assumptions and continue unless blocked. If the exact benchmark/setting is not encoded in the repo, tune against the available public fixtures and small synthetic tail tests, then leave clear instructions for running the same binary on the user's intended benchmark/setting.

If a proposed repair reduces max displacement but causes a severe average displacement or DOR regression, prefer a smaller bounded repair and document the tradeoff. Ask the user only if two viable approaches require choosing between substantially lower max displacement and substantially worse overall quality.
