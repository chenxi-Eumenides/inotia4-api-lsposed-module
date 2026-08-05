#!/usr/bin/env python3
"""静态表字段语义分析：对表记录每个 u16 字段偏移做文本引用检测 + 值分布统计。

方法：
- text 命中：字段值作为 text_id 查 zh-Hans 文本表，命中率高 = 文本引用字段（名称/描述）
- 值分布：唯一值数、范围、0 占比 → 推断 ID/枚举/数值/位域
- 输出按「文本命中率」排序，供 field_catalog.json 扩展

用法：uv run python scripts/analyze/table_fields.py [表名...]（默认全部核心表）
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TABLES = ROOT / "static-data" / "json" / "tables"
TEXT_PATH = ROOT / "static-data" / "json" / "text" / "zh-Hans.json"

CORE_TABLES = [
    "ITEMDATABASE", "MONDATABASE", "SKILLDESCBASE", "QUESTINFOBASE",
    "QUESTREWARDBASE", "MAPINFOBASE", "CHARCLASSBASE", "MERCENARYINFOBASE",
    "NPCINFOBASE", "ITEMCLASSBASE", "ITEMGRADEBASE", "ITEMRARITYGRADEBASE",
    "SKILLTRAINBASE", "MONSKILLBASE", "MONSTERDROPBASE", "PORTALINFOBASE",
    "ITEMSTATICBASE", "ITEMDESCBASE", "ITEMENCHANTBASE", "ATTRINITBASE",
]


def load_texts() -> list:
    with open(TEXT_PATH, encoding="utf-8") as f:
        return json.load(f)


def analyze(table_name: str, texts: list) -> None:
    path = TABLES / f"{table_name}.json"
    if not path.exists():
        print(f"!! {table_name}: 表不存在")
        return
    data = json.loads(path.read_text(encoding="utf-8"))
    recs = data.get("records", [])
    if not recs:
        print(f"!! {table_name}: 空表")
        return
    size = data.get("record_size", len(recs[0]["u16"]) * 2)
    print(f"\n===== {table_name}（{len(recs)} 条，{size}B/条，{len(recs[0]['u16'])} u16 字段）=====")

    width = len(recs[0]["u16"])
    text_len = len(texts)
    for j in range(width):
        vals = [r["u16"][j] for r in recs]
        hits = [v for v in vals if 0 < v < text_len and texts[v]]
        hit_rate = len(hits) / len(vals)
        unique = len(set(vals))
        zero = sum(1 for v in vals if v == 0) / len(vals)
        vmin, vmax = min(vals), max(vals)
        top = Counter(vals).most_common(3)
        flags = []
        if hit_rate >= 0.5:
            flags.append(f"TEXT({hit_rate:.0%})")
        if unique <= 10:
            flags.append("ENUM")
        if zero >= 0.5:
            flags.append("SPARSE")
        if vmax > 0x8000:
            flags.append("BIG")
        if not flags:
            flags.append("NUM")
        # 文本示例（若有）
        sample_txt = ""
        if hits:
            sample_txt = f" 例:{texts[hits[0]][:12]!r}"
        print(f"  +0x{j*2:02x} {flags[0]:<12} 值[{vmin}..{vmax}] 唯一{unique} 0占比{zero:.0%} "
              f"top{[(v, c) for v, c in top[:2]]}{sample_txt}")


def main() -> None:
    names = sys.argv[1:] or CORE_TABLES
    texts = load_texts()
    print(f"文本表: {len(texts)} 条")
    for name in names:
        analyze(name, texts)


if __name__ == "__main__":
    main()
