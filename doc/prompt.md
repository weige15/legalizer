# Vibe Coding Prompt: Reduce Legalizer Displacement Outliers

## Objective

Improve the existing C++17 placement legalizer so it reduces high-displacement outliers while preserving or improving DOR and final quality score.

The current public benchmark comparison against OpenROAD detailed placement is:

| Metric | OpenROAD detailed placement | Current legalizer |
| --- | ---: | ---: |
| Final quality score | 33.7702 | 34.9474 |
| DOR | 31.57% | 31.38% |
| Average displacement | 1.9u | 2.0u |
| Max displacement | 222.8u | 565.4u |

The average displacement and DOR are already close, but a small number of cells are being pushed hundreds of microns away. Fix that tail behavior without accepting a tradeoff that lowers max displacement but increases the final quality score.

Primary target:

- Substantially reduce max displacement on the public benchmark.
- Keep average displacement the same or better.
- Keep DOR roughly the same or better.
- Do not regress the `flow.tcl` final quality score compared with the current legalizer on the same benchmark and parameters.

## Inputs

Read these files first:

- `doc/proposal.md`: assignment objective, constraints, quality formula, and validation plan.
- `doc/detailed-design.md`: architecture and original module contracts.
- `doc/tasks/legalizer.md`: legalizer tasks already marked complete; use this to understand current intended behavior before changing it.
- `doc/tasks/density-estimator.md`: density scoring contract.
- `doc/tasks/row-interval-builder.md`: row capacity and obstacle handling contract.
- `src/legalizer.hpp`
- `src/legalizer.cpp`
- `src/density_estimator.hpp`
- `src/density_estimator.cpp`
- `src/row_interval_builder.hpp`
- `src/row_interval_builder.cpp`
- `src/placement_model.hpp`
- `src/placement_model.cpp`
- `tests/test_legalizer.cpp`
- `Makefile`
- `flow.tcl`: authoritative local legality, DOR, displacement, and final-score metric.
- `README.md`: build/run expectations.

Use `flow.tcl` as the source of truth for benchmark metrics. Internal density estimates and quality proxies are only guides for candidate acceptance.

## Current Implementation

This repository already contains a buildable C++17 legalizer:

- `Makefile` builds a root-level `Legalizer` executable and provides `make test`.
- `src/main.cpp` implements the CLI:

  ```sh
  ./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
  ```

- `src/gp_parser.*` parses `.gp` files produced by `extract.tcl`.
- `src/placement_model.*` provides DBU-based rectangles, snapping, displacement, validation, and model structures.
- `src/row_interval_builder.*` builds legal row free intervals after subtracting `MACRO` and `BLOCKAGE` obstacles.
- `src/density_estimator.*` approximates 10 micron density overflow and excludes fully macro-covered grids from density scoring.
- `src/legalizer.*` performs deterministic greedy row-based legalization.
- `src/tcl_writer.*` emits `place_cell -inst_name <name> -orient R0 -origin {X Y}` commands in microns.
- `tests/test_legalizer.cpp` contains lightweight parser, geometry, row, density, writer, and legalizer tests.

The current `Legalizer` algorithm in `src/legalizer.cpp`:

- Sorts cells by area, height, original Y, original X, and input order.
- Orders row starts by vertical distance from each cell's original Y.
- Builds common obstacle-free intervals for all rows spanned by a cell.
- Subtracts already committed row occupancy.
- Evaluates a limited set of X candidates per available interval: snapped preferred X, interval endpoints, and a small +/- five-site neighborhood.
- Scores candidates using:

  ```text
  alpha * (manhattan_displacement_um * 18.2) + (1 - alpha) * density_score
  ```

- Limits feasible row exploration using a row budget of 32, 64, or 96 depending on alpha.
- Commits each placement immediately and updates occupancy and density state.

The likely source of extreme max displacement is the one-way greedy commitment order plus limited local X sampling. Earlier placements can consume near-original slots, leaving later cells with no opportunity to reclaim nearby vacancies or swap with lower-displacement cells. The refinement should address this without weakening legality or DOR behavior.

Assignment and workspace constraints to preserve exactly:

- Output only `place_cell` commands.
- Do not emit or call `detailed_placement`.
- All emitted orientations must remain `R0`.
- All cells must stay inside the die.
- All placements must be aligned to legal site rows and sites.
- No overlaps with movable cells, macros, or blockages.
- Runtime must remain under 30 minutes for public benchmark validation.
- Do not delete files or directories in batch. Do not use `rm -rf`, `rmdir /s`, `Remove-Item -Recurse`, `rd /s`, or `del /s`. If cleanup is needed, delete only one explicit file path at a time, or ask the user to delete batch artifacts manually.

## Execution Model

Operate autonomously and complete the implementation end to end. The main agent owns progress tracking, decomposition, integration, benchmark comparison, and final reporting. Use subagents where they can work on independent modules or analysis in parallel, but keep write scopes disjoint.

When spawning worker agents, tell them they are not alone in the codebase, must not revert edits made by others, and must adapt to concurrent changes. The main agent remains responsible for final integration, metric quality, legality, and all quality gates.

Make conservative assumptions when uncertain. Do not ask for human checkpoints unless implementation is genuinely blocked or a choice risks violating assignment constraints.

## Module Plan

### Workstream 1: Baseline Measurement and Outlier Diagnosis

Owned files:

- No required source writes.
- Optional notes in `doc/` only if useful.

Tasks:

- Run the existing gates:

  ```sh
  make test
  ```

- Run the public benchmark exactly as requested:

  ```sh
  ALPHA=0.7 THRESHOLD=45 PLACER_MODE=legalizer openroad flow.tcl
  ```

- Capture current final quality score, average displacement, max displacement, and DOR.
- Compare against `PLACER_MODE=detailed` only as context; acceptance is based on before/after current-legalizer comparison.
- Add temporary diagnostics if needed to identify the worst-displaced cells, their original row/X, final row/X, dimensions, and whether nearby legal capacity exists. Remove or gate noisy diagnostics before finalizing.

### Workstream 2: Legalizer Occupancy and Move Infrastructure

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- related tests in `tests/test_legalizer.cpp`

Tasks:

- Add internal helpers that can evaluate legality for moving an already placed cell:
  - Remove a cell's current row occupancy and density contribution from a temporary state, or build a temporary occupancy view for candidate evaluation.
  - Test whether a candidate rectangle is inside the die, inside common row free intervals, site-aligned, and non-overlapping with current movable occupancy.
  - Keep behavior deterministic through stable sorting and explicit tie-breaks.
- If modifying `DensityEstimator` to support temporary removal is too risky, use a local density-delta approximation or copy-on-write estimator for refinement evaluation. Do not corrupt committed density state.
- Keep the existing initial greedy legalizer legal and deterministic.

### Workstream 3: Outlier Refinement Pass

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`
- focused tests in `tests/test_legalizer.cpp`

Tasks:

- After initial greedy legalization succeeds, compute each movable cell's displacement in microns.
- Compute the average displacement and select outliers above:

  ```text
  max(45u, 3 * average_displacement)
  ```

  Make this threshold an internal constant or private helper value. Do not change the public CLI unless there is a strong reason.

- Process outliers in descending displacement, then by input order for determinism.
- For each outlier, search legal nearby slots centered on the original coordinate:
  - Rows in increasing vertical distance from original Y.
  - X starts around original X over a wider radius than the initial +/- five-site search.
  - Include exact interval-derived vacancies and nearest feasible snapped X positions.
  - Bound the search so runtime remains comfortably under 30 minutes.
- Allow at least empty-space relocation. Prefer also supporting a local one-for-one swap with a lower-displacement cell when:
  - both final placements remain legal,
  - the swap reduces max displacement or total displacement,
  - the moved partner does not become a new severe outlier,
  - the density penalty does not materially increase.
- Accept a move only when the weighted quality proxy improves:

  ```text
  alpha * normalized_displacement + (1 - alpha) * density_penalty
  ```

  Use `18.2` as the normalized-displacement scale to match `flow.tcl`. Include a hard guard that estimated density penalty does not materially increase. A small epsilon is fine for floating-point ties.

- Prefer moves that reduce maximum displacement first, then total displacement, then density score, then lower final Y/X for deterministic tie-breaking.
- Run at most a bounded number of refinement rounds. Stop when no accepted move remains.

### Workstream 4: Candidate Selection Improvements

Owned files:

- `src/legalizer.hpp`
- `src/legalizer.cpp`

Tasks:

- Inspect whether the current initial greedy candidate sampling misses nearby legal space because it only checks a few X positions per free interval.
- If beneficial, expand initial candidate generation in a targeted way:
  - Add interval-nearest positions around original X.
  - Add a small geometric or site-step radius expansion.
  - Avoid exhaustive full-row scanning for every cell unless bounded by row/interval size and runtime.
- Keep density scoring in initial placement and refinement acceptance.
- Ensure any changes remain deterministic and do not produce hidden benchmark-specific behavior.

### Workstream 5: Density Evaluation Support

Owned files:

- `src/density_estimator.hpp`
- `src/density_estimator.cpp`
- density tests in `tests/test_legalizer.cpp`

Tasks:

- Add safe support for refinement scoring if needed:
  - score current placement removal plus candidate addition, or
  - expose a copyable estimator for temporary what-if scoring, or
  - implement a local density-delta helper in `Legalizer`.
- Preserve existing macro-grid exclusion semantics.
- Add tests showing density scoring does not decrease incorrectly after moves and that fully macro-covered grids remain excluded.

Only edit this module if the legalizer cannot safely estimate density deltas with existing APIs.

### Workstream 6: Tests and Regression Coverage

Owned files:

- `tests/test_legalizer.cpp`
- small `.gp` fixtures only if needed.

Tasks:

- Add focused unit tests for the outlier refinement behavior using small synthetic designs:
  - A case where an initially far-displaced cell can be moved into nearby empty legal space after greedy placement.
  - A case where a local swap reduces max displacement without creating overlap.
  - A case where a move that would increase density overflow is rejected or loses to a density-safer move.
- Keep tests deterministic and independent of OpenROAD.
- Ensure existing parser, geometry, row interval, density, writer, and overfull tests still pass.

## Testing and Quality Gates

Required local gates:

```sh
make test
```

Also run a smoke check if useful:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp /tmp/out_one_cell.tcl
```

Verify generated TCL still contains only legal placement commands:

```sh
grep -n "detailed_placement" /tmp/out_one_cell.tcl
```

The grep should find nothing.

Required public benchmark gate:

```sh
ALPHA=0.7 THRESHOLD=45 PLACER_MODE=legalizer openroad flow.tcl
```

Compare before and after on the same benchmark and parameters:

- Final quality score
- Average displacement
- Max displacement
- DOR

Do not accept a change that lowers max displacement but increases final quality score. If a candidate implementation improves max displacement while slightly worsening DOR, keep tuning until final quality is non-regressing or revert the quality-harming part.

If OpenROAD is unavailable in the environment, report that explicitly and still complete `make test` plus any executable smoke tests. Do not claim benchmark improvement without `flow.tcl` evidence.

## Acceptance Criteria

The task is complete when:

- `make test` passes.
- `Legalizer` still builds with the repository `Makefile`.
- Generated TCL contains only `place_cell` commands, uses `-orient R0`, and contains no `detailed_placement`.
- Legalizer output remains inside die, site-aligned, obstacle-free, and overlap-free.
- The outlier refinement pass or candidate-selection improvement is implemented in `src/legalizer.*` with deterministic behavior.
- Unit tests cover the new refinement behavior or any new move-evaluation helpers.
- Public benchmark metrics are reported before and after using:

  ```sh
  ALPHA=0.7 THRESHOLD=45 PLACER_MODE=legalizer openroad flow.tcl
  ```

- Before/after results show:
  - Max displacement drops substantially.
  - Average displacement stays the same or improves.
  - DOR stays roughly the same or improves.
  - Final quality score does not regress.

## Uncertainty Protocol

Known uncertainties:

- `flow.tcl` is authoritative; the internal density estimator is only an approximation. If internal scoring and `flow.tcl` disagree, tune based on `flow.tcl`.
- Swaps may be harder to implement safely than empty-space relocation. Implement vacancy-based relocation first, then add swaps only if they materially improve the displacement tail without risking legality.
- The best outlier threshold may differ by benchmark. Start with `max(45u, 3 * average_displacement)` and make the value an internal constant/helper so it can be tuned in one place.
- Runtime matters. Bound refinement rows, X probes, swap partners, and number of rounds; prefer deterministic local neighborhoods over global exhaustive search.

Make conservative implementation choices that preserve legality and final score. Ask a concise question only if blocked by missing tooling or a true assignment ambiguity.
