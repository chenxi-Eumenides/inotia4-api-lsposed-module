#!/usr/bin/env python3
"""逐步解析 m31 文件：base layer → MAP_LoadLayer → exit count → exits。

验证 exit 数据定位与 runtime 的 9 个 bit7 cell 是否一致。
"""
from __future__ import annotations
import sys
from pathlib import Path


def main() -> None:
    data = (Path("static-data/raw") / "m31.dat.bin").read_bytes()
    print(f"file size: {len(data)}")

    w, h = data[2], data[3]
    print(f"width={w} height={h}")

    # 1. base layer
    pos = 4
    base_bytes = w * h * 2
    pos += base_bytes
    print(f"after base layer: pos={pos} (base={base_bytes} bytes)")

    # 2. MAP_LoadLayer: 4 u16 layer configs
    layer_cfgs = []
    for i in range(4):
        val = data[pos] | (data[pos + 1] << 8)
        layer_cfgs.append(val)
        pos += 2
    print(f"layer configs: {[hex(v) for v in layer_cfgs]}")

    # 3. section count
    section_count = data[pos]
    pos += 1
    print(f"section count: {section_count}")

    # 4. parse each section
    sections = []
    for s in range(section_count):
        header = data[pos]
        pos += 1
        lo4 = header & 0xf
        hi4 = header >> 4
        feature_count = data[pos] | (data[pos + 1] << 8)
        pos += 2
        features = []
        for f in range(feature_count):
            fx, fy, fhi, flo = data[pos], data[pos+1], data[pos+2], data[pos+3]
            features.append((fx, fy, fhi, flo))
            pos += 4
        sections.append((header, lo4, hi4, feature_count, features[:3]))
        if s < 3:
            print(f"  section {s}: hdr=0x{header:02x} lo4={lo4} hi4={hi4} count={feature_count} "
                  f"first_feats={features[:2]}")
        elif s == 3:
            print(f"  ... total {section_count} sections ...")
            print(f"  last section {s}: hdr=0x{header:02x} lo4={lo4} hi4={hi4} count={feature_count}")

    print(f"after layer data: pos={pos}")

    # 5. exit count
    exit_count = data[pos]
    pos += 1
    print(f"exit count: {exit_count}")

    # 6. exits (6 bytes each)
    in_bounds = []
    for i in range(exit_count):
        e = data[pos:pos + 6]
        x, y = e[0], e[1]
        pos += 6
        if x < 64 and y < 64:
            in_bounds.append((i, x, y))
    print(f"after exits: pos={pos}")
    print(f"exits in 64x64 bounds: {len(in_bounds)}")
    for i, x, y in in_bounds:
        print(f"  exit {i}: x={x} y={y} → index {x + y*64} (runtime check)")

    print(f"file remaining after exits: {len(data) - pos} bytes")


if __name__ == "__main__":
    main()
