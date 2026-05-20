# OpenROAD Detailed Placement / OpenDP

Source type: open-source tool and documentation  
Docs: https://openroad.readthedocs.io/en/latest/main/src/dpl/README.html  
Code: https://github.com/The-OpenROAD-Project/OpenROAD/tree/master/src/dpl

## Summary

OpenROAD's detailed placement module (`dpl`) is based on OpenDP. Documentation lists support for fragmented rows, fence regions, macro blocks, and mixed-cell-height legalization. The default engine performs a diamond/BFS-style search outward from each cell's global placement position. Current docs also describe a `NegotiationLegalizer` with an optional Abacus pre-pass.

## Relevance To `p3_placement.pdf`

The assignment forbids directly emitting `detailed_placement` in the final output TCL, so OpenDP cannot be used as the submitted legalizer command. It is still a legitimate reference for architecture, legality checks, row/site representation, local search, and post-placement improvement.

## Implementation Ideas

- Use diamond-search behavior as a simple local fallback for cells with high displacement.
- Add `improve_placement`-style local swaps in our own code after initial legalization.
- Study fragmented row/macro handling to improve row interval construction.
- Keep our output as explicit `place_cell` commands only.

## Caution

Do not call OpenROAD `detailed_placement` from generated TCL. The assignment says that violates the rules. Reading the public source/docs to understand techniques is different from calling the prohibited command.

