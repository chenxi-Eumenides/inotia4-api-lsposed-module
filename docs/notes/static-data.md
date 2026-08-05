# M3 静态数据提取：game_res 格式逆向与交付说明

> 日期：2026-08-05 ｜ 状态：✅ 完成 ｜ 对应里程碑 M3「提取静态数据 → JSON 数据库」

## 1. 格式逆向结论

**`assets/common/game_res/*.dat.jpg` 不是加密文件，是 LZMA1 raw 压缩容器。** 观察到的
`01 00 5d 00 00 00 01` 固定前缀是容器头，非 AES 密文（此前符号表里的 `DecryptData`/`rijndaelDecrypt`
等 AES 原语用于 Hive 存档/网络层，game_res 数据文件不经过它们）。

### 容器字节布局（15 字节头 + LZMA1 流）

```
偏移   长度   含义
0      2      magic = 01 00
2      1      LZMA 属性字节（0x5d = lc3 lp0 pb2）
3      4      dict_size（LE，典型 0x01000000 = 16MB）
7      4      out_size（LE，解压后字节数）
11     4      保留（通常 0）
15     ...    LZMA1 raw 压缩流（FORMAT_RAW + FILTER_LZMA1，参数取自属性字节）
```

- 解压工具等价 Python：`lzma.LZMADecompressor(format=lzma.FORMAT_ALONE)` 从偏移 2 起，或
  `FORMAT_RAW + FILTER_LZMA1(lc/lp/pb, dict_size)` 从偏移 15 起
- 445 个 `.dat.jpg` 中 **441 个为标准容器**；4 个非容器（`i_imgfont`/`i_portrait`/`i_title`/`i_worldmap`）为位图/SNASYS 直存数据
- 纯图片文件（`*.png.jpg`）是真实 PNG，`font.fnt.jpg` 是位图字体，均无需解析

## 2. 内容分类

| 内容 | 文件 | 解析产物 |
|---|---|---|
| 数值表（100 张） | `game.dat.jpg` → 157,791B | `static-data/json/tables/*.json`（14,396 条记录） |
| 文本表（7 语言） | `memorytext*.dat.jpg` → 各 ~650KB | `static-data/json/text/*.json`（各 35,811 条） |
| 公式文本 | `memorytext_e.dat.jpg` | `static-data/json/text/formula-e.json`（1,991 条） |
| 瓦片集 | `i_tile.dat.jpg` | `static-data/json/snasys/i_tile.json`（1,983 条目） |
| 地图特征 | `i_mapfeature.dat.jpg` | `static-data/json/snasys/i_mapfeature.json`（213 条目） |
| 世界地图 | `i_worldmap.dat.jpg`（非容器） | `static-data/json/snasys/i_worldmap.json`（94 条目） |
| 事件 | `eventdata.dat.jpg` | `static-data/json/reverse/events.json`（608 事件 / 28,598 命令） |
| 地图文件 | `m0~m423.dat.jpg`（416 地图 + 8 文本） | `static-data/json/maps_summary.json`（424 项概要） |

## 3. 产物结构

```
static-data/
├── raw/                          # 解压中间产物（441 个 .bin，11MB）
│   ├── game.dat.bin              # 100 张 Excel 表源
│   ├── memorytext_zhhans.dat.bin # 简体中文文本源
│   ├── m0.dat.bin ...            # 地图瓦片矩阵
│   └── manifest.json             # 解压清单（原大小/解压大小/类型）
└── json/                         # 最终交付 JSON（22MB）
    ├── tables/                   # 100 张表，每表一个 JSON
    │   ├── _summary.json         # 表索引/记录数/宽度总览
    │   ├── CHARCLASSBASE.json    # 职业（6 条）
    │   ├── ITEMDATABASE.json     # 物品主表（1,018 条）
    │   ├── ...                   # 其余 97 张
    ├── text/                     # 7 语言文本（index → 字符串）
    ├── snasys/                   # SNASYS 条目切分（tile/mapfeature/worldmap）
    ├── reverse/                  # vendor 深度逆向成果
    │   ├── game_values_core.json # 核心数值表（怪物/状态骰子/Buff/ACT/任务奖励）
    │   ├── events.json           # 事件表（EVTINFO 索引 + eventdata 命令）
    │   ├── event_conditions.json # 事件条件
    │   ├── event_command_flags.json
    │   └── field_catalog.json    # 48 个已验证字段目录
    └── maps_summary.json         # 地图清单（尺寸/头部）
```

## 4. 表 JSON 格式

```json
{
  "table": "ITEMDATABASE",
  "index": 13,
  "record_count": 1018,
  "record_size": 23,
  "records": [
    {
      "hex": "0100...",           // 原始记录字节（hex）
      "u16": [1, 25, ...],        // 每 2 字节的 u16 LE 解读
      "text_0": "治疗药水"         // 已验证文本字段（text_<偏移>，联查 zh-Hans）
    }
  ]
}
```

- 文本字段 `text_<offset>`：该偏移的 u16 是 memorytext 索引，已联查简体中文
- 已验证文本偏移（命中率 100%）：ITEMDATABASE 名称+0、MONDATABASE 名称+0、NPCINFOBASE 名称+0、
  QUESTINFOBASE 标题+2 / 详情+14 / 进度+16 / 完成+18、SKILLDESCBASE +2、MERCENARYINFOBASE +2、
  MAPINFOBASE 地图名+0（416/416）、CHARCLASSBASE 描述+2、ITEMDESCBASE +2、CHOICEBASE 提示+0
- 数值字段语义：`game_values_core.json` 与 `field_catalog.json` 已验证 48 个（怪物/任务/Buff/事件/状态骰子），
  其余表字段语义待逆向（偏移定义方式：`*BASE_pData` + `record_index * nRecordSize`）

## 5. 工具链（scripts/parse/）

| 文件 | 作用 |
|---|---|
| `vendor/inotia_resources.py` | 基础库：容器解压、Excel 表拆分、文本解析、100 表名映射（MIT，来自 github.com/sunflower9264/reverse_inotia4） |
| `vendor/consolidate_texts.py` | 文本关系（NPC/怪物/选项/任务文本映射） |
| `vendor/export_reverse_datasets.py` | 深度逆向导出（事件/数值/字段目录） |
| `extract_all.py` | 批量解压 445 个 `.dat.jpg` → `static-data/raw/` |
| `export_tables.py` | 100 张表 → JSON + 文本联查 |
| `export_texts.py` | 7 语言文本 → JSON |
| `export_snasys.py` | SNASYS 条目切分（含内层二次解压） |

运行：`uv run python scripts/parse/<script>.py`（项目 .venv，Python 3.13，仅标准库）

## 6. 与运行时数据的关系

- **静态表**（本任务）：全部 100 张配置表，供 API 联查名称/属性/价格等（如物品 ID → 名称）
- **运行时数据**（M4 模块）：`INVEN_nMoney`、`PARTY_pChar` 等 native 全局，供 API 提供当前状态
- 两者在 API 层按 ID 关联：如背包物品 `itemId` → `ITEMDATABASE.json` 联查名称/稀有度

## 7. 待办 / 后续可扩展

- [ ] 100 张表中数值字段的完整语义逆向（当前 48 个已验证）
- [ ] 地图瓦片矩阵的渲染/通行矩阵解码（静态 m*.dat，与运行时 `MAP_nBaseTile` 互补）
- [ ] 内购表（CASHITEM/CHARGEDITEM）语义确认（盗版版已断网，数据仍在）
- [ ] SNASYS 条目按类型结构化（tile 精灵属性等）
