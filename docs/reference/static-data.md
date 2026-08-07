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
| 文本表（6 语言） | `memorytext*.dat.jpg` → 各 ~650KB | `static-data/json/text/*.json`（各 35,811 条；en 为 35,812） |
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
    ├── text/                     # 6 语言文本（index → 字符串）
    ├── snasys/                   # SNASYS 条目切分（tile/mapfeature/worldmap）
    ├── reverse/                  # vendor 深度逆向成果
    │   ├── game_values_core.json # 核心数值表（怪物/状态骰子/Buff/ACT/任务奖励）
    │   ├── events.json           # 事件表（EVTINFO 索引 + eventdata 命令）
    │   ├── event_conditions.json # 事件条件
    │   ├── event_command_flags.json
    │   └── field_catalog.json    # 71 个已验证字段目录
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
- 数值字段语义：`game_values_core.json` 与 `field_catalog.json` 已验证 71 个（怪物/任务/Buff/事件/状态骰子），
  其余表字段语义待逆向（偏移定义方式：`*BASE_pData` + `record_index * nRecordSize`）

## 5. 工具链（scripts/parse/）

| 文件 | 作用 |
|---|---|
| `vendor/inotia_resources.py` | 基础库：容器解压、Excel 表拆分、文本解析、100 表名映射（MIT，来自 github.com/sunflower9264/reverse_inotia4） |
| `vendor/consolidate_texts.py` | 文本关系（NPC/怪物/选项/任务文本映射） |
| `vendor/export_reverse_datasets.py` | 深度逆向导出（事件/数值/字段目录） |
| `extract_all.py` | 批量解压 445 个 `.dat.jpg` → `static-data/raw/` |
| `export_tables.py` | 100 张表 → JSON + 文本联查 |
| `export_texts.py` | 6 语言文本 → JSON |
| `export_snasys.py` | SNASYS 条目切分（含内层二次解压） |

运行：`uv run python scripts/parse/<script>.py`（项目 .venv，Python 3.13，仅标准库）

## 6. 与运行时数据的关系

- **静态表**（本任务）：全部 100 张配置表，供 API 联查名称/属性/价格等（如物品 ID → 名称）
- **运行时数据**（M4 模块）：`INVEN_nMoney`、`PARTY_pChar` 等 native 全局，供 API 提供当前状态
- 两者在 API 层按 ID 关联：如背包物品 `itemId` → `ITEMDATABASE.json` 联查名称/稀有度

## 7. 字段语义逆向补充（无实机开发阶段）

> 日期：2026-08-05 ｜ 基于 `scripts/analyze/table_fields.py` TEXT 命中分析 + 跨表交叉验证

### 7.1 方法

对 20 张核心表运行 `table_fields.py`，对每个 u16 字段做三件事：
1. **TEXT 命中检测**：字段值作为 text_id 查 zh-Hans 文本表（35,811 条），命中率≥80% 视为疑似文本引用
2. **值分布统计**：唯一值数、值域、0 占比 → 推断枚举/ID 引用/数值/位域
3. **交叉验证**：对 TEXT 命中的字段，对照已知表关系（如 MONDATABASE 掉落物品 ↔ ITEMDATABASE 名称、MONDATABASE 地图索引 ↔ MAPINFOBASE 记录）确认真实语义

### 7.2 核心发现

#### MONDATABASE（怪物表，553 条）
| 偏移 | 宽度 | 新命名 | 置信度 | 交叉验证 |
|---|---|---|---|---|
| +0x02 | u16 | drop_item_text | high | 176/203 唯一值命中 ITEMDATABASE 名称 text_id，401/553 条记录命中 |
| +0x04 | u16 | spawn_zone_text | medium | TEXT 86%，含区域名（瑟林湖/肯金/封印地下城等），109 唯一值 |
| +0x08 | u16 | map_index | high | 126/159 唯一值在 0–415 范围内（MAPINFOBASE 索引），多怪共享一图 |
| +0x10 | u16 | monster_type_text | high | TEXT 100%，11 唯一值，5 个与 CHARCLASSBASE 技能文本重合 |

**关键交叉验证——MONDATABASE +0x08 ↔ MAPINFOBASE**：
- 126 个不同值落入 MAPINFOBASE 记录索引范围（0–415）
- 但 0 值出现 66 次（空/无地图），且大部分怪物集中在小范围索引值内
- 偏移 +0x08 在现有 catalog 中是 _未覆盖区域_（已有条目覆盖 +0x05~0x07、+0x0B、+0x0F~0x14、+0x22~0x24）

#### ITEMDATABASE（物品表，1018 条）
| 偏移 | 宽度 | 新命名 | 置信度 | 交叉验证 |
|---|---|---|---|---|
| +0x04 | u16 | item_subcategory_text | medium | TEXT 70%，101 唯一值。高值（>25000）在文本表外 → 空文本，低值（如 511="基础斗篷"）为子类别描述符 |
| +0x0a | u16 | item_effect_desc | medium | TEXT 90%，68 唯一值，短效果描述（如"+HP最大值/+防御力"等） |

#### QUESTINFOBASE（任务表，507 条）
| 偏移 | 宽度 | 新命名 | 置信度 | 交叉验证 |
|---|---|---|---|---|
| +0x0c | u16 | required_class_desc | medium | TEXT 65%，24 唯一值。值 0=任意职业（55 次），1–5 对应 5 种职业描述文本 |
| +0x18 | u16 | quest_info_text | medium | TEXT 69%，11 唯一值。含奖励提示文本（256="束腰布衫"/512="基础项链" 等物品名） |

#### MAPINFOBASE（地图表，416 条）
| 偏移 | 宽度 | 新命名 | 置信度 | 交叉验证 |
|---|---|---|---|---|
| +0x02 | u16 | map_subtitle_text | medium | TEXT 85%，50 唯一值 |
| +0x08 | u16 | map_flags_text | medium | TEXT 100%，仅 2 唯一值（255 占 415/416、0 占 1/416）。可能为城镇安全区标志 |

#### CHARCLASSBASE（职业表，6 条）—— 全字段语义确认
| 偏移 | 宽度 | 新命名 | 置信度 | 交叉验证 |
|---|---|---|---|---|
| +0x04 | u16 | class_display_name | high | TEXT 83%，6 唯一 = 6 职业名（影子猎人侦察兵/封印地下城等） |
| +0x0a | u16 | base_skill_text | high | TEXT 100%，6 唯一，各职业默认技能名 |
| +0x0c | u16 | starter_equip_text | high | TEXT 100%，6 唯一，初始装备名 |
| +0x10 | u16 | starter_item_text | high | TEXT 100%，6 唯一，初始道具描述 |

#### 其他快速扫描表

| 表 | 偏移 | 宽度 | 新命名 | 置信度 | 说明 |
|---|---|---|---|---|---|
| MERCENARYINFOBASE | +0x00 | u16 | mercenary_attr_text | high | 佣兵属性文本（TEXT 100%，41/47 唯一） |
| MERCENARYINFOBASE | +0x04 | u16 | mercenary_name | high | 佣兵名（TEXT 100%，47 唯一） |
| NPCINFOBASE | +0x02 | u16 | npc_role_text | medium | NPC 角色文本（TEXT 81%，17 唯一） |
| MONSTERDROPBASE | +0x00 | u16 | drop_item_text | high | 掉落物品名（TEXT 100%，35 唯一） |
| MONSTERDROPBASE | +0x02 | u16 | drop_desc_text | high | 掉落描述（TEXT 100%，109 唯一） |
| ITEMDESCBASE | +0x00 | u16 | item_desc_ref | high | 物品描述引用（TEXT 98%，152 唯一） |
| ITEMDESCBASE | +0x02 | u16 | item_name_text | high | 物品名文本（TEXT 100%，如"4格背包"等） |
| ITEMENCHANTBASE | +0x00 | u16 | enchant_attr_text | high | 附魔属性名（TEXT 100%，12 唯一） |
| ITEMENCHANTBASE | +0x06 | u16 | enchant_suffix_text | high | 附魔后缀（TEXT 100%，16 唯一） |

### 7.3 局限性

1. **仅 TEXT 字段可验证**：纯数值字段（价格、概率、等级参数等）无法通过文本表交叉验证，需实机调试或代码分析
2. **u16 盲区**：table_fields.py 仅分析 u16 字段；u8 字段（如现有 catalog 中的 monster_scaled_u8 系列）不会被覆盖
3. **TEXT 假阳性**：小数值（如 0–100）可能偶然命中文本表（如 map_index 字段被误标为 TEXT(87%），交叉验证可排除
4. **无实机验证**：所有语义判断基于静态数据分布逻辑推断，未经游戏内观测确认

### 7.4 产物

- `field_catalog.json`：从 48 条扩展到 **71 条**（新增 23 条，全部带交叉验证注释）
- 新增字段覆盖 11 张表：MONDATABASE(4)、ITEMDATABASE(2)、QUESTINFOBASE(2)、MAPINFOBASE(2)、CHARCLASSBASE(4)、MERCENARYINFOBASE(2)、NPCINFOBASE(1)、MONSTERDROPBASE(2)、ITEMDESCBASE(2)、ITEMENCHANTBASE(2)

## 8. 待办 / 后续可扩展

> 静态数据相关待办已统一收录至 `docs/backlog.md`（数据层表），本节不再维护。
