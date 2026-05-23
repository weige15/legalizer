#!/usr/bin/env python3
"""Create a medium-size synthetic DEF/LEF placement stress case.

By default this script copies LEF files from an existing case and writes a new
synthetic DEF with many movable standard cells plus a small number of fixed
macro/blockage obstacles.  The older public-case copy-and-cluster behavior is
still available with ``--mode cluster``.
"""

from __future__ import annotations

import argparse
import math
import random
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path


MAX_TA_CELLS = 150000
MAX_TA_OBSTACLES = 10

DIE_RE = re.compile(r"DIEAREA\s+\(\s+(-?\d+)\s+(-?\d+)\s+\)\s+\(\s+(-?\d+)\s+(-?\d+)\s+\)")
BLOCKAGES_RE = re.compile(r"^BLOCKAGES\s+(\d+)\s*;")
ROW_RE = re.compile(
    r"^ROW\s+\S+\s+\S+\s+(-?\d+)\s+(-?\d+)\s+(\S+)\s+DO\s+(\d+)\s+BY\s+\d+\s+STEP\s+(-?\d+)"
)
PLACEMENT_RE = re.compile(
    r"^(?P<indent>\s*)\+\s+(?P<status>UNPLACED|PLACED|FIXED|COVER)"
    r"(?:\s+\(\s+-?\d+\s+-?\d+\s+\)\s+\S+)?\s*;\s*$"
)
INLINE_COMPONENT_RE = re.compile(
    r"^(?P<prefix>\s*-\s+\S+\s+\S+.*?)\+\s+"
    r"(?P<status>UNPLACED|PLACED|FIXED|COVER)"
    r"(?:\s+\(\s+-?\d+\s+-?\d+\s+\)\s+\S+)?\s*;\s*$"
)
LEF_UNITS_RE = re.compile(r"DATABASE\s+MICRONS\s+(\d+)\s*;", re.IGNORECASE)
LEF_SITE_RE = re.compile(r"^SITE\s+(\S+)", re.IGNORECASE)
LEF_MACRO_RE = re.compile(r"^MACRO\s+(\S+)", re.IGNORECASE)
LEF_CLASS_RE = re.compile(r"CLASS\s+(\S+)\s*;", re.IGNORECASE)
LEF_SIZE_RE = re.compile(r"SIZE\s+([0-9.]+)\s+BY\s+([0-9.]+)\s*;", re.IGNORECASE)


@dataclass(frozen=True)
class LefMaster:
    name: str
    cls: str
    width_dbu: int
    height_dbu: int


@dataclass(frozen=True)
class LefInfo:
    dbu_per_micron: int
    site_name: str
    site_width_dbu: int
    site_height_dbu: int
    core_masters: list[LefMaster]
    block_masters: list[LefMaster]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a medium synthetic case, or cluster an existing public case."
    )
    parser.add_argument("source_case", type=Path, help="Existing case directory, e.g. public/ispd19_sample")
    parser.add_argument("dest_case", type=Path, help="New output case directory")
    parser.add_argument(
        "--mode",
        choices=("synthetic", "cluster"),
        default="synthetic",
        help="synthetic writes a new medium DEF; cluster copies and rewrites an existing DEF, default: synthetic",
    )
    parser.add_argument(
        "--cells",
        type=int,
        default=50000,
        help="Number of movable CELL components for synthetic mode, default: 50000",
    )
    parser.add_argument(
        "--macros",
        type=int,
        default=4,
        help="Number of fixed macro components for synthetic mode, default: 4",
    )
    parser.add_argument(
        "--blockages",
        type=int,
        default=4,
        help="Number of DEF placement blockages for synthetic mode, default: 4",
    )
    parser.add_argument(
        "--utilization",
        type=float,
        default=0.70,
        help="Approximate legal placement utilization for synthetic mode, default: 0.70",
    )
    parser.add_argument(
        "--design-name",
        default="medium_stress",
        help="Synthetic DEF design name, default: medium_stress",
    )
    parser.add_argument(
        "--cluster-x",
        type=float,
        default=0.05,
        help="Cluster lower-left x as a fraction of die width, default: 0.05",
    )
    parser.add_argument(
        "--cluster-y",
        type=float,
        default=0.05,
        help="Cluster lower-left y as a fraction of die height, default: 0.05",
    )
    parser.add_argument(
        "--cluster-width",
        type=float,
        default=0.12,
        help="Cluster width as a fraction of die width, default: 0.12",
    )
    parser.add_argument(
        "--cluster-rows",
        type=int,
        default=20,
        help="Number of nearby rows to use for clustered placements, default: 20",
    )
    parser.add_argument(
        "--max-components",
        type=int,
        default=0,
        help="Only move the first N movable components; 0 means all movable components",
    )
    parser.add_argument("--seed", type=int, default=7, help="Random seed for row jitter, default: 7")
    parser.add_argument(
        "--force",
        action="store_true",
        help="Allow overwriting files in an existing destination directory",
    )
    parser.add_argument(
        "--allow-over-limit",
        action="store_true",
        help="Allow cases that exceed the TA discussion limits",
    )
    return parser.parse_args()


def find_single_def(case_dir: Path) -> Path:
    defs = sorted(case_dir.glob("*.def"))
    if len(defs) != 1:
        names = ", ".join(str(path) for path in defs) or "none"
        raise RuntimeError(f"expected exactly one DEF in {case_dir}, found: {names}")
    return defs[0]


def copy_case(source: Path, dest: Path, force: bool) -> None:
    if not source.is_dir():
        raise RuntimeError(f"source case is not a directory: {source}")
    if dest.exists() and not force:
        raise RuntimeError(f"destination already exists, use --force to overwrite files: {dest}")
    shutil.copytree(source, dest, dirs_exist_ok=force)


def find_lef_files(case_dir: Path) -> list[Path]:
    lefs = sorted(case_dir.glob("*.lef"))
    if not lefs:
        raise RuntimeError(f"expected at least one LEF in {case_dir}")
    return lefs


def copy_lefs(source: Path, dest: Path, force: bool) -> list[Path]:
    if not source.is_dir():
        raise RuntimeError(f"source case is not a directory: {source}")
    if dest.exists() and not dest.is_dir():
        raise RuntimeError(f"destination exists but is not a directory: {dest}")
    if dest.exists() and not force:
        raise RuntimeError(f"destination already exists, use --force to overwrite LEF/DEF files: {dest}")
    dest.mkdir(parents=True, exist_ok=True)

    existing_defs = sorted(dest.glob("*.def"))
    synthetic_def = dest / "medium_stress.def"
    stale_defs = [path for path in existing_defs if path != synthetic_def]
    if stale_defs:
        names = ", ".join(str(path) for path in stale_defs)
        raise RuntimeError(
            "destination contains existing DEF files that could confuse flow.tcl: "
            f"{names}. Use a fresh destination or delete those files individually."
        )

    copied = []
    for lef in find_lef_files(source):
        target = dest / lef.name
        shutil.copy2(lef, target)
        copied.append(target)
    return copied


def parse_lef_info(lef_paths: list[Path]) -> LefInfo:
    dbu_per_micron = None
    site_name = None
    site_size = None
    masters: list[LefMaster] = []

    for lef_path in lef_paths:
        current_name = None
        current_class = ""
        current_size = None
        in_site = False
        site_candidate = None

        for raw_line in lef_path.read_text(errors="replace").splitlines():
            line = raw_line.strip()
            units_match = LEF_UNITS_RE.search(line)
            if units_match and dbu_per_micron is None:
                dbu_per_micron = int(units_match.group(1))

            site_match = LEF_SITE_RE.match(line)
            if site_match:
                in_site = True
                site_candidate = site_match.group(1)
                continue
            if in_site:
                size_match = LEF_SIZE_RE.search(line)
                if size_match and site_size is None:
                    site_name = site_candidate
                    site_size = (float(size_match.group(1)), float(size_match.group(2)))
                if line.upper().startswith("END "):
                    in_site = False
                continue

            macro_match = LEF_MACRO_RE.match(line)
            if macro_match:
                current_name = macro_match.group(1)
                current_class = ""
                current_size = None
                continue
            if current_name is None:
                continue
            class_match = LEF_CLASS_RE.search(line)
            if class_match:
                current_class = class_match.group(1).upper()
            size_match = LEF_SIZE_RE.search(line)
            if size_match:
                current_size = (float(size_match.group(1)), float(size_match.group(2)))
            if line.upper() == f"END {current_name}".upper():
                if current_size is not None:
                    if dbu_per_micron is None:
                        raise RuntimeError(f"LEF units must appear before macro sizes in {lef_path}")
                    masters.append(
                        LefMaster(
                            current_name,
                            current_class,
                            max(1, round(current_size[0] * dbu_per_micron)),
                            max(1, round(current_size[1] * dbu_per_micron)),
                        )
                    )
                current_name = None

    if dbu_per_micron is None:
        raise RuntimeError("LEF has no DATABASE MICRONS unit")
    if site_name is None or site_size is None:
        raise RuntimeError("LEF has no SITE with SIZE")

    site_width_dbu = max(1, round(site_size[0] * dbu_per_micron))
    site_height_dbu = max(1, round(site_size[1] * dbu_per_micron))
    core_masters = [
        master for master in masters
        if master.cls != "BLOCK" and master.height_dbu == site_height_dbu
    ]
    block_masters = [master for master in masters if master.cls == "BLOCK"]
    if not core_masters:
        raise RuntimeError("LEF has no single-row CORE masters for synthetic cells")

    return LefInfo(
        dbu_per_micron,
        site_name,
        site_width_dbu,
        site_height_dbu,
        core_masters,
        block_masters,
    )


def validate_synthetic_args(cells: int, macros: int, blockages: int, allow_over_limit: bool) -> None:
    errors = []
    if cells <= 0:
        errors.append("--cells must be positive")
    if macros < 0 or blockages < 0:
        errors.append("--macros and --blockages must be non-negative")
    if cells > MAX_TA_CELLS:
        errors.append(f"CELL count {cells} exceeds {MAX_TA_CELLS}")
    if macros + blockages > MAX_TA_OBSTACLES:
        errors.append(f"MACRO+BLOCKAGE count {macros + blockages} exceeds {MAX_TA_OBSTACLES}")
    if errors and not allow_over_limit:
        raise RuntimeError("; ".join(errors) + "; pass --allow-over-limit to create it anyway")


def synthetic_dimensions(
    lef: LefInfo,
    cells: int,
    utilization: float,
    rng: random.Random,
) -> tuple[int, int, int]:
    if utilization <= 0 or utilization >= 1:
        raise RuntimeError("--utilization must be between 0 and 1")
    sample = [rng.choice(lef.core_masters) for _ in range(min(cells, 1000))]
    avg_width = sum(master.width_dbu for master in sample) / len(sample)
    total_sites = math.ceil((cells * avg_width / lef.site_width_dbu) / utilization)
    row_count = max(20, math.ceil(math.sqrt(total_sites / 1.8)))
    row_sites = max(50, math.ceil(total_sites / row_count))
    return row_count, row_sites, total_sites


def write_synthetic_def(
    dest: Path,
    lef: LefInfo,
    design_name: str,
    cells: int,
    macros: int,
    blockages: int,
    utilization: float,
    seed: int,
) -> Path:
    rng = random.Random(seed)
    row_count, row_sites, _ = synthetic_dimensions(lef, cells, utilization, rng)
    macro_masters = lef.block_masters[:macros]
    effective_macros = len(macro_masters)
    if effective_macros < macros:
        print(
            f"warning: LEF has only {effective_macros} BLOCK masters; using "
            f"{effective_macros} macros and {blockages} blockages",
            file=sys.stderr,
        )
    if macro_masters:
        min_macro_width = max(master.width_dbu for master in macro_masters) + lef.site_width_dbu * 20
        min_macro_height = max(master.height_dbu for master in macro_masters) + lef.site_height_dbu * 20
        row_sites = max(row_sites, math.ceil(min_macro_width / lef.site_width_dbu))
        row_count = max(row_count, math.ceil(min_macro_height / lef.site_height_dbu))

    die_ux = row_sites * lef.site_width_dbu
    die_uy = row_count * lef.site_height_dbu
    cluster_rows = max(5, min(25, row_count // 8))
    cluster_sites = max(20, row_sites // 8)

    lines = [
        "VERSION 5.8 ;\n",
        'DIVIDERCHAR "/" ;\n',
        'BUSBITCHARS "[]" ;\n',
        f"DESIGN {design_name} ;\n",
        f"UNITS DISTANCE MICRONS {lef.dbu_per_micron} ;\n",
        "\n",
        f"DIEAREA ( 0 0 ) ( {die_ux} {die_uy} ) ;\n",
        "\n",
    ]
    for row in range(row_count):
        orient = "N" if row % 2 == 0 else "FS"
        y = row * lef.site_height_dbu
        lines.append(
            f"ROW {lef.site_name}_ROW_{row} {lef.site_name} 0 {y} {orient} "
            f"DO {row_sites} BY 1 STEP {lef.site_width_dbu} 0 ;\n"
        )

    lines.extend(["\n", f"COMPONENTS {cells + effective_macros} ;\n"])
    for index, master in enumerate(macro_masters):
        x = ((index + 1) * die_ux) // (effective_macros + 1) - master.width_dbu // 2
        y = ((index % 3) + 1) * die_uy // 5
        x = max(0, min(x, die_ux - master.width_dbu))
        y = max(0, min(y, die_uy - master.height_dbu))
        lines.append(f"- macro_{index} {master.name} + FIXED ( {x} {y} ) N ;\n")

    for index in range(cells):
        master = rng.choice(lef.core_masters)
        row = index % cluster_rows
        site = (index * 3 + rng.randrange(cluster_sites)) % cluster_sites
        x = site * lef.site_width_dbu
        y = row * lef.site_height_dbu
        orient = "N" if row % 2 == 0 else "FS"
        lines.append(f"- cell_{index} {master.name} + PLACED ( {x} {y} ) {orient} ;\n")
    lines.extend(["END COMPONENTS\n", "\n", "PINS 0 ;\n", "END PINS\n", "\n", "NETS 0 ;\n", "END NETS\n"])

    if blockages > 0:
        lines.extend(["\n", f"BLOCKAGES {blockages} ;\n"])
        blockage_w = max(lef.site_width_dbu * 20, die_ux // 20)
        blockage_h = max(lef.site_height_dbu * 4, die_uy // 30)
        for index in range(blockages):
            x = ((index + 1) * die_ux) // (blockages + 1) - blockage_w // 2
            y = ((index % 4) + 1) * die_uy // 6
            x = max(0, min(x, die_ux - blockage_w))
            y = max(0, min(y, die_uy - blockage_h))
            lines.append(
                f"- PLACEMENT + RECT ( {x} {y} ) ( {x + blockage_w} {y + blockage_h} ) ;\n"
            )
        lines.append("END BLOCKAGES\n")

    lines.append("\nEND DESIGN\n")
    def_path = dest / f"{design_name}.def"
    def_path.write_text("".join(lines))
    return def_path


def parse_die_and_rows(lines: list[str]) -> tuple[tuple[int, int, int, int], list[tuple[int, int, str, int]]]:
    die = None
    rows = []
    for line in lines:
        die_match = DIE_RE.search(line)
        if die_match:
            die = tuple(int(value) for value in die_match.groups())
            continue
        row_match = ROW_RE.match(line.strip())
        if row_match:
            x, y, orient, count, step = row_match.groups()
            rows.append((int(x), int(y), orient, int(step)))
    if die is None:
        raise RuntimeError("DEF has no DIEAREA")
    if not rows:
        raise RuntimeError("DEF has no ROW records")
    return die, rows


def count_def_objects(lines: list[str]) -> tuple[int, int, int]:
    movable_components = 0
    fixed_or_cover_components = 0
    blockages = 0
    in_components = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("COMPONENTS "):
            in_components = True
            continue
        if stripped == "END COMPONENTS":
            in_components = False
            continue

        blockages_match = BLOCKAGES_RE.match(stripped)
        if blockages_match:
            blockages = int(blockages_match.group(1))

        if not in_components:
            continue

        inline_match = INLINE_COMPONENT_RE.match(line)
        placement_match = PLACEMENT_RE.match(line)
        match = inline_match if inline_match is not None else placement_match
        if match is None:
            continue
        if match.group("status") in {"FIXED", "COVER"}:
            fixed_or_cover_components += 1
        else:
            movable_components += 1

    return movable_components, fixed_or_cover_components, blockages


def validate_discussion_limits(def_path: Path, allow_over_limit: bool) -> tuple[int, int, int]:
    lines = def_path.read_text().splitlines(keepends=True)
    movable_components, fixed_or_cover_components, blockages = count_def_objects(lines)
    obstacle_estimate = fixed_or_cover_components + blockages

    errors = []
    if movable_components > MAX_TA_CELLS:
        errors.append(
            f"estimated movable CELL count {movable_components} exceeds {MAX_TA_CELLS}"
        )
    if obstacle_estimate > MAX_TA_OBSTACLES:
        errors.append(
            "estimated MACRO+BLOCKAGE count "
            f"{obstacle_estimate} exceeds {MAX_TA_OBSTACLES} "
            f"({fixed_or_cover_components} fixed/cover components + {blockages} DEF blockages)"
        )
    if errors and not allow_over_limit:
        joined = "; ".join(errors)
        raise RuntimeError(f"{joined}; pass --allow-over-limit to create it anyway")

    return movable_components, fixed_or_cover_components, blockages


def choose_cluster_rows(
    rows: list[tuple[int, int, str, int]],
    die: tuple[int, int, int, int],
    cluster_y: float,
    cluster_rows: int,
) -> list[tuple[int, int, str, int]]:
    _, die_ly, _, die_uy = die
    target_y = die_ly + int((die_uy - die_ly) * cluster_y)
    ordered = sorted(rows, key=lambda row: abs(row[1] - target_y))
    chosen = ordered[: max(1, min(cluster_rows, len(ordered)))]
    return sorted(chosen, key=lambda row: row[1])


def rewrite_def(
    def_path: Path,
    cluster_x: float,
    cluster_y: float,
    cluster_width: float,
    cluster_rows: int,
    max_components: int,
    seed: int,
) -> int:
    lines = def_path.read_text().splitlines(keepends=True)
    die, rows = parse_die_and_rows(lines)
    die_lx, _, die_ux, _ = die
    width = die_ux - die_lx
    if width <= 0:
        raise RuntimeError("DEF die width must be positive")

    chosen_rows = choose_cluster_rows(rows, die, cluster_y, cluster_rows)
    rng = random.Random(seed)
    x_start = die_lx + int(width * cluster_x)
    x_span = max(1, int(width * cluster_width))
    site_step = max(1, min(abs(row[3]) for row in rows if row[3] != 0))
    moved = 0
    in_components = False
    new_lines = []

    def next_location() -> tuple[int, int, str]:
        nonlocal moved
        row = chosen_rows[(moved + rng.randrange(len(chosen_rows))) % len(chosen_rows)]
        slots = max(1, x_span // site_step)
        x = x_start + ((moved * 3 + rng.randrange(slots)) % slots) * site_step
        moved += 1
        return x, row[1], row[2]

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("COMPONENTS "):
            in_components = True
            new_lines.append(line)
            continue
        if stripped == "END COMPONENTS":
            in_components = False
            new_lines.append(line)
            continue

        inline_match = INLINE_COMPONENT_RE.match(line) if in_components else None
        if inline_match is not None:
            if inline_match.group("status") in {"FIXED", "COVER"}:
                new_lines.append(line)
                continue
            if max_components > 0 and moved >= max_components:
                new_lines.append(line)
                continue
            x, y, orient = next_location()
            new_lines.append(f"{inline_match.group('prefix')}+ PLACED ( {x} {y} ) {orient} ;\n")
            continue

        match = PLACEMENT_RE.match(line) if in_components else None
        if match is None or match.group("status") in {"FIXED", "COVER"}:
            new_lines.append(line)
            continue
        if max_components > 0 and moved >= max_components:
            new_lines.append(line)
            continue

        x, y, orient = next_location()
        new_lines.append(f"{match.group('indent')}+ PLACED ( {x} {y} ) {orient} ;\n")

    if moved == 0:
        raise RuntimeError("no movable UNPLACED/PLACED components were rewritten")
    def_path.write_text("".join(new_lines))
    return moved


def main() -> int:
    args = parse_args()
    try:
        if args.mode == "synthetic":
            validate_synthetic_args(args.cells, args.macros, args.blockages, args.allow_over_limit)
            copied_lefs = copy_lefs(args.source_case, args.dest_case, args.force)
            lef = parse_lef_info(copied_lefs)
            def_path = write_synthetic_def(
                args.dest_case,
                lef,
                args.design_name,
                args.cells,
                args.macros,
                args.blockages,
                args.utilization,
                args.seed,
            )
            movable_components = args.cells
            fixed_or_cover_components, blockages = validate_discussion_limits(
                def_path, args.allow_over_limit
            )[1:]
            moved = args.cells
        else:
            copy_case(args.source_case, args.dest_case, args.force)
            def_path = find_single_def(args.dest_case)
            movable_components, fixed_or_cover_components, blockages = validate_discussion_limits(
                def_path, args.allow_over_limit
            )
            moved = rewrite_def(
                def_path,
                args.cluster_x,
                args.cluster_y,
                args.cluster_width,
                args.cluster_rows,
                args.max_components,
                args.seed,
            )
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Created {args.mode} stress case: {args.dest_case}")
    print(f"Rewritten DEF      : {def_path}")
    print(f"Estimated CELLs    : {movable_components}")
    print(
        "Estimated obstacles: "
        f"{fixed_or_cover_components + blockages} "
        f"({fixed_or_cover_components} fixed/cover components + {blockages} DEF blockages)"
    )
    print(f"Moved components   : {moved}")
    print("Run with RUN_GP=0 so OpenROAD does not overwrite the initial stressed placement.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
