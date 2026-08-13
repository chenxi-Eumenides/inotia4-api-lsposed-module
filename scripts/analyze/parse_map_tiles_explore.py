#!/usr/bin/env python3
"""瓦片矩阵离线解析研究脚本（探索性）

根据 MAP_LoadBase 反汇编（0x112060）逻辑：
- 11-bit tile ID 编码：(byte1 & 0x7) << 8 | byte2
- matrix 字节：(byte1 >> 4) | (0x40 if blocking)
- 阻挡 tile ID 列表从 0x11210c-0x11225c cmp 指令提取
- 64×64 matrix，索引 y*64+x

运行：uv run python scripts/analyze/parse_map_tiles_explore.py
"""
from __future__ import annotations
import sys
from pathlib import Path


def is_blocking_tile(tile_id: int) -> bool:
    if tile_id in (0xa1, 0xa8, 0xaf, 0xb2, 0x259, 0x264, 0x267, 0x273, 0x276, 0x6f6, 0x758):
        return True
    if 0x8d <= tile_id <= 0x8f:
        return True
    if tile_id == 0x95:
        return True
    if 0x99 <= tile_id <= 0x9e:
        return True
    if 0xaa <= tile_id <= 0xad:
        return True
    if 0xb5 <= tile_id <= 0xb6:
        return True
    if 0xb8 <= tile_id <= 0xbb:
        return True
    if tile_id == 0x23d:
        return True
    if 0x24a <= tile_id <= 0x24b:
        return True
    return False


def parse_map_file(data: bytes, map_id: int) -> dict:
    """按 MAP_Load 0x1149d4 反汇编解析。

    文件结构假设：
    - byte 0: 跳过（LZMA marker 检查后跳 1 字节）
    - byte 1-4: header（4 字节，byte3→width, byte4→height）
    - 后续: base layer (width*height*2) + exit + layer data
    """
    if len(data) < 5:
        return {"error": f"file too small ({len(data)} bytes)"}

    pos = 1
    _b1, _b2, width, height = data[pos], data[pos+1], data[pos+2], data[pos+3]
    pos += 4

    matrix = bytearray(64 * 64)
    blocking_count = 0
    cells_processed = 0

    for y in range(min(height, 64)):
        for x in range(min(width, 64)):
            if pos + 2 > len(data):
                break
            byte1 = data[pos]
            byte2 = data[pos + 1]
            pos += 2

            tile_id = ((byte1 & 0x7) << 8) | byte2
            blocking = is_blocking_tile(tile_id)
            matrix_byte = (byte1 >> 4) | (0x40 if blocking else 0)
            matrix[y * 64 + x] = matrix_byte

            cells_processed += 1
            if blocking:
                blocking_count += 1

    return {
        "mapId": map_id,
        "fileSize": len(data),
        "width": width,
        "height": height,
        "cellsProcessed": cells_processed,
        "blockingCount": blocking_count,
        "expectedTileData": width * height * 2,
        "actualTileData": cells_processed * 2,
        "remaining": len(data) - pos,
    }


def main() -> None:
    raw_dir = Path("apk/static-data/raw")
    if not raw_dir.exists():
        print(f"ERROR: {raw_dir} not found", file=sys.stderr)
        sys.exit(1)

    samples = [0, 1, 31, 100, 200, 300, 415]

    print(f"{'mapId':<8} {'fileSize':<10} {'width':<6} {'height':<6} {'cells':<8} {'blocking':<10} {'expected':<10} {'actual':<10} {'remaining':<10}")
    print("-" * 90)

    for map_id in samples:
        path = raw_dir / f"m{map_id}.dat.bin"
        if not path.exists():
            print(f"m{map_id}: NOT FOUND")
            continue
        data = path.read_bytes()
        result = parse_map_file(data, map_id)
        if "error" in result:
            print(f"m{map_id:<6}: {result['error']}")
            continue
        print(
            f"m{map_id:<6} {result['fileSize']:<10} {result['width']:<6} {result['height']:<6} "
            f"{result['cellsProcessed']:<8} {result['blockingCount']:<10} "
            f"{result['expectedTileData']:<10} {result['actualTileData']:<10} {result['remaining']:<10}"
        )

    print()
    print("观察：cellsProcessed < expectedTileData/2 表明 fileSize 不够装完整 base layer")


if __name__ == "__main__":
    main()
