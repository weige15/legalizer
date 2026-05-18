# Vibe Coding Prompt: OpenROAD Placement Legalizer

## Objective

Implement the complete Programming Assignment #3 placement legalizer for OpenROAD in this repository. Build a Linux C++ command-line program named `Legalizer` that reads an extracted `.gp` global-placement file, legalizes every movable standard cell onto legal site-row/site-column coordinates without overlaps, and writes an OpenROAD TCL script containing one `place_cell` command per movable cell.

Legality is the first priority. After legality is satisfied, optimize the assignment quality metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of non-macro 10um x 10um density grids whose utilization exceeds the supplied `threshold`.

The final program must support the TA command shape:

```sh
make
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

## Inputs

Read these source materials first:

- `p3_placement.pdf`: assignment specification, command-line contract, input/output formats, scoring, runtime limit, and prohibited output behavior.
- `doc/proposal.md`: project objective, assumptions, proposed implementation stages, validation plan, and open questions.
- `doc/high-level-design.md`: architecture overview, module responsibilities, data flow, legality contract, and risks.
- `doc/detailed-design.md`: module-level design details, interfaces, failure handling, and test plans.
- `doc/tasks/progress.md`: current task checklist.
- `doc/tasks/cli-and-configuration.md`
- `doc/tasks/gp-parser.md`
- `doc/tasks/placement-data-model.md`
- `doc/tasks/row-and-obstacle-model.md`
- `doc/tasks/density-grid-model.md`
- `doc/tasks/legalization-engine.md`
- `doc/tasks/local-refinement.md`
- `doc/tasks/tcl-writer.md`
- `doc/tasks/validation-helpers.md`
- `flow.tcl`: OpenROAD reference flow for global placement, extraction, debug detailed placement, legality checks, displacement, heatmap DOR, and final quality.
- `extract.tcl`: exact `.gp` extraction script and object typing logic.
- `public/ispd15_mgc_matrix_mult_a/` and `public/ispd19_sample/`: public LEF/DEF benchmarks for integration validation.

## Current Implementation

There is no C++ implementation, Makefile, or test suite in the homework folder yet. The repository currently contains planning documents, assignment PDFs/scripts, `public.tar`, and public benchmark input data.

The assignment `.gp` format contains:

```text
DBU_Per_Micron <integer>
DieArea_LL <x> <y>
DieArea_UR <x> <y>
Site_Width <integer>
Site_Height <integer>

Name LLX LLY Width Height Type
<instName> <lowerleftX> <lowerleftY> <width> <height> <CELL|MACRO|BLOCKAGE>
```

Only `CELL` records are movable. `MACRO` and `BLOCKAGE` records are fixed obstacles. Use integer DBU coordinates internally. Convert output origins to microns only when writing TCL, using `DBU_Per_Micron`.

The output TCL must contain commands in this form:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {<xMicron> <yMicron>}
```

Do not emit commands for macros or blockages. Do not emit or call `detailed_placement` in the generated output TCL. Cell rotation is forbidden, so every emitted command must use `-orient R0`.

`flow.tcl` currently runs OpenROAD `detailed_placement` as a debugging baseline. During implementation, replace that debug step with `exec make`, `exec timeout 30m ./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`, and `source <output>.tcl` when validating. The final generated TCL must still avoid `detailed_placement`.

The current planning documents use these module boundaries:

- CLI and Configuration
- GP Parser
- Placement Data Model
- Row and Obstacle Model
- Density Grid Model
- Legalization Engine
- Local Refinement
- TCL Writer
- Validation Helpers

## Execution Model

Work autonomously from implementation through validation. The main agent owns the overall plan, updates task progress, decomposes work by module, spawns subagents for independent modules where useful, integrates their changes, resolves cross-module API issues, and completes the whole implementation without human-in-the-loop checkpoints.

When spawning worker agents, give each one a disjoint write scope. Tell every worker that they are not alone in the codebase, must not revert edits made by others, and should adapt their work to concurrent changes. The main agent remains responsible for reviewing and integrating all worker output.

Use conservative implementation choices that match the planning docs. If docs and code conflict, prefer assignment correctness and document the resolution in the relevant source comments or docs.

## Module Plan

### Workstream 1: Build System, CLI, and Data Model

Owned files should include `Makefile`, `src/main.cpp`, `src/config.*`, `src/geometry.*`, `src/design.*`, and matching tests.

Implement:

- `make` target that builds `./Legalizer`.
- Optional `make test`, `make lint`, and `make typecheck` targets if useful for local quality gates.
- `Config { alpha, threshold, inputPath, outputPath }`.
- Strict command-line parsing for exactly four user arguments.
- Finite numeric parsing with trailing-character rejection.
- Shared DBU geometry types: `Point`, `Rect`, `Cell`, `Obstacle`, `Design`.
- Half-open rectangle overlap helpers.
- Site snapping helpers relative to `DieArea_LL`.
- DBU-to-micron conversion and Manhattan displacement helpers.
- Validation of die dimensions, site dimensions, and impossible cell dimensions.

### Workstream 2: GP Parser and TCL Writer

Owned files should include `src/gp_parser.*`, `src/tcl_writer.*`, parser/writer fixtures, and tests.

Implement:

- Parser for required metadata keys: `DBU_Per_Micron`, `DieArea_LL`, `DieArea_UR`, `Site_Width`, and `Site_Height`.
- Header detection for `Name LLX LLY Width Height Type`, tolerating optional blank lines.
- Record parsing with signed 64-bit DBU geometry.
- Rejection of malformed lines, missing metadata, non-positive dimensions, bad numbers, and unknown object types.
- Preservation of original record order for deterministic tie-breaking and output.
- TCL writing in original movable-cell order.
- Stable decimal formatting for micron coordinates.
- A hard guard that generated output contains no `detailed_placement`.

### Workstream 3: Row and Obstacle Model plus Validation

Owned files should include `src/row_model.*`, `src/validation.*`, and related tests.

Implement:

- Legal row origins derived from `DieArea_LL.y`, `DieArea_UR.y`, and `Site_Height`.
- Per-row free intervals initialized to die width.
- Macro and blockage subtraction for every intersected row.
- Free interval snapping to legal site columns.
- `canPlace(cell, x, y)` for one-row and multi-row cells.
- Candidate slot enumeration near target X and target row with bounded per-row output.
- Occupancy `commit` and `uncommit` that cannot mutate state on illegal moves.
- Internal validation for missing placements, die containment, X/Y site alignment, fixed-obstacle overlaps, and movable-cell overlaps.

### Workstream 4: Density Grid Model

Owned files should include `src/density_grid.*` and related tests.

Implement:

- 10um x 10um bins using `10 * DBU_Per_Micron` DBU.
- At least one bin in each dimension, even for tiny dies.
- Macro-overlapped bins excluded from DOR counting.
- Blockages treated as placement obstacles but not macro-excluded density regions unless benchmark validation clearly requires otherwise.
- Approximate occupied cell area per non-excluded bin.
- `densityCost(cell, x, y)`, `addCell`, `removeCell`, and optional `estimateDOR()`.
- Stable zero-overflow behavior when all bins are excluded.

The density grid is a heuristic only. It must never override row/site legality.

### Workstream 5: Legalization Engine and Local Refinement

Owned files should include `src/legalizer.*`, `src/refinement.*`, and related tests.

Implement:

- Deterministic cell order by snapped original row, original X, descending width tie-breaker, and input index.
- Candidate row search by increasing vertical distance from original row.
- Candidate X generation from clamped original X, interval boundaries, rightmost legal origin, and nearby legal sites.
- Cost function:

```text
score = alpha * displacementMicrons + (1 - alpha) * densityCost
```

- Deterministic tie-breaking by score, displacement, row distance, X, and row index.
- Full-design fallback search if bounded local search finds no legal candidate.
- Clear fatal diagnostic if a cell cannot be legalized.
- Commit through row occupancy and density updates only after a candidate is validated.
- Bounded refinement passes:
  - single-cell relocation for high-displacement or high-density cells,
  - row-local compaction toward original X without changing row order,
  - optional nearby pair swaps when both cells remain legal and cost improves.
- Rollback for every rejected or failed trial move.
- Validation after placement and after refinement.

## Testing and Quality Gates

Add focused tests as the implementation grows. Prefer a small C++ test executable or scriptable test target that can run without OpenROAD for unit-level checks.

Required unit coverage:

- CLI parser: valid arguments, malformed numeric values, wrong argument counts.
- GP parser: valid PDF-style sample, missing metadata, bad numeric fields, unknown type, invalid dimensions.
- Geometry/model: touching rectangles, real overlap, snapping, micron conversion, object splitting.
- Row model: obstacle subtraction, boundary-touching intervals, multi-row cells, occupancy overlap.
- Density grid: bin indexing, macro exclusion, add/remove symmetry, threshold overflow.
- Legalizer: already-legal cells, overlapping cells, macro-split rows, density-aware movement, fallback expansion.
- Refinement: accepted move, rejected move rollback, compaction legality, no-improvement early stop.
- Validation: valid placement, out-of-bounds, X/Y misalignment, obstacle overlap, movable overlap, missing placement.
- TCL writer: one command per movable cell, no fixed-object commands, micron units, no `detailed_placement`.

Required command gates before finishing:

```sh
make
pytest
mypy .
ruff check .
```

This is primarily a C++ project, so `pytest`, `mypy`, and `ruff` may be unavailable or irrelevant unless a Python test harness is introduced. If they are not configured, do not fake success. Record the exact reason, and ensure the meaningful C++ gates pass instead, such as:

```sh
make test
```

or the repository's implemented equivalent.

Integration validation with OpenROAD should use `flow.tcl` and the public benchmarks when OpenROAD is available:

1. Generate `<designName>_insts.gp` through `extract.tcl`.
2. Run `./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl`.
3. Source the generated TCL after global placement.
4. Confirm `check_placement -verbose` passes.
5. Inspect displacement, DOR, and final quality metrics.
6. Test at least one displacement-focused and one DOR-focused parameter set.

## Acceptance Criteria

- `make` builds an executable named `Legalizer` in the repository root.
- `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl` parses the input, places every movable `CELL`, and writes the output TCL.
- Every placed cell is inside the die, aligned to legal site columns and rows, and non-overlapping with fixed macros, blockages, and other cells.
- Output coordinates are in microns, not DBU.
- Output includes exactly one `place_cell` command per movable cell and no commands for `MACRO` or `BLOCKAGE` records.
- Output uses `-orient R0` for every cell and never contains `detailed_placement`.
- The placement process is deterministic for identical inputs and parameters.
- Internal validation runs before TCL writing and prevents writing known-illegal results.
- Unit tests cover the module responsibilities listed above.
- Public benchmark validation passes OpenROAD `check_placement -verbose` when OpenROAD is available.
- Runtime stays bounded and is designed for the assignment's 30-minute per-case limit.
- `doc/tasks/progress.md` is updated as tasks are completed.

## Uncertainty Protocol

Make conservative assumptions unless blocked. Do not pause for human input to tune optional heuristics; implement a legal, deterministic baseline first, then improve quality.

Known uncertainties and resolutions:

- Student ID and final submission folder name are not known. Do not package the final tarball unless the user later provides the ID.
- The exact hidden benchmark scale is unknown. Use row intervals, bounded candidate generation, and deterministic fallback search; avoid global pairwise overlap checks in hot paths.
- The exact OpenROAD heatmap density scaling may differ from the internal density estimate. Treat density as a heuristic and validate final DOR with `flow.tcl`.
- The assignment does not define whether all cells are single-row. Support multi-row cells in legality checks and candidate placement.
- The assignment shows a mandatory blank line before the header, but `extract.tcl` may be the practical source of truth. Tolerate optional blank lines.

If a true blocker remains after reading the docs and code, ask one concise question naming the file, command, or requirement that blocks implementation. Otherwise, proceed autonomously to a complete implementation and verification pass.
