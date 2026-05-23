# Hand-Written GP Stress Cases

These `.gp` files are small enough to inspect by hand but target common
legalization bugs from `p3_placement_v2.pdf`.

Run a positive case with:

```sh
./Legalizer 0.7 45 tests/stress_nonzero_origin.gp tests/out_stress_nonzero_origin.tcl
```

Run the DOR-focused case with a low alpha:

```sh
./Legalizer 0.0 45 tests/stress_dor_cluster.gp tests/out_stress_dor_cluster.tcl
```

The intentionally impossible case should fail:

```sh
./Legalizer 0.7 45 tests/stress_over_capacity_should_fail.gp tests/out_stress_over_capacity_should_fail.tcl
```

## Cases

- `stress_nonzero_origin.gp`: nonzero die origin, off-grid original locations,
  cells outside the die, mirrored orientations, one macro, and one blockage.
- `stress_macro_corridor.gp`: full-height macro and partial macro obstacles
  split rows into separated corridors.
- `stress_dor_cluster.gp`: many wide cells start in the same 10um region, so
  low-alpha runs should care about spreading density instead of only movement.
- `stress_exact_capacity_fragmented.gp`: usable free space is fragmented by
  macros and blockages, with tight capacity.
- `stress_mixed_width_boundary.gp`: mixed-width cells start near or outside
  die boundaries, plus fixed obstacles in the middle.
- `stress_over_capacity_should_fail.gp`: total movable width exceeds available
  placement capacity after blockages, so a correct legalizer should report
  failure instead of emitting an illegal TCL.
