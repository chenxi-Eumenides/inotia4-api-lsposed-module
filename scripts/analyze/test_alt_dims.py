#!/usr/bin/env python3
"""尝试备选解读：width=byte2, height=byte3（而非 disassembly 的 byte3/byte4）。

重新解析所有 416 个 map 文件，与真机 runtime 矩阵对比。
"""
from __future__ import annotations
import base64
import json
import sys
import urllib.request
from pathlib import Path


BLOCKING_TILES_EXACT = frozenset({0xa1, 0xa8, 0xaf, 0xb2, 0x259, 0x264, 0x267, 0x273, 0x276, 0x6f6, 0x758})
BLOCKING_TILES_RANGES = (
    (0x8d, 0x8f), (0x95, 0x95), (0x99, 0x9e), (0xaa, 0xad),
    (0xb5, 0xb6), (0xb8, 0xbb), (0x23d, 0x23d), (0x24a, 0x24b),
)


def is_blocking(tile_id: int) -> bool:
    if tile_id in BLOCKING_TILES_EXACT:
        return True
    return any(lo <= tile_id <= hi for lo, hi in BLOCKING_TILES_RANGES)


def parse_alt(data: bytes) -> tuple[int, int, bytearray]:
    if len(data) < 5:
        return 0, 0, bytearray(64 * 64)
    w, h = data[2], data[3]
    pos = 5
    matrix = bytearray(64 * 64)
    for y in range(min(h, 64)):
        for x in range(min(w, 64)):
            if pos + 2 > len(data):
                return w, h, matrix
            b1, b2 = data[pos], data[pos + 1]
            pos += 2
            tile_id = ((b1 & 0x7) << 8) | b2
            matrix[y * 64 + x] = (b1 >> 4) | (0x40 if is_blocking(tile_id) else 0)
    return w, h, matrix


def fetch_runtime(ip: str, map_id: int) -> bytearray:
    base = f"http://{ip}:8088"
    with urllib.request.urlopen(f"{base}/api/info/current-map", timeout=5) as r:
        info = json.loads(r.read())
    if info["mapId"] != map_id:
        print(f"WARN: device at {mapId} (expected m{map_id})")
    with urllib.request.urlopen(f"{base}/api/info/current-map/tiles", timeout=5) as r:
        tiles = json.loads(r.read())
    return bytearray(base64.b64decode(tiles["tiles"]))


def main() -> None:
    ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.3.54"
    raw_dir = Path("static-data/raw")

    target = int(sys.argv[2]) if len(sys.argv) > 2 else 31
    data = (raw_dir / f"m{target}.dat.bin").read_bytes()
    w, h, off = parse_alt(data)
    run = fetch_runtime(ip, target)
    print(f"m{target}: alt_width={w} alt_height={h}")
    print(f"  off bit3={sum(1 for b in off if b & 0x08)} nonzero={sum(1 for b in off if b != 0)}")
    print(f"  run bit3={sum(1 for b in run if b & 0x08)} nonzero={sum(1 for b in run if b != 0)}")

    diffs = [(i, o, r) for i, (o, r) in enumerate(zip(off, run)) if o != r]
    print(f"  diffs: {len(diffs)}")
    if diffs and len(diffs) < 30:
        for i, o, r in diffs[:30]:
            print(f"    [{i:4d}] off=0x{o:02x} run=0x{r:02x}")


if __name__ == "__main__":
    main()
