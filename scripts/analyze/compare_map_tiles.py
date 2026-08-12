#!/usr/bin/env python3
"""对比离线解析的 tiles.json 与 frida dump 的 tiles_frida.json。

输出：每图的差异位统计（bit 3/bit 6 差异 + 字节差异数）
成功标准：每个 map 64x64 矩阵完全一致（或仅 bit 7 等"已知非阻挡"位差异）

运行：uv run python scripts/analyze/compare_map_tiles.py
"""
from __future__ import annotations
import base64
import json
import sys
from pathlib import Path


def main() -> None:
    offline_path = Path("static-data/json/maps/tiles.json")
    frida_path = Path("static-data/json/maps/tiles_frida.json")
    if not offline_path.exists():
        print(f"ERROR: {offline_path} not found", file=sys.stderr)
        sys.exit(1)
    if not frida_path.exists():
        print(f"ERROR: {frida_path} not found (run dump_all_map_tiles.py first)", file=sys.stderr)
        sys.exit(1)

    with open(offline_path) as f:
        offline = json.load(f)
    with open(frida_path) as f:
        frida_data = json.load(f)

    if set(offline.keys()) != set(frida_data.keys()):
        print(f"WARN: key sets differ (offline={len(offline)} frida={len(frida_data)})")

    total = 0
    identical = 0
    diff_in_bit3 = 0
    diff_in_other = 0
    examples: list[tuple[str, int, int]] = []

    for key in sorted(offline.keys() & frida_data.keys()):
        off_bytes = bytearray(base64.b64decode(offline[key]["tiles"]))
        fri_bytes = bytearray(base64.b64decode(frida_data[key]["tiles"]))
        total += 1
        if off_bytes == fri_bytes:
            identical += 1
            continue
        b3_diff = sum(1 for o, f in zip(off_bytes, fri_bytes) if (o & 0x08) != (f & 0x08))
        other_diff = sum(1 for o, f in zip(off_bytes, fri_bytes) if o != f) - b3_diff
        if b3_diff > 0 and other_diff == 0:
            diff_in_bit3 += 1
        else:
            diff_in_other += 1
        if len(examples) < 10:
            examples.append((key, b3_diff, other_diff))

    print(f"Total compared: {total}")
    print(f"Identical: {identical} ({100 * identical / total:.1f}%)")
    print(f"Differ in bit3 only: {diff_in_bit3}")
    print(f"Differ in other bits too: {diff_in_other}")
    print()
    if examples:
        print("Sample differences (mapId, bit3-diffs, other-diffs):")
        for key, b3, o in examples:
            print(f"  {key}: bit3={b3}, other={o}")


if __name__ == "__main__":
    main()
