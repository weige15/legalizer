# Proposal: Placement Legalizer for OpenROAD

## Objective
Build a Linux C++ placement legalizer for Programming Assignment #3, "Placement with OpenROAD." The program must read an extracted `.gp` placement file, move all movable standard cells to legal site-row locations, avoid overlaps with other cells, fixed macros, and blockages, and write an OpenROAD TCL script containing `place_cell` commands for the legalized placement.

The implementation should optimize the assignment quality metric:

`Quality = alpha * Average Displacement + (1 - alpha) * DOR`

where average displacement measures movement from the global-placement coordinates, and DOR is the percentage of non-macro 10 micron by 10 micron density grids whose density exceeds the provided threshold.

## Current Project State
The repository contains the assignment PDF, OpenROAD helper scripts, public LEF/DEF benchmarks, a `README.md`, and an existing compiled `Legalizer` binary.

Observed files:

- `p3_placement.pdf`: official assignment specification.
- `flow.tcl`: OpenROAD test flow that extracts `.gp`, runs `make`, executes `./Legalizer <alpha> <threshold> <input>.gp <output>.tcl`, sources the output TCL, checks placement legality, and computes displacement and DOR.
- `extract.tcl`: OpenROAD extraction script that writes the assignment `.gp` input format from LEF/DEF data.
- `public/`: public benchmark inputs.
- `README.md`: describes a C++17 project with `src/`, `tests/`, and `make test`, but those source/test directories and Makefile are not present in the current checkout.

## Assumptions
The project should be reconstructed as a C++17 assignment submission, not a Python or uv project, because the assignment prefers C/C++ and the TA runs `make` followed by `./Legalizer`.

The final submitted folder must include source code and a Makefile capable of building a root-level `Legalizer` executable.

The output TCL must not call OpenROAD `detailed_placement`; it may only place cells directly with `place_cell` commands.

All movable cells must keep orientation `R0`; cell rotation is forbidden by the assignment.

Coordinates in the `.gp` input are in database units, while the sample TCL output uses micron coordinates. The implementation must convert output origins using `DBU_Per_Micron` so that OpenROAD receives coordinates in the expected units.

## Proposed Approach
Create a compact C++17 legalizer with clear separation between parsing, placement data modeling, legalization, density estimation, and TCL emission.

The parser should read:

- `DBU_Per_Micron`
- die lower-left and upper-right coordinates
- site width and site height
- instance rows with `Name LLX LLY Width Height Type`

The internal model should classify entries as:

- movable cells: `Type == CELL`
- fixed obstacles: `Type == MACRO` or `Type == BLOCKAGE`

The legalizer should:

- Snap every movable cell to a valid site-aligned row and site-aligned X coordinate inside the die.
- Precompute row intervals that remain usable after subtracting fixed macro and blockage spans.
- Place cells into legal row intervals without overlap.
- Start from each cell's global-placement coordinate to keep displacement small.
- Use `alpha` and `threshold` to tune the tradeoff between displacement and density distribution.

A practical first implementation should use row-based candidate search:

- Generate legal candidate rows near each cell's original Y coordinate.
- For each row, find candidate X slots near the original X coordinate within available intervals.
- Score candidates by displacement plus a density penalty based on estimated 10 micron grid occupancy.
- Commit each cell to the best available candidate and update row occupancy and density estimates.

The density strategy should discourage local overcrowding by tracking approximate occupied area per 10 micron grid, excluding grids fully covered by fixed macros from overflow accounting where practical. The implementation does not need to exactly reproduce OpenROAD's GUI heatmap calculation, but it should guide placements toward lower DOR under the assignment metric.

The output writer should emit one line per movable cell:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

where `X` and `Y` are converted from DBU to microns.

## Milestones
1. Recreate the buildable project skeleton with `Makefile`, `src/`, and focused C++ modules for data structures, parsing, legalization, and output.
2. Implement strict `.gp` parsing and TCL emission that preserves all movable cell names and produces assignment-compatible command-line behavior.
3. Implement baseline row legalization that guarantees die containment, site-row alignment, and non-overlap against cells, macros, and blockages.
4. Add density-aware candidate scoring controlled by `<alpha>` and `<threshold>` to improve DOR without excessive displacement.
5. Add public benchmark validation through `flow.tcl`, plus small unit or fixture tests for parser behavior, row interval construction, overlap checks, and output conversion.

## Open Questions
None. The assignment specification, current scripts, and TA command line define the required behavior.

## Validation Plan
Build with:

```sh
make
```

Run the legalizer using the exact TA interface:

```sh
./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl
```

Validate through OpenROAD with `flow.tcl` on both public benchmarks and at least two parameter configurations, including one displacement-heavy run and one density-heavy run.

The final implementation should pass:

- OpenROAD `check_placement -verbose`
- no cell overlaps
- no placement outside the die
- all cells aligned to legal site rows
- all cells retaining original orientation
- no `detailed_placement` command in generated TCL
- runtime under 30 minutes per benchmark

Quality should be tracked using the metrics printed by `flow.tcl`: total displacement, average displacement, DOR, normalized displacement, and final quality score.
