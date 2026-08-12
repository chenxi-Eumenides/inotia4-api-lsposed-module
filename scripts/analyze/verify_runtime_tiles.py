#!/usr/bin/env python3
"""快速验证：连真机取当前地图 tiles，与离线解析结果对比。

运行：uv run python scripts/analyze/verify_runtime_tiles.py [IP]
"""
from __future__ import annotations
import base64
import json
import sys
import urllib.request
from pathlib import Path


def fetch(url: str, timeout: int = 5) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8"))


def main() -> None:
    ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.3.54"
    base = f"http://{ip}:8088"

    info = fetch(f"{base}/api/info/current-map")
    map_id = info["mapId"]
    print(f"Current map: m{map_id}")

    tiles_resp = fetch(f"{base}/api/info/current-map/tiles")
    runtime_b64 = tiles_resp["tiles"]
    runtime_bytes = bytearray(base64.b64decode(runtime_b64))
    print(f"Runtime matrix: {len(runtime_bytes)} bytes, "
          f"bit3_set={sum(1 for b in runtime_bytes if b & 0x08)}, "
          f"nonzero={sum(1 for b in runtime_bytes if b != 0)}")

    tiles_path = Path("static-data/json/maps/tiles.json")
    if not tiles_path.exists():
        print("ERROR: tiles.json not found, run export_map_tiles.py first")
        sys.exit(1)
    with open(tiles_path) as f:
        offline = json.load(f)

    key = f"m{map_id}"
    if key not in offline:
        print(f"ERROR: {key} not in offline parser output")
        sys.exit(1)

    off_entry = offline[key]
    off_bytes = bytearray(base64.b64decode(off_entry["tiles"]))
    print(f"Offline matrix: width={off_entry['width']} height={off_entry['height']} "
          f"blocking={off_entry['blockingCount']} bit3_set={sum(1 for b in off_bytes if b & 0x08)} "
          f"nonzero={sum(1 for b in off_bytes if b != 0)}")

    if off_bytes == runtime_bytes:
        print("✓ IDENTICAL: 4096 bytes fully match")
        return

    diffs = [(i, o, r) for i, (o, r) in enumerate(zip(off_bytes, runtime_bytes)) if o != r]
    print(f"\n✗ DIFFER: {len(diffs)} / 4096 bytes differ")
    if diffs:
        print("First 10 diffs (idx, offline, runtime, off_bits, run_bits):")
        for i, o, r in diffs[:10]:
            print(f"  [{i:4d}] off=0x{o:02x} run=0x{r:02x}  off_bits={o:08b} run_bits={r:08b}")
    b3_off = sum(1 for o, r in zip(off_bytes, runtime_bytes) if (o & 0x08) != (r & 0x08))
    b6_off = sum(1 for o, r in zip(off_bytes, runtime_bytes) if (o & 0x40) != (r & 0x40))
    print(f"\nbit3 diffs: {b3_off}")
    print(f"bit6 diffs: {b6_off}")
    print(f"other bit diffs: {len(diffs) - b3_off - b6_off}")


if __name__ == "__main__":
    main()
