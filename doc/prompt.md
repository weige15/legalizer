# Vibe Coding Prompt: Make Tail Repair Less Aggressive

## Objective

Tune the existing C++17 OpenROAD placement legalizer so the recent max-displacement repair and ordering changes do not increase average displacement or the assignment DOR proxy unnecessarily.

Keep the expanded candidate search. The best next adjustment is to make repair and ordering more conservative:

- Relax or remove constrainedness-first placement ordering if it is increasing average displacement.
- Make tail repair accept a move only when it reduces maximum displacement by a meaningful margin, for example at least `5u`.
- Reject tail-repair moves that increase total displacement or the density-overflow/DOR proxy.
- Use density-aware scoring during repair instead of pure displacement-only reinsertion.

Preserve all legality guarantees, deterministic output, the command-line interface, and the current build/test workflow.

## Inputs

Read these files first:

- `doc/proposal.md`: original assignment goal and `Quality = alpha * Average Displacement + (1 - alpha) * DOR` metric.
- `doc/high-level-design.md`: module boundaries and data flow.
- `doc/detailed-design.md`: detailed legalizer, density, row, parser, and writer contracts.
- `doc/tasks/progress.md`: confirms all baseline modules are implemented.
- `doc/tasks/legalizer.md`: current legalizer scope and completed baseline requirements.
- `README.md`: build, run, test, and clean commands.
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
- `src/density_estimator.*` tracks 10 micron grid occupancy, exposes `scoreCandidate`, commits placements, and can `rebuildMovableOccupancy`.
- `src/tcl_writer.*` emits one `place_cell -inst_name <name> -orient R0 -origin {X Y}` command per movable cell, converting DBU to microns.
- `src/legalizer.*` owns greedy row-based placement and the current repair pass.

Current `Legalizer` behavior to preserve or adjust carefully:

- `placementOrder()` currently computes `constrainedStartCount()` for each cell and sorts by fewer near-origin legal starts first, then descending area, descending height, original Y, original X, and input index.
- `evaluateCandidatesForRow()` already has expanded X search. It samples snapped preferred X, snapped-up preferred X, interval endpoints, width-offset endpoints, and plus/minus up to `max(8, width_sites + 16)` sites around the preferred X. Keep this expanded search unless tests prove a small local correction is needed.
- Candidate cost in greedy placement is `alpha * displacement_cost + (1 - alpha) * density_cost`, with displacement measured in microns and scaled by `kNormFactor = 18.2`.
- `legalize()` searches a row budget of 32/64/96 feasible rows based on `alpha`, commits each chosen placement immediately, then calls `repairDisplacementTail()`.
- `repairDisplacementTail()` sorts up to 64 cells by descending displacement, removes each candidate cell from occupancy, searches all rows with `displacement_only = true`, and accepts any strictly lower displacement for that cell. It rebuilds movable density at the end.

Known tuning issue:

- The constrainedness-first ordering and pure displacement-only tail repair can reduce the worst single-cell movement but increase average displacement or worsen density/DOR behavior.
- The current repair acceptance test is too permissive: it accepts tiny single-cell displacement improvements even when the overall placement quality proxy gets worse.
- The next run should make the algorithm less aggressive, not add broader swaps or benchmark-specific rules.

Workspace constraint:

- Do not delete files or directories in batch. Do not use `rm -rf`, `rmdir /s`, `Remove-Item -Recurse`, `rd /s`, or `del /s`. If cleanup is needed, delete only one explicit file path at a time, or ask the user to delete batch artifacts manually.

## Execution Model

Operate autonomously and complete the tuning implementation end to end. The main agent owns overall progress, decomposes work into bounded modules, spawns subagents for independent modules where useful, integrates all results, and completes without human-in-the-loop checkpoints unless truly blocked.

When spawning worker agents, use disjoint write scopes. Tell workers they are not alone in the codebase, must not revert edits made by others, and should adapt to concurrent changes. The main agent remains responsible for final design consistency, legality, benchmark validation, and quality gates.

Prefer small, measurable changes. This is a tuning pass over an existing algorithm, not a rewrite.

## Module Plan

### Workstream 1: Metrics and Regression Fixture

Owned files:

- `tests/test_legalizer.cpp`
- optional small fixtures under `tests/`

Implement or improve test helpers that compute:

- per-cell Manhattan displacement in microns,
- maximum displacement,
- total displacement,
- average displacement,
- a density-overflow proxy using `DensityEstimator::scoreCandidate` or another repository-local approximation consistent with `DensityEstimator`.

Add a focused fixture that catches the over-aggressive behavior: a repair move or constrainedness-first ordering choice may reduce one cell's displacement slightly but increases total/average displacement or density pressure. The test should assert that the new implementation rejects that kind of tradeoff while preserving legality.

Keep tests deterministic and independent of OpenROAD.

### Workstream 2: Less Aggressive Placement Ordering

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- legalizer-focused tests in `tests/test_legalizer.cpp`

Evaluate whether strict constrainedness-first ordering is increasing average displacement. Adjust it conservatively:

- Option A: remove constrainedness from the primary sort and return to area/height/original-coordinate ordering.
- Option B: bucket constrainedness so only truly constrained cells move earlier, while similarly flexible cells still follow area/height and original-coordinate order.
- Option C: use constrainedness only as a tie-breaker after area/height when tests show that gives better average displacement.

Choose the smallest option that improves or protects average displacement in the available fixtures. Preserve deterministic tie-breaking.

### Workstream 3: Conservative Tail Repair Acceptance

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- legalizer-focused tests in `tests/test_legalizer.cpp`

Change `repairDisplacementTail()` so it commits a move only when all of these are true:

- The placement remains legal through the existing occupancy and row-interval machinery.
- The move reduces the current maximum displacement by a meaningful margin. Use `5.0` microns as the default minimum improvement unless repository tests show a more natural local constant.
- The move does not increase total displacement. A tiny floating-point epsilon is fine, but do not allow material average-displacement regression.
- The move does not increase the density/DOR proxy. Prefer using `DensityEstimator` scoring/rebuild behavior rather than inventing a disconnected metric.

The repair pass should still be bounded, deterministic, and safe for the assignment timeout. Keep the current limit of inspecting up to 64 tail cells unless there is a measured reason to change it.

### Workstream 4: Density-Aware Repair Scoring

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- `src/density_estimator.hpp`
- `src/density_estimator.cpp` only if a small helper is needed
- tests in `tests/test_legalizer.cpp`

Stop using pure displacement-only reinsertion in repair. Instead:

- Evaluate repair candidates with density-aware scoring, consistent with greedy placement.
- If density rollback is awkward, temporarily remove the cell from occupancy, evaluate candidate rectangles with the current density state plus explicit before/after proxy checks, and rebuild movable density after accepted or rejected attempts so density state matches final placements.
- Avoid broad API churn in `DensityEstimator`; add only narrow helpers if they materially simplify correct before/after checks.

The repair scoring should not let density dominate a clearly beneficial max-displacement improvement, but it must prevent repair from worsening density/DOR proxy for tiny displacement wins.

### Workstream 5: Legality and Output Protection

Owned files:

- `tests/test_legalizer.cpp`
- optionally `src/legalizer.cpp` internal validation helpers if useful

Keep or extend regression checks:

- Every placed cell remains inside `design.die`.
- X is aligned to `site_width` from `die.x_min`.
- Y is aligned to `site_height` from `die.y_min`.
- Movable cells do not overlap each other.
- Movable cells do not overlap fixed `MACRO` or `BLOCKAGE` rectangles.
- Multi-row occupancy remains correct for multi-height cells.
- `TclWriter` still emits no `detailed_placement` command.

Prefer reusable test helper functions so tuning tests stay readable.

## Testing and Quality Gates

The final implementation must pass:

```sh
make
make test
```

Also run at least one CLI smoke test:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp /tmp/one_cell_less_aggressive_repair.tcl
```

Verify the smoke-test output does not contain the prohibited command:

```sh
grep -n "detailed_placement" /tmp/one_cell_less_aggressive_repair.tcl
```

The grep should produce no matches.

If OpenROAD is available, run `flow.tcl` or the repository's established OpenROAD validation flow on the benchmark/setting that motivated this adjustment. Record before/after metrics:

- total displacement,
- average displacement,
- maximum displacement if available or computed from original/final locations,
- top-K or high-percentile displacement if available,
- DOR,
- final quality.

Success is not just lower maximum displacement. The expected result is a better tradeoff: meaningful max-displacement improvements are kept, while average displacement and DOR proxy do not regress from tiny repair wins or overly strict constrainedness-first ordering.

## Acceptance Criteria

- `doc/prompt.md` remains a standalone implementation prompt and accurately reflects the current source tree.
- Expanded candidate search remains in place.
- `placementOrder()` no longer lets strict constrainedness-first sorting materially increase average displacement in local regression tests.
- `repairDisplacementTail()` accepts moves only when they reduce max displacement by at least about `5u` and do not increase total displacement or density/DOR proxy.
- Repair candidate evaluation is density-aware rather than pure displacement-only.
- The implementation does not rely on hard-coded benchmark cell names, absolute coordinates, or hidden benchmark constants.
- All existing legality guarantees are preserved.
- Focused tests cover the less-aggressive repair/order behavior.
- `make` and `make test` pass.
- The CLI smoke test passes and generated TCL contains only direct `place_cell` commands.
- If OpenROAD is available, benchmark notes show the average-displacement/DOR tradeoff improved or explain why only local unit proxies could be run.

## Uncertainty Protocol

Make conservative assumptions and continue unless blocked. If the exact benchmark/setting is not encoded in the repo, tune against the available public fixtures and small synthetic tests, then leave clear instructions for running the same binary on the user's intended benchmark/setting.

If no local fixture reproduces average displacement or DOR regression, still implement the safer repair acceptance thresholds and density-aware repair scoring because they directly match the requested next adjustment. Ask the user only if two viable approaches require choosing between substantially lower max displacement and substantially worse overall quality.
