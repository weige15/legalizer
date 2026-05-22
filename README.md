# Placement Legalizer

This repository contains a C++17 placement legalizer for Programming Assignment 3, "Placement with OpenROAD." The tool reads an OpenROAD-extracted `.gp` placement file, legalizes movable cells onto site rows while avoiding fixed macros and blockages, and writes an OpenROAD TCL script with `place_cell` commands.

## Build

```sh
make
```

This creates the `Legalizer` executable in the repository root.

## Run

```sh
./Legalizer <alpha> <threshold> <input.gp> <output.tcl>
```

Example:

```sh
./Legalizer 0.7 45 input.gp output.tcl
```

Arguments:

- `alpha`: weight for average displacement in the assignment quality metric.
- `threshold`: density overflow threshold percentage.
- `input.gp`: extracted global placement input.
- `output.tcl`: generated OpenROAD placement script.

The tool validates the input, legalizes all movable `CELL` rows, validates the
final placement, and atomically replaces `output.tcl` only after a valid
placement is ready. Multi-row-height movable cells are rejected with a named
diagnostic rather than emitted as knowingly illegal placements.

## Test

```sh
make test
```

The test target builds and runs `tests/test_legalizer`.

It also smoke-tests the required executable interface:

```sh
./Legalizer 0.7 45 tests/fixture_one_cell.gp tests/out_one_cell.tcl
```

## OpenROAD Flow

`flow.tcl` defaults to running both public cases,
`public/ispd15_mgc_matrix_mult_a` and `public/ispd19_sample`, in isolated
OpenROAD child processes. Set `CASE_NAME` to run one case, or `CASES` to run a
custom Tcl list of case directories.

For each case, the flow builds `Legalizer`, runs it on the extracted
`${design_name}_insts.gp`, checks that a nonempty output TCL was created,
rejects output containing `detailed_placement`, and then sources the generated
placement commands. Build, invocation, output, and source failures are reported
with explicit diagnostics.

After each successful case, the flow reports average displacement, DOR, and the
weighted quality score used by the assignment:

```text
Quality = alpha * average_displacement + (1 - alpha) * DOR
```

The same metrics are also written to `flow_metrics.csv` in the repository root.
Set `METRICS_CSV=path/to/file.csv` to choose another file, or `METRICS_CSV=` to
disable CSV output.

## Clean

```sh
make clean
```

## Repository Notes

- Source code is under `src/`.
- Unit tests are under `tests/`.
- Design notes and task planning documents are under `doc/`.
- Public sample inputs are under `public/`.
- Build products and local cache directories are ignored by git.
