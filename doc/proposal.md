# Proposal: OpenROAD Placement Legalizer

## Objective
Build a Linux command-line placement legalizer for Programming Assignment #3, "Placement with OpenROAD." The program will read an OpenROAD-extracted global placement file, move all standard cells to legal site-row aligned locations, avoid overlaps with cells, macros, and blockages, and emit an OpenROAD TCL script containing `place_cell` commands for the legalized cell origins.

The primary requirement is placement legality. Among legal placements, the implementation should minimize the assignment quality metric:

```text
Quality = alpha * Average Displacement + (1 - alpha) * DOR
```

where DOR is the percentage of non-macro 10um x 10um density grids whose utilization exceeds the supplied threshold.

## Current Project State
The project currently contains the assignment PDF, OpenROAD helper scripts, and public benchmark data:

- `p3_placement.pdf`: assignment specification.
- `flow.tcl`: reference OpenROAD flow that performs global placement, extracts `.gp` input, optionally runs detailed placement for debugging, checks legality, and computes displacement/DOR metrics.
- `extract.tcl`: exports DBU, die area, site dimensions, instance rectangles, macros, and blockages into the required `.gp` format.
- `public/ispd15_mgc_matrix_mult_a/`: public LEF/DEF benchmark.
- `public/ispd19_sample/`: public LEF/DEF benchmark.
- `public.tar`: packaged public benchmark archive.

No implementation source files, Makefile, or existing proposal file were observed before this proposal was created.

## Assumptions
- The implementation target is C++ on Linux because the assignment prefers C or C++ and requires a `make` build.
- The final executable name must be `Legalizer`.
- The TA will run:

```sh
make
./Legalizer <alpha> <threshold> <input>.gp <output>.tcl
```

- Input coordinates, sizes, and site dimensions are in DBU. Output `place_cell` origins should be emitted in microns for OpenROAD TCL, using `DBU_Per_Micron` for conversion.
- Only instances with type `CELL` are movable. `MACRO` and `BLOCKAGE` entries are fixed obstacles.
- Cell rotation is forbidden, so every output command should use `-orient R0`.
- The final output TCL must not invoke `detailed_placement`.

## Proposed Approach
Implement a standalone C++ legalizer with four main stages.

1. Parse the `.gp` input
   - Read `DBU_Per_Micron`, die lower-left/upper-right coordinates, `Site_Width`, and `Site_Height`.
   - Parse all instance records after the `Name LLX LLY Width Height Type` header.
   - Store movable cells separately from fixed obstacles.
   - Preserve each movable cell's original global-placement coordinate for displacement scoring.

2. Construct a row/site legality model
   - Derive placement rows from the die Y range and `Site_Height`.
   - Mark unavailable intervals in each row from macros and blockages intersecting that row.
   - Represent each row as sorted free intervals snapped to legal site coordinates.
   - Enforce that every cell origin is within the die, aligned to site rows, aligned to site columns, and contained in one free interval.

3. Legalize cells
   - Start from each cell's global-placement location and snap candidate positions to the legal site grid.
   - Process cells in a deterministic order, such as row-major by original Y/X position with optional width-aware tie breaking.
   - For each cell, search nearby rows and free intervals for the lowest-cost legal slot.
   - Use a cost function that combines Manhattan displacement and density pressure. The supplied `alpha` should bias the search toward lower displacement when high and toward lower local density overflow when low.
   - Maintain row occupancy intervals as cells are placed so later cells cannot overlap earlier cells.

4. Improve and emit the solution
   - Add local refinement passes that try to reduce average displacement and smooth density without breaking legality.
   - Optionally perform row-local compaction or window-based swaps for cells with high displacement.
   - Write one TCL command per movable cell:

```tcl
place_cell -inst_name <instName> -orient R0 -origin {X Y}
```

   - Do not output commands for macros or blockages.

## Milestones
1. Build the parser, data model, Makefile, and a minimal TCL writer that round-trips all movable cells.
2. Implement row/free-interval construction with macro and blockage exclusion.
3. Implement the first complete legal placement algorithm and verify legality through OpenROAD `check_placement`.
4. Add density-grid estimation using the assignment's 10um x 10um grid model and incorporate `alpha`/`threshold` into placement cost.
5. Add refinement passes, benchmark both public cases under multiple alpha/threshold settings, and tune for lower quality score while preserving legality.
6. Package the final source and Makefile under the required student-ID folder for submission.

## Open Questions
- Student ID and final submission folder name are not specified yet.
- Exact hidden-case scale is unknown, so the implementation should avoid algorithms worse than approximately O(cells * nearby_rows * candidate_intervals) in the common path and must complete each test within 30 minutes.
- The assignment describes average displacement in microns and DOR after macro removal from the heatmap count. The implementation can approximate DOR during search, but final scoring should be validated with the provided OpenROAD flow.

## Validation Plan
Use the provided OpenROAD flow as the source of truth for correctness and quality.

1. Generate `.gp` files from public benchmarks using `flow.tcl` and `extract.tcl`.
2. Run `make` and confirm it produces `./Legalizer`.
3. Run the required command shape:

```sh
./Legalizer <alpha> <threshold> <designName>_insts.gp <designName>_insts.tcl
```

4. Source the generated TCL in OpenROAD after global placement.
5. Confirm `check_placement -verbose` reports legality pass.
6. Compare total displacement, average displacement, max displacement, DOR, and final quality score against OpenROAD detailed placement used only for debugging.
7. Test both public benchmarks with at least two parameter profiles: one displacement-focused and one DOR-focused.
8. Inspect the generated TCL to ensure it contains only placement commands and never calls `detailed_placement`.
