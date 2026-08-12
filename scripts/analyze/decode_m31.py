#!/usr/bin/env python3
"""深度分析 m31 文件 → runtime matrix 的编码。

穷举可能的 byte→matrix 编码：byte1, byte2, byte1<<4, byte2<<4, byte1|byte2, 等
"""
from __future__ import annotations
import base64
import json
import sys
import urllib.request
from pathlib import Path


def fetch_runtime_tiles(ip: str) -> tuple[int, bytearray]:
    with urllib.request.urlopen(f"http://{ip}:8088/api/info/current-map", timeout=5) as r:
        info = json.loads(r.read())
    map_id = info["mapId"]
    with urllib.request.urlopen(f"http://{ip}:8088/api/info/current-map/tiles", timeout=5) as r:
        tiles = json.loads(r.read())
    return map_id, bytearray(base64.b64decode(tiles["tiles"]))


def main() -> None:
    ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.3.54"
    map_id, runtime = fetch_runtime_tiles(ip)
    data = (Path("static-data/raw") / f"m{map_id}.dat.bin").read_bytes()

    print(f"m{map_id}: file={len(data)} bytes, runtime_matrix={len(runtime)} bytes")
    print(f"runtime bit3_set={sum(1 for b in runtime if b & 0x08)} nonzero={sum(1 for b in runtime if b != 0)}")
    print()

    print("File first 32 bytes (5-byte header + 27 bytes):")
    print(" ".join(f"{b:02x}" for b in data[:32]))
    print()
    print("Runtime first 32 bytes of matrix:")
    print(" ".join(f"{b:02x}" for b in runtime[:32]))
    print()

    print("Mapping file pair (byte1, byte2) → runtime matrix byte:")
    print(f"{'idx':<5} {'byte1':<6} {'byte2':<6} {'tile_id':<8} {'matrix':<6} {'bit3':<5} {'bit7':<5} {'bit6':<5}")
    for i in range(32):
        if 5 + 2*i + 1 >= len(data):
            break
        b1 = data[5 + 2*i]
        b2 = data[5 + 2*i + 1]
        tile_id = ((b1 & 0x7) << 8) | b2
        m = runtime[i]
        print(f"{i:<5} 0x{b1:02x}   0x{b2:02x}   0x{tile_id:04x}   0x{m:02x}    {(m>>3)&1:<5} {(m>>7)&1:<5} {(m>>6)&1:<5}")

    print()
    print("Pattern check: For cells 3-31 (runtime 0x08), what determines 0x08?")
    for i in range(3, 32):
        b1 = data[5 + 2*i]
        b2 = data[5 + 2*i + 1]
        m = runtime[i]
        candidates = []
        if (b1 & 0x08) != 0:
            candidates.append("byte1.bit3")
        if (b2 & 0x08) != 0:
            candidates.append("byte2.bit3")
        if b1 & 0x80:
            candidates.append("byte1.hi")
        if b2 & 0x80:
            candidates.append("byte2.hi")
        tile_id = ((b1 & 0x7) << 8) | b2
        if tile_id in {0x8d, 0x8e, 0x8f, 0x95} or (0x99 <= tile_id <= 0x9e) or (0xaa <= tile_id <= 0xad) or (0xb5 <= tile_id <= 0xb6) or (0xb8 <= tile_id <= 0xbb):
            candidates.append("blocking")
        print(f"  idx={i:2d}  b1=0x{b1:02x} b2=0x{b2:02x}  m=0x{m:02x}  candidates={candidates}")


if __name__ == "__main__":
    main()
