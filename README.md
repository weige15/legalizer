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
- `threshold`: density overflow threshold.
- `input.gp`: extracted global placement input.
- `output.tcl`: generated OpenROAD placement script.

## Test

```sh
make test
```

The test target builds and runs `tests/test_legalizer`.

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
