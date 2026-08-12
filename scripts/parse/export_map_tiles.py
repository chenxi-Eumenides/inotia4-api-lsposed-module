#!/usr/bin/env python3
"""离线解析 416 个 map 文件，输出通行矩阵 JSON。

按 MAP_LoadBase (0x112060) 反汇编逻辑：
- 11-bit tile ID 编码：(byte1 & 0x7) << 8 | byte2
- matrix 字节：(byte1 >> 4) | (0x40 if blocking)
- 阻挡 tile ID 列表从 0x11210c-0x11225c cmp 指令提取
- matrix 64x64 (MAP_IsBlocking y*64+x)
- 文件结构：byte 0 skip, byte 1-4 header (byte3=width, byte4=height)

输出：static-data/json/maps/tiles.json
格式：{"m0": {"width": 30, "height": 128, "tiles": "<base64 of 4096 bytes>"}, ...}

运行：uv run python scripts/parse/export_map_tiles.py
"""
from __future__ import annotations
import base64
import json
import sys
from pathlib import Path


BLOCKING_TILES_EXACT = frozenset({0xa1, 0xa8, 0xaf, 0xb2, 0x259, 0x264, 0x267, 0x273, 0x276, 0x6f6, 0x758})
BLOCKING_TILES_RANGES = (
    (0x8d, 0x8f), (0x95, 0x95), (0x99, 0x9e), (0xaa, 0xad),
    (0xb5, 0xb6), (0xb8, 0xbb), (0x23d, 0x23d), (0x24a, 0x24b),
)


def is_blocking_tile(tile_id: int) -> bool:
    if tile_id in BLOCKING_TILES_EXACT:
        return True
    for lo, hi in BLOCKING_TILES_RANGES:
        if lo <= tile_id <= hi:
            return True
    return False


def parse_map_tiles(data: bytes) -> tuple[int, int, bytearray, int]:
    """解析单个 map 文件，返回 (width, height, 64x64 matrix, blocking_count)。

    矩阵填充规则（按 MAP_LoadBase 0x11210c-0x11216c 反汇编）：
    - 索引 y*64+x (stride 64)
    - 每个 cell 读 2 字节，11-bit tile ID
    - matrix_byte = (byte1 >> 4) | (0x40 if blocking)
    - 文件数据不足时该 cell 保持 0
    """
    if len(data) < 5:
        return 0, 0, bytearray(64 * 64), 0

    width = data[3]
    height = data[4]
    pos = 5

    matrix = bytearray(64 * 64)
    blocking_count = 0

    for y in range(min(height, 64)):
        for x in range(min(width, 64)):
            if pos + 2 > len(data):
                return width, height, matrix, blocking_count
            byte1 = data[pos]
            byte2 = data[pos + 1]
            pos += 2

            tile_id = ((byte1 & 0x7) << 8) | byte2
            if is_blocking_tile(tile_id):
                matrix[y * 64 + x] = (byte1 >> 4) | 0x40
                blocking_count += 1
            else:
                matrix[y * 64 + x] = (byte1 >> 4)

    return width, height, matrix, blocking_count


def main() -> None:
    raw_dir = Path("static-data/raw")
    out_path = Path("static-data/json/maps/tiles.json")
    if not raw_dir.exists():
        print(f"ERROR: {raw_dir} not found", file=sys.stderr)
        sys.exit(1)

    out_path.parent.mkdir(parents=True, exist_ok=True)

    files = sorted(f for f in raw_dir.glob("m*.dat.bin") if f.stem[1:].split(".")[0].isdigit())
    print(f"Parsing {len(files)} map files...")

    result: dict[str, dict] = {}
    blocking_total = 0
    skip_count = 0

    for f in files:
        map_id = int(f.stem[1:].split(".")[0])
        data = f.read_bytes()
        width, height, matrix, blocking_count = parse_map_tiles(data)

        if width == 0 and height == 0:
            skip_count += 1

        blocking_total += blocking_count
        result[f"m{map_id}"] = {
            "mapId": map_id,
            "width": width,
            "height": height,
            "blockingCount": blocking_count,
            "tiles": base64.b64encode(bytes(matrix)).decode("ascii"),
        }

    out_path.write_text(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
    size_kb = out_path.stat().st_size / 1024
    print(f"OK: {len(result)} maps, {blocking_total} total blocking tiles")
    print(f"Output: {out_path} ({size_kb:.1f} KB)")


if __name__ == "__main__":
    main()
