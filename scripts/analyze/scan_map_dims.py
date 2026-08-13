#!/usr/bin/env python3
"""扫描 416 个 map 文件尺寸分布，确认 matrix 边界。

按 MAP_Load 0x1149d4 反汇编：
- byte 0 skip, byte 3=width, byte 4=height
- matrix 64x64 (从 MAP_IsBlocking y*64+x 索引确认)
- 文件 size 决定实际能装多少 base layer cell

运行：uv run python scripts/analyze/scan_map_dims.py
"""
from __future__ import annotations
import sys
from pathlib import Path
from collections import Counter


def main() -> None:
    raw_dir = Path("apk/static-data/raw")
    if not raw_dir.exists():
        print(f"ERROR: {raw_dir} not found", file=sys.stderr)
        sys.exit(1)

    files = sorted(raw_dir.glob("m*.dat.bin"))
    print(f"Total map files: {len(files)}")
    print()

    widths: list[int] = []
    heights: list[int] = []
    sizes: list[int] = []
    overflow_count = 0
    empty_count = 0

    for f in files:
        data = f.read_bytes()
        if len(data) < 5:
            continue
        _b1, _b2, width, height = data[1], data[2], data[3], data[4]
        widths.append(width)
        heights.append(height)
        sizes.append(len(data))

        expected_tile_data = width * height * 2
        actual_capacity = (len(data) - 5) // 2
        if actual_capacity < width * 64:
            overflow_count += 1
        if height == 0:
            empty_count += 1

    print(f"Width range: {min(widths)} - {max(widths)} (avg {sum(widths)/len(widths):.1f})")
    print(f"Height range: {min(heights)} - {max(heights)} (avg {sum(heights)/len(heights):.1f})")
    print(f"Size range: {min(sizes)} - {max(sizes)} bytes")
    print()
    print(f"Maps with height=0 (empty base layer): {empty_count}")
    print(f"Maps where file too small to fill 64 rows: {overflow_count}")
    print()

    width_hist = Counter(widths)
    print("Width distribution (top 10):")
    for w, c in width_hist.most_common(10):
        print(f"  width={w}: {c} maps")
    print()

    height_hist = Counter(heights)
    print("Height distribution (top 10):")
    for h, c in height_hist.most_common(10):
        print(f"  height={h}: {c} maps")
    print()

    print(f"Max width × 64 cells needed: {max(widths) * 64 * 2} bytes")
    print(f"Max file size: {max(sizes)} bytes")
    print()

    matrix_bytes = len(files) * 64 * 64
    matrix_kb = matrix_bytes / 1024
    matrix_mb = matrix_bytes / 1024 / 1024
    b64_bytes = matrix_bytes * 4 // 3
    print(f"Total matrix storage (64x64 per map): {matrix_bytes} bytes = {matrix_kb:.1f} KB = {matrix_mb:.2f} MB")
    print(f"Base64 encoded: {b64_bytes} bytes = {b64_bytes/1024:.1f} KB")


if __name__ == "__main__":
    main()
