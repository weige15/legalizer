#!/usr/bin/env python3
"""Create a copied DEF/LEF case with movable components clustered together.

The source case is never modified. The destination case is a copy of the source
directory, then the DEF inside the destination is rewritten so non-FIXED
components are assigned dense PLACED locations on existing DEF rows.
"""

from __future__ import annotations

import argparse
import random
import re
import shutil
import sys
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Copy a public LEF/DEF case and cluster movable DEF components."
    )
    parser.add_argument("source_case", type=Path, help="Existing case directory, e.g. public/ispd19_sample")
    parser.add_argument("dest_case", type=Path, help="New output case directory")
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

    print(f"Created stress case: {args.dest_case}")
    print(f"Rewritten DEF      : {def_path}")
    print(f"Estimated CELLs    : {movable_components}")
    print(
        "Estimated obstacles: "
        f"{fixed_or_cover_components + blockages} "
        f"({fixed_or_cover_components} fixed/cover components + {blockages} DEF blockages)"
    )
    print(f"Moved components   : {moved}")
    print("Run with RUN_GP=0 so OpenROAD does not overwrite the clustered DEF placement.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
