# Benchmark Validation Notes

Local validation does not require OpenROAD:

```sh
make
make test
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

When OpenROAD is installed, validate a public benchmark with the assignment flow:

```sh
openroad flow.tcl
```

For assignment runs, edit `flow.tcl` step 4 to replace `detailed_placement` with:

```tcl
exec make
exec timeout 30m ./Legalizer 0.7 45 <input>.gp <output>.tcl
source <output>.tcl
```

Then keep the existing `check_placement -verbose`, heatmap dump, displacement, DOR, normalized displacement, and final quality reporting in `flow.tcl`.

Suggested public benchmark matrix:

```text
ispd19_sample: alpha=0.7 threshold=45
ispd19_sample: alpha=0.3 threshold=35
ispd15_mgc_matrix_mult_a: alpha=0.7 threshold=45
ispd15_mgc_matrix_mult_a: alpha=0.3 threshold=35
```

Record the OpenROAD log values for legality, total displacement, average displacement, max displacement, total grids, overflow grids, DOR, normalized displacement, final quality score, and runtime. OpenROAD was not available during this implementation pass, so public benchmark metrics are intentionally left unreported rather than guessed.
