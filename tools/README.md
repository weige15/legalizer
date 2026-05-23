# Test Case Tools

## Medium Synthetic DEF Case

`make_def_stress_case.py` defaults to generating a new medium-size DEF case
instead of copying a public DEF.  It copies LEF files from a source case, parses
the available standard-cell and block masters, then writes a synthetic DEF with
clustered initial cell placements.

Example:

```sh
python3 tools/make_def_stress_case.py \
  public/ispd15_mgc_matrix_mult_a \
  generated/medium_50k \
  --cells 50000 \
  --macros 4 \
  --blockages 4
```

The script enforces the TA discussion limits by default:

- `CELL <= 150000`
- `MACRO + BLOCKAGE <= 10`

Run the normal flow to exercise OpenROAD global placement plus the legalizer:

```sh
CASE_NAME=generated/medium_50k openroad -exit flow.tcl
```

Use `RUN_GP=0` only when you intentionally want to preserve the clustered
starting placement and stress the legalizer without global placement:

```sh
CASE_NAME=generated/medium_50k RUN_GP=0 openroad -exit flow.tcl
```

The old behavior is still available with `--mode cluster`.
