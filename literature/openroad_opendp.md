# OpenROAD Detailed Placement / OpenDP

Source type: official documentation and open-source tool

Docs: https://openroad.readthedocs.io/en/latest/main/src/dpl/README.html

Code: https://github.com/The-OpenROAD-Project/OpenROAD/tree/master/src/dpl

## Summary

OpenROAD's detailed placement module (`dpl`) is based on OpenDP. The official docs list support for fragmented rows, fence regions, macro blocks, mixed-cell-height legalization, and two placement engines. The default engine performs a diamond/BFS-style search outward from each cell's global placement position. The optional negotiation legalizer uses a two-pass strategy with an optional Abacus pre-pass.

## Relevance To The Assignment

The assignment is built around OpenROAD and emits OpenROAD TCL. OpenDP is therefore the most practical reference for:

- row and site representation,
- fragmented rows caused by macros/blockages,
- legality checking,
- local search from global-placement coordinates,
- post-legalization improvement.

## Important Constraint

The assignment explicitly forbids using `detailed_placement` in the final output TCL. Reading public docs/source to understand techniques is different from calling the prohibited command. The submitted output should contain explicit `place_cell` commands only.

## Implementation Ideas

- Use diamond-search behavior as a simple fallback for cells that Abacus places far away.
- Add local swaps after legalization, similar in spirit to detailed-placement improvement passes.
- Use OpenDP's fragmented row model as a sanity check for row interval construction.
- Add a `check_placement`-like internal validator before writing the final TCL.

