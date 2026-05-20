# Proposal: Abacus-Based Density-Aware Placement Legalizer

## Objective

Build a standalone C++17 placement legalizer for Programming Assignment #3, "Placement with OpenROAD." The program will implement the required command-line interface:

```sh
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

The legalizer will read the OpenROAD-extracted `.gp` file, place all movable `CELL` instances legally on site rows, avoid all `MACRO` and `BLOCKAGE` regions, and write explicit OpenROAD `place_cell` commands. The algorithm should minimize the assignment quality metric:

```text
Quality = alpha * Average_Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of non-macro 10um by 10um density grids whose density exceeds `threshold`.

## Current Project State

The repository contains the assignment handout `p3_placement.pdf`, public OpenROAD inputs under `public/`, extraction and evaluation scripts (`extract.tcl`, `flow.tcl`), and local literature notes under `literature/`.

The current `Makefile` expects a modular C++17 source layout under `src/` and tests under `tests/`, but those directories are not present in the current checkout. A previously built `Legalizer` binary exists, but source should be reconstructed so the project can be built and submitted from source.

The assignment requires:

- Linux-compatible C or C++ implementation.
- Output TCL containing only explicit placement commands, not `detailed_placement`.
- All movable cells aligned to legal rows and sites.
- No cell rotation.
- No overlap with other cells, macros, or blockages.
- Runtime under 30 minutes per benchmark.

## Assumptions

- The implementation will use C++17 and the existing `Makefile` interface, with no external dependencies beyond the standard library.
- Input coordinates are in DBU, and output coordinates must be converted to microns for OpenROAD `place_cell -origin {X Y}`.
- Public cases appear primarily single-row-height for movable cells. The initial implementation will support standard single-row cells robustly and either handle simple multi-row cells conservatively or reject unsupported multi-row movable cells with a clear diagnostic.
- The legalizer will not call OpenROAD detailed placement in generated TCL.
- The source layout will follow the existing `Makefile` module names unless implementation discovers a strong reason to simplify them.

## Proposed Approach

### 1. Parse and Normalize the Placement Model

Implement a strict parser for the `.gp` format:

- Read `DBU_Per_Micron`, die bounds, site width, and site height.
- Parse all instance records with fields `Name LLX LLY Width Height Type`.
- Separate movable cells from fixed macros and blockages.
- Preserve each cell's original global placement coordinate for displacement scoring.
- Normalize row coordinates from the die lower-left Y to the die upper-right Y in `Site_Height` steps.
- Snap legal X coordinates to site columns and legal Y coordinates to site rows.

The internal model should store all geometry in integer DBU to avoid floating-point drift during legality checks.

### 2. Build Legal Row Intervals

For each placement row:

- Start with the full die-width row interval.
- Subtract intersections with every macro and blockage rectangle.
- Snap interval starts upward to the next legal site and interval ends downward to a legal site boundary.
- Discard intervals that are narrower than the minimum movable cell width.

This creates fragmented subrows similar to the row-fragment handling described in OpenROAD/OpenDP. All later algorithms operate only inside these intervals, which makes macro and blockage avoidance a structural property rather than a late correction.

### 3. Use Abacus as the Primary Legalization Engine

Use the Abacus legalizer as the main algorithm because the assignment heavily rewards low average displacement. Compared with Tetris, Abacus can move cells already assigned to a row when inserting a new cell, which usually gives significantly lower displacement.

The base flow:

1. Sort movable cells by original X coordinate.
2. For each cell, search candidate row intervals near the cell's original Y.
3. Tentatively insert the cell into each candidate interval in original-X order.
4. Run an Abacus `PlaceRow` cluster collapse for that interval.
5. Score the tentative placement by displacement delta and density impact.
6. Commit the best candidate.

The `PlaceRow` routine will:

- Maintain cells in row order.
- Create clusters when cells fit without overlap.
- Merge clusters when overlap occurs.
- Compute each cluster's optimal X from weighted original positions and cell widths.
- Clamp clusters to interval bounds.
- Expand merged clusters until all cells are non-overlapping and in-bounds.

Candidate-row search should use vertical displacement as a lower bound: once a good row has been found, rows whose vertical distance alone is already worse can be skipped. This keeps runtime practical on large benchmarks.

### 4. Adapt the Score to `alpha` and `threshold`

The program receives `alpha` and `threshold`, so candidate selection should change with the grading mode.

For high `alpha`, prioritize displacement:

- Prefer the candidate row with the smallest incremental average displacement.
- Apply density only as a tie-breaker or mild penalty.

For low `alpha`, prioritize density:

- Maintain a coarse 10um by 10um density grid matching the assignment's DOR definition.
- Estimate whether placing a cell in a candidate interval increases or decreases overflow-grid count.
- Penalize moves into bins already above `threshold`.
- Prefer row intervals near underfilled density regions when displacement cost is comparable.

The candidate score can use:

```text
candidate_cost =
    alpha * normalized_displacement_delta
    + (1 - alpha) * estimated_dor_delta
    + small_penalties_for_fragment_edge_or_high_local_density
```

This does not need to perfectly recompute full DOR for every trial; a lazy estimate is enough during insertion, followed by exact validation after each full pass.

### 5. Add Local Repair After Initial Legalization

After the first complete legal placement, run bounded improvement passes inspired by Ripple, OpenDP, and density-aware detailed placement literature:

- Identify cells with the largest displacement.
- Identify density bins above `threshold`.
- For each candidate cell, search a small window around its original coordinate.
- Try legal reinsertion into nearby row intervals using the same Abacus row solver.
- Try local swaps between nearby cells when the swap reduces final objective.
- Commit a move only if it improves the full objective or improves DOR with a bounded displacement increase.

This phase should be optional and time-bounded. It should stop when no improving moves are found or when a runtime budget is reached.

### 6. Run Multiple Deterministic Passes and Keep the Best

Because legalization order affects quality, run a small set of deterministic variants and keep the best legal result:

- Left-to-right by original X.
- Right-to-left by original X.
- Density-first ordering for low `alpha`, prioritizing cells in overflowing regions.
- Large-cell-first tie-breaks inside dense regions.

Each pass must produce a fully legal placement. The final output should be the legal placement with the lowest exact objective computed by the program's validation logic.

### 7. Validate Before Writing Output

Before generating the TCL file:

- Confirm every movable cell is inside the die.
- Confirm every movable cell is aligned to site X and row Y.
- Confirm no two movable cells overlap.
- Confirm no movable cell overlaps a macro or blockage.
- Recompute average displacement.
- Recompute DOR on 10um by 10um grids, excluding macro-covered grids.
- Reject or report any unsupported multi-row cell case before writing invalid TCL.

Only after validation passes should the program write:

```tcl
place_cell -inst_name <name> -orient R0 -origin {<x_micron> <y_micron>}
```

The output must never contain `detailed_placement`.

## Milestones

1. Recreate the C++17 project skeleton expected by the `Makefile`, including parser, geometry model, row interval builder, legalizer, density estimator, TCL writer, and tests.
2. Implement strict `.gp` parsing and row interval construction with macro/blockage subtraction.
3. Implement a baseline legalizer that always produces legal site-row placements for public cases.
4. Replace the baseline placement engine with Abacus cluster-based row optimization.
5. Add exact legality, displacement, and DOR validation before output.
6. Add density-aware candidate scoring controlled by `alpha` and `threshold`.
7. Add local repair passes for high-displacement cells and overflowing density bins.
8. Integrate with `flow.tcl` and benchmark both public cases under displacement-focused and DOR-focused parameter settings.

## Open Questions

- Hidden cases may include movable multi-row-height cells. The safest first implementation is to detect and reject unsupported multi-row movable cells clearly, but a stronger implementation could legalize them using a local-window method from the multi-row-height literature.
- The assignment handout defines the quality function directly, while `flow.tcl` normalizes average displacement with `norm_factor = 18.2`. The implementation should report both raw and flow-compatible objective estimates when validating locally.
- The exact hidden benchmark density behavior is unknown, so density-aware repair should be deterministic and conservative rather than overfit to public cases.

## Validation Plan

Validation should cover both unit-level geometry behavior and end-to-end OpenROAD flow behavior.

Unit tests:

- Parse valid and malformed `.gp` files.
- Snap coordinates to legal site rows.
- Split rows around macros and blockages.
- Run Abacus `PlaceRow` on simple overlap, cluster merge, and interval-boundary cases.
- Detect cell-cell, cell-macro, and cell-blockage overlaps.
- Compute average displacement and DOR on small synthetic examples.

End-to-end tests:

- Build with `make`.
- Run `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl` on small fixtures.
- Verify the generated TCL contains only `place_cell` commands.
- Run OpenROAD `flow.tcl` on `public/ispd19_sample` and `public/ispd15_mgc_matrix_mult_a`.
- Compare quality under at least two configurations: one displacement-focused high-`alpha` run and one density-focused low-`alpha` run.

The implementation is complete when both public cases pass OpenROAD `check_placement`, produce valid density CSVs, avoid prohibited commands, and finish comfortably under the 30-minute limit.
