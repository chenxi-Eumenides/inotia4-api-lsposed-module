# 游戏指南与 API 操作手册（Inotia 4 远程操控）

> **本文档 = 一站式指南**：既介绍《艾诺迪亚4》的游戏信息与系统，也完整说明如何通过 HTTP API 远程操控游戏（端点/请求/参数/返回格式/用途）。
> 面向读者：人类玩家 + AI 代理。**只看本文档即可上手通过 API 玩游戏。**
>
> 适用版本：**模块 v0.5.13**（2026-08-14 构建 `output/inotia4-export-module-v0.5.13.apk`）。
> 路由与 `docs/api-reference.md` 完全对齐（v0.5.13 已全量修正 controller 路径）；实现状态以本文档标注为准（部分端点占位）。
> 技术细节见 `docs/research/data-sources.md`；函数签名见 `docs/research/control-capability.md`；逆向笔记见 `docs/research/systems/`。

---

## 目录

1. [游戏介绍](#1-游戏介绍)
2. [部署与环境](#2-部署与环境)
3. [通用约定](#3-通用约定)
4. [快速上手](#4-快速上手5-分钟从零开始)
5. [数据模型](#5-数据模型返回-json-结构)
6. [端点全表](#6-端点全表按域分组)
7. [玩法指南](#7-玩法指南用-api-完成游戏内目标)
8. [越权操作（OP）](#8-越权操作op-端点)
9. [静态数据表](#9-静态数据表查询)
10. [事件流](#10-事件流感知游戏变化)
11. [AI 代理决策建议](#11-给-ai-代理的决策建议)
12. [常见问题](#12-常见问题与注意事项)
13. [附录：参考表](#13-附录参考表)

---

## 1. 游戏介绍

### 1.1 基本信息

| 项 | 内容 |
|---|---|
| 游戏 | 《艾诺迪亚4》（Inotia 4），Com2uS，Android 单机 ARPG |
| 对接版本 | 盗版大修 20260704（离线、无网络验证） |
| 玩法类型 | 暗黑破坏神风格：点击移动 + 即时战斗 + 技能释放 |
| 队伍 | 1 名主控角色 + 2 名佣兵 = **3 人出战**，可随时切换主控 |
| 地图 | 64×64 瓦片矩阵，出口区域自动切图 |
| 等级上限 | 105 级 |
| 存档 | 3 个存档槽（save0.dat/save1.dat/save2.dat） |

### 1.2 六大职业

| class_idx | 职业 | 特点 |
|---|---|---|
| 0 | 黑暗骑士 | 物理坦克 |
| 1 | 忍者 | 高暴击物理输出 |
| 2 | 黑魔导 | 魔法输出 |
| 3 | 祭司 | 治疗辅助 |
| 4 | 暗影猎手 | 远程物理 |
| 5 | 狂战士 | 近战爆发 |

> 无转职；每职业约 15 个技能（全游戏 90+）。

### 1.3 核心系统（19 系统）与对应 API

| 系统 | 说明 | API 域 |
|---|---|---|
| 角色养成 | 等级/经验/属性点/技能点 | character.grow |
| 队伍/佣兵 | 3 人出战、佣兵入队/离队/遣散 | character.party / mercenary |
| 战斗 | 普攻/技能/自动反击/AI 用技能 | character.combat |
| 装备 | 10 槽位、强化/镶嵌/附魔、稀有度 5 档 | item.inventory |
| 背包/物品 | 6 袋×16 槽、使用/丢弃/出售/移动 | item.inventory |
| 货币 | 金币（游戏内金/银/铜，API 只读金） | item.inventory.money |
| 商店 | NPC 商店买卖（价格由静态表决定） | item.shop |
| 任务 | 主线/支线 NPC 任务 | quest |
| 地图/移动 | 瓦片/出口/寻路/切图 | world.map / movement |
| 对话/剧情 | NPC 对话树、剧情 AVG、弹窗 | ui.dialog |
| 存档 | 3 槽、保存/进出档/导出备份 | system.save |
| 合成/炼金 | 配方合成（API 未开放） | op.craft（占位） |
| 内购/每日奖励 | 盗版版已阻断（模块 hook 屏蔽支付弹窗） | — |
| 附魔 | 大修版新增：镶满 4 宝石触发神秘附魔 | item.inventory.enchant |

### 1.4 盗版大修 20260704 关键修改（相对原版）

- 经验获取 **-30%**
- 强化上限 12 阶、失败不扣强化次数；混沌/深渊合成 **100% 成功**
- 掉率提升（基础 5 倍；Lv60+ 装备紫装概率提升）
- **新增附魔系统**（原版无）：镶嵌 4 宝石触发附魔，25% 概率完美附魔
- 陨子（属性骰子）投掷：初始属性最大，升级随机加点
- 背包"粉碎"改"售卖"（售价原 70%）
- 内购/支付已断（模块 hook `SelectTarget.iapSelectTarget` 阻断支付弹窗）

> ⚠️ 数值/机制与原版 wiki 资料存在偏差，以游戏实际为准。

---

## 2. 部署与环境

### 2.1 前置条件

| 项 | 要求 |
|---|---|
| 设备 | Android 11+ root 手机（Zygisk-LSPosed），或 LSPatch 集成版 |
| 模块 | `output/inotia4-export-module-v0.5.13.apk` |
| 游戏 | 艾诺迪亚4 盗版大修 20260704，包名 `com.com2us.inotia4.normal.freefull.google.global.android.common` |
| 网络 | 手机与调用方同一局域网 |

### 2.2 启动服务

1. LSPosed 管理器启用模块，勾选游戏作用域
2. 重启/强停游戏进程 —— **HTTP 服务随游戏进程启动**（游戏退出则服务关闭，无独立守护）
3. 服务监听 `0.0.0.0:8088`（默认；可在模块 APK 的 `config.json` 配置监听地址/端口）

### 2.3 服务地址与健康检查

```
http://<手机局域网IP>:8088
```

```bash
curl http://<手机IP>:8088/api/health/
# {"ok":true}
```

> ⚠️ **无鉴权**：任何局域网客户端可访问全部端点（含 OP 越权端点）。仅限可信局域网。
> ⚠️ **明文 HTTP**，勿在公共网络使用。

---

## 3. 通用约定

### 3.1 请求/响应

- 请求、响应均为 **JSON**（POST body 为 JSON 字符串，建议带 `Content-Type: application/json`）
- **GET** = 读数据；**POST** = 执行操作
- 操作成功：`{"ok":true,"state":<快照>}`（`state` 为操作后最新状态，类型见各端点）
- 操作失败：`{"ok":false,"error":"<原因>"}`
- native 未就绪：所有端点返回 `{"error":"not ready"}`（模块初始化中，稍后重试）
- 不在游戏内：多数读端点返回 `{"error":"not in game"}`

### 3.2 索引约定

| 概念 | 范围 | 说明 |
|---|---|---|
| `role` | 0..2 | 出战槽位 |
| `bag` | 0..5 | 背包袋 |
| `slot` | 0..15 | 袋内槽位 |
| `equip_slot` | 0..9 | 装备槽（见 5.2 位置映射） |
| `class_idx` | 0..5 | 职业（见 1.2） |
| `direction` | 0..3 | 0=下 1=左 2=上 3=右 |
| `mercenary_slot` | — | 佣兵槽 ID（见 6.2 两套索引说明） |

### 3.3 操作前置检查（重要！）

- **弹窗阻塞**：`GET /api/ui/dialog` 返回 `active=true` 时游戏处于阻塞弹窗态，大部分写操作被 UI 阻塞。操作前先检查。
- **屏幕状态**：`GET /api/ui/screen` 应为 `"world"` 才能执行战斗/移动/背包操作；`"main_menu"` 只能做存档操作；`"story"` 表示剧情对话中（先 skip）。

---

## 4. 快速上手：5 分钟从零开始

> 把 `<手机IP>` 换成实际地址。全部命令可直接复制。

### 步骤 0：确认服务在线

```bash
curl http://<手机IP>:8088/api/health/
```

### 步骤 1：查看游戏信息与存档槽

```bash
curl http://<手机IP>:8088/api/system/game/info
```

```json
{
  "version": "0.5.13",
  "game": "main_menu",
  "save_slots": [{ "slot": 0, "exists": false }],
  "current_save_slot": -1,
  "package_name": "com.com2us.inotia4...",
  "base": 532410707968
}
```

### 步骤 2：创建新角色 或 进入已有存档

**新建**（槽 1，职业 2=黑魔导）：

```bash
curl -X POST http://<手机IP>:8088/api/system/create_slot \
  -H "Content-Type: application/json" \
  -d '{"slot":1,"class_idx":2}'
```

**进入已有存档**（先确认 `save_slots` 中该槽 `exists=true`，否则会崩溃）：

```bash
curl -X POST http://<手机IP>:8088/api/system/enter_slot -d '{"slot":0}'
```

### 步骤 3：看全局状态

```bash
curl http://<手机IP>:8088/api/system/snapshot
# {"frame":N,"screen":"world","money":N,"map_id":N,"x":N,"y":N,
#  "main_mercenary_slot":0,"party_count":2,"party":[{type,level,hp,mp,max_hp,max_mp,main_stats,name}]}
```

### 步骤 4：移动

```bash
# 寻路移动到像素坐标（正常走路，非瞬移）
curl -X POST http://<手机IP>:8088/api/world/movement/move_to -d '{"x":320,"y":480}'

# 或方向键持续移动后停止
curl -X POST http://<手机IP>:8088/api/world/movement/walk_dir -d '{"direction":3}'
sleep 1
curl -X POST http://<手机IP>:8088/api/world/movement/stop_move
```

### 步骤 5：找敌人并攻击

```bash
# 查看周围单位（status=2 为敌人）
curl http://<手机IP>:8088/api/world/map/units
# {"units":[{"slot":12,"x":480,"y":400,"type":0,"status":2,"level":3,"hp":120,...}]}

# 攻击（target_slot 用上面的 slot）
curl -X POST http://<手机IP>:8088/api/character/combat/0/attack_target -d '{"target_slot":12}'

# 停止战斗
curl -X POST http://<手机IP>:8088/api/character/combat/0/stop_combat
```

### 步骤 6：使用物品回血

```bash
curl http://<手机IP>:8088/api/item/inventory/
# 找到药水位置后：
curl -X POST http://<手机IP>:8088/api/item/inventory/use_item -d '{"bag":0,"slot":3}'
```

### 步骤 7：存档 + 备份

```bash
curl -X POST http://<手机IP>:8088/api/system/save
curl "http://<手机IP>:8088/api/system/export_save_file?slot=1"
# {"ok":true,"slot":1,"name":"save1.dat","content":"<base64>"}  → base64 解码即存档文件
```

---

## 5. 数据模型（返回 JSON 结构）

### 5.1 Player（玩家全局）

`GET /api/item/inventory/money` → `{"money":72503}`

| 字段 | 说明 |
|---|---|
| `money` | 金币 |
| `map_id` | 当前地图 ID（MAPINFOBASE 记录下标 0-415） |
| `x`/`y` | 玩家坐标（像素，tile×16） |
| `active_quest` | 当前追踪任务 ID |
| `main_mercenary_slot` | 主控角色对应出战槽 |
| `party_count` | 出战人数 |

### 5.2 Role（出战角色）

`GET /api/character/party` 返回 3 槽数组（空槽 `null`）。每项：

```json
{
  "type": 1, "name_id": 2210, "level": 27,
  "hp": 10598, "mp": 200, "max_hp": 10664, "max_mp": 250,
  "exp": 12000, "exp_next": 15000,
  "stats": { "0": 60, "4": 300, "30": 10664, "31": 212 },
  "main_stats": [96, 139, 101, 54, 38],
  "status_point": 78,
  "equipment": [ { "...": "装备" }, null, ... ],
  "name": "凯恩"
}
```

| 字段 | 说明 |
|---|---|
| `type` | 职业索引 0-5 |
| `stats` | 战斗属性数组（键为字符串索引 0-31，关键索引见附录 13.2） |
| `main_stats` | 主属性数组 [力量, 敏捷, 体力, 智力, 精力] |
| `equipment` | 10 装备槽（含 `slot`/`position`/`name`/属性），空槽 `null` |

**装备位置映射（equipment 数组下标）：**

| slot | position | 位置 |
|---|---|---|
| 0 | head | 头部 |
| 1 | glove | 护手 |
| 2 | cloak | 斗篷 |
| 3 | armor | 铠甲 |
| 4 | boots | 鞋 |
| 5 | weapon1 | 主手武器 |
| 6 | weapon2 | 副手（盾牌/双持） |
| 7 | necklace | 项链 |
| 8 | ring | 戒指 |
| 9 | unused | 未使用 |

### 5.3 Item（物品）

```json
{
  "slot": 3, "type_flags": 512, "raw_rarity": 0, "category": 1,
  "count": 5, "equip": false, "rarity": 0,
  "damage": 0, "defense": 0, "magic_rate": 0,
  "socket": 0, "socket_filled": 0, "socket_total": 0,
  "enchant": 0, "chaos": false, "enchant_id": 0, "enchant_level": 0,
  "chaos_level": 0, "chaos_rate": 100,
  "options": [], "option_ids": [],
  "name": "治疗药水", "rarity_tier": "白",
  "option_names": [], "options_detailed": [],
  "static_options": [], "socket_info": {}, "enchant_info": {}, "chaos_info": {}
}
```

| 字段 | 说明 |
|---|---|
| `category` | 物品 ID（ITEMDATABASE 索引；`name` 由模块联查注入） |
| `count` | 数量（装备类恒 1） |
| `equip` | 是否装备类 |
| `rarity`/`raw_rarity`/`rarity_tier` | 稀有度档位 0-4 / 原始位 0-15 / 档位名（白绿蓝黄紫） |
| `options`/`option_ids` | 词缀值 / 词缀索引数组（一一对应） |
| `option_names`/`options_detailed` | 词缀名 / 词缀明细 |
| `socket`/`socket_filled`/`socket_total` | 宝石孔原始值 / 已镶 / 总孔 |
| `enchant`/`enchant_id`/`enchant_level`/`chaos` | 附魔原始值 / ID / 等级 / 混沌标志 |
| `chaos_level`/`chaos_rate` | 混沌等级 / 混沌率（100=无混沌） |

### 5.4 Skills（技能）

`GET /api/character/party/{slot}/skills`：

```json
{
  "role": 0,
  "skills": [ { "action_id": 3, "level": 1, "max_level": 4, "skill_name": "血之复仇" } ],
  "unlock_bitmap": 65535,
  "active_skill_id": 3,
  "skill_points": 6
}
```

> `max_level` 常规最高 4 级，技能书可提升至 8 级。

### 5.5 Mercenaries（佣兵）

`GET /api/character/mercenary`：

```json
[
  { "slot": 0, "type": 0, "flags": 219, "in_party": true, "name": "凯恩", "level": 27, "x": 320, "y": 480 }
]
```

- `flags` bit0=占用、bit1=在队伍；未上场佣兵坐标 2048 = 未激活哨兵

### 5.6 Map（当前地图）

`GET /api/world/map`：

```json
{
  "map_id": 30, "x": 120, "y": 312,
  "tile": { "tx": 7, "ty": 19, "blocking": false },
  "exits": [ { "tx": 24, "ty": 19, "px": 384, "py": 304 } ],
  "map_data": { "text_id": 3513, "name": "影子丛林1" },
  "units": [ ... ], "enemies": [ ... ], "interactives": [ ... ], "drops": []
}
```

**Unit（场景单位）字段**：`slot/x/y/type/status/level/hp/mp/name/distance/nearest_distance`
- `status`：2=敌人/召唤物，1=城镇 NPC/佣兵，0=队伍
- `distance`：玩家 BFS 可达距离（-1=不可达，此时看 `nearest_distance`）

### 5.7 GameState（界面状态）

`GET /api/ui`：

```json
{
  "screen": "world",
  "frame": 12345,
  "dialog_active": false,
  "dialog": { "text": "...", "has_ok": false, "has_cancel": false, "buttons": [] }
}
```

`screen` 取值：`loading` / `main_menu` / `world` / `story`（剧情 AVG）/ `dialog`（弹窗）/ `tutorial_pause`（药水教学暂停）/ 面板名（inventory/skills/mercenary/quests/settings/character_info/shop/craft/save_slot/wipeout/...）

### 5.8 Snapshot（全量快照）

`GET /api/system/snapshot`（v0.5.13 精简版：**不含装备明细与佣兵列表**）：

```json
{
  "frame": 12345, "screen": "world",
  "money": 72503, "map_id": 30, "x": 304, "y": 376,
  "main_mercenary_slot": 0, "party_count": 2,
  "party": [
    { "type": 1, "level": 27, "hp": 10598, "mp": 200, "max_hp": 10664,
      "max_mp": 250, "main_stats": [96,139,101,54,38], "name": "凯恩" }
  ]
}
```

> 装备明细走 `GET /api/character/party/{slot}/equipment`；佣兵走 `GET /api/character/mercenary`。

### 5.9 DialogContent（对话/弹窗内容）

`GET /api/ui/dialog` 统一检测返回：

```json
{
  "type": "npc", "active": true,
  "speaker": "商人", "text": "欢迎光临！",
  "options": [ { "id": "0", "label": "购买物品" }, { "id": "next", "label": "下一句" } ]
}
```

| type | 场景 | 可选动作（select 的 action） |
|---|---|---|
| `story` | 剧情对话 | `next` 下一句 / `skip` 跳过 |
| `npc` | NPC 对话 | `next` 下一句 / `index` 选项选择（`{"index":n}`） |
| `popup` | 确认弹窗 | `ok` / `cancel` |
| `npc_quest` | 任务完成面板 | `complete` / `close` |
| `wipeout` | 死亡面板 | `revive` / `special_revive` / `game_over` |
| `save_slot` | 存档槽面板 | `save` / `close` |
| `save`/`sell`/`quest` | 各类弹窗 | `confirm` / `quit` / `cancel` |
| `none` | 无对话 | 空 options |

---

## 6. 端点全表（按域分组）

> 全部端点以 controller 代码实际路由为准（v0.5.13 已与 api-reference.md 对齐）。**共 100+ 端点**，已实现见下，占位端点标注 ⏳。

### 6.1 system（系统与会话）

#### 服务与游戏整体

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/health/` | 服务存活检查 | `{"ok":true}` |
| GET | `/api/system/game/` | 游戏复合（快照+信息） | `{"snapshot":...,"info":...}` |
| GET | `/api/system/snapshot` | 全量快照（精简版） | `<Snapshot>` |
| GET | `/api/system/game/info` | 模块/软件信息 | `{"version","game","save_slots","current_save_slot","package_name","base"}` |
| GET | `/api/system/game/frame` | 帧计数 | `{"frame":12345}` |

#### 存档

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/system/save` | 手动存档 | 无 body | `{"ok":true,"state":<Player>}` |
| POST | `/api/system/enter_slot` | 进入存档槽（读档） | `{"slot":0}` (0-2) | `{"ok":true,"state":<Player>}` |
| POST | `/api/system/create_slot` | 创建新角色 | `{"slot":1,"class_idx":2}` | `{"ok":true,"state":<Player>}` |
| GET | `/api/system/export_save_file?slot=1` | 导出存档文件 | query `slot` | `{"ok":true,"name":"save1.dat","content":"<base64>"}` |

**⚠️ enter_slot 注意事项**：
- 只能在非 world 状态调用（world 中 → `already in game`）
- **存档不存在时调用会崩溃**——先查 `game/info` 的 `save_slots` 确认 `exists=true`

#### 事件流

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/system/events/?since=` | 轮询游戏事件（差异检测） | `{"events":[...]}`（见第 10 章） |

#### 静态数据表

| 方法 | 路径 | 用途 | 状态 |
|---|---|---|---|
| GET | `/api/system/tables` | 表清单 | ✅ |
| GET | `/api/system/tables/{table}` | 单表全量（表名自动大写） | ✅ |
| GET | `/api/system/tables/{table}/search?q=` | 表内名称搜索（q 必填） | ✅ |
| GET | `/api/system/tables/{table}/download` | 下载静态表文件 | ⏳ `{"ok":false,"error":"not implemented"}` |
| GET | `/api/system/tables/story-events` | 剧情事件数据 | ✅ |
| GET | `/api/system/tables/text?lang=` | 多语言文本（zh-Hans/en） | ✅ |
| GET | `/api/system/help` | 模块帮助文档 | ⏳ `{"ok":false,"error":"not implemented"}` |
| GET | `/api/system/download` | 下载帮助文档 | ⏳ `{"ok":false,"error":"not implemented"}` |

### 6.2 character（角色与队伍）

#### 出战队伍读

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/character/party` | 3 槽出战角色完整状态 | `[<Role>,...]` |
| GET | `/api/character/party/count` | 出战人数 | `{"count":2}` |
| GET | `/api/character/leader` | 当前主控角色 | `<Role>` 或 `{"error":"not found"}` |
| GET | `/api/character/party/{slot}` | 指定出战槽完整状态 | `<Role>` |
| GET | `/api/character/party/{slot}/id` | 职业索引 | `{"id":1,"id_name":"忍者"}` |
| GET | `/api/character/party/{slot}/name` | 角色名 | `{"name":"凯恩"}` |
| GET | `/api/character/party/{slot}/level` | 等级 | `{"level":27}` |
| GET | `/api/character/party/{slot}/status` | 状态聚合 | `{"status":{hp,max_hp,mp,max_mp,exp,exp_next,attribute_points,skill_points}}` |
| GET | `/api/character/party/{slot}/stats` | 全部战斗属性 | `{"stats":{...}}` |
| GET | `/api/character/party/{slot}/equipment` | 全部装备（含名称） | `{"equipment":[...]}` |
| GET | `/api/character/party/{slot}/equipment/{equip_slot}` | 单装备槽 | `<Item>` 或 `{"error":"not found"}` |
| GET | `/api/character/party/{slot}/skills` | 技能（含名称） | `<Skills>` |

#### 佣兵读

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/character/mercenary` | 全部佣兵（含未上场） | `[<Mercenary>...]` |
| GET | `/api/character/mercenary/list` | 非空佣兵槽 id 列表 | `{"slots":[0,1,3]}` |
| GET | `/api/character/mercenary/{slot}` | 指定佣兵槽 | `<Mercenary>` 或 `{"error":"not found"}` |

#### ⚠️ 两套佣兵索引

| 概念 | 值示例 | 来源 |
|---|---|---|
| 读端点 `slot`（槽数组下标） | 27/32/58 | `MERCENARYSYSTEM_pSlotList` 下标 |
| 写参数 `mercenary_slot`（角色槽 ID） | 0/1/255 | 角色对象 +0x352 字段 |

写端点（include/exclude/discharge/withdraw）的 `mercenary_slot` 用**角色槽 ID**（多数角色有效值 0；其余 255 无效）。两套编号不一致是已知设计问题。

#### 队伍操作（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/character/party/include` | 佣兵入队 | `{"mercenary_slot":0}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/character/party/exclude` | 佣兵离队 | `{"mercenary_slot":0}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/character/party/discharge` | 遣散佣兵 | `{"mercenary_slot":27}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/character/party/withdraw` | 取出佣兵装备 | `{"mercenary_slot":0,"equip_slot":3}` | `{"ok":true,"state":<Party>}` |

**常见错误**：已在队 → `already in party`；满员 → `party full`；主控 → `cannot exclude leader`；任务 NPC → `cannot exclude quest npc`；无角色 → `mercenary not found`。

#### 角色成长（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/character/grow/add_skill` | 主角学习/升级技能 | `{"action_id":3,"level":1}` | `{"ok":true,"state":<Skills>}` |
| POST | `/api/character/grow/{role}/add_stat` | 批量分配属性点 | `{"attrs":{"strength":1,"agility":2}}` | `{"ok":true,"applied":[...]}` |
| POST | `/api/character/grow/reset_stat` | 重置主角分配属性 | 无 body | `{"ok":true,"state":<Player>}` |
| POST | `/api/character/grow/reset_skill` | 重置主角技能 | 无 body | `{"ok":true,"state":<Player>}` |

**add_stat 属性名**：`strength`(0=力量)/`agility`(1=敏捷)/`vitality`(2=体力)/`intelligence`(3=智力)/`spirit`(4=精力)，也可用索引 `"0"`-`"4"`，值须为正整数。
**注意**：各属性分配数量总和不能超过剩余能力点 → `no status point`；属性名非法 → `bad attr`；值非法 → `bad value`。

#### 战斗（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/character/combat/{role}/set_auto_attack` | 开关自动反击（受击后自动还击） | `{"on":true}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/character/combat/{role}/set_skill_usage` | 设置技能 AI 使用档位 | ⏳ **stub**：`{"ok":false,"error":"not implemented"}` | — |
| POST | `/api/character/combat/switch_player` | 切换主控角色 | `{"slot":1}` (0-2) | `{"ok":true,"state":<Player>}` |
| POST | `/api/character/combat/{role}/cast_skill` | 立即释放技能 | `{"action_id":5}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/character/combat/{role}/attack_target` | 进入战斗并攻击目标 | `{"target_slot":5}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/character/combat/{role}/stop_combat` | 停止战斗 | 无 body | `{"ok":true,"state":<Player>}` |

> ⚠️ `set_auto_attack` 只控制"受击后自动还击"，**不是**自动索敌挂机——需先 `attack_target` 进入战斗。
> `cast_skill` 的 `action_id` 必须是已学**技能**（非普攻）；未学 → `skill not learned`；无目标 → `no target`。
> `attack_target`：目标无效 → `target not found`；持续普攻该目标直至死亡/离开/stop_combat。

### 6.3 world（地图与移动）

#### 当前地图读（GET）

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/world/map` | 地图复合 | `<Map 模型>` |
| GET | `/api/world/map/id` | 地图 ID + 名称 | `{"map_id":30,"id_name":"影子丛林1"}` |
| GET | `/api/world/map/exits` | 出口区域 | `{"exits":[...]}` |
| GET | `/api/world/map/units` | 全部场景单位 | `{"units":[...],"char_loc":[...]}` |
| GET | `/api/world/map/enemies` | 敌人过滤视图 | `{"units":[...]}` |
| GET | `/api/world/map/interactives` | NPC/佣兵过滤视图 | `{"units":[...]}` |
| GET | `/api/world/map/drops` | 掉落物（占位恒空） | `{"drops":[]}` |
| GET | `/api/world/map/distance?tx=&ty=` | BFS 距离计算（不移动） | `{"target","start","found","distance","nearest"}` |

> ⚠️ v0.5.13 已移除运行时瓦片端点（`/api/world/map/tile`、`/api/world/map/tiles`）——瓦片只从静态数据获取（见下方 maps）。

#### 移动操作（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/world/movement/move_to` | 寻路移动到像素坐标 | `{"x":600,"y":400}` | `{"ok":true,"state":<Player>}` |
| POST | `/api/world/movement/walk_dir` | 方向键持续移动 | `{"direction":1}` (0-3) | `{"ok":true,"state":<Player>}` |
| POST | `/api/world/movement/stop_move` | 停止所有移动 | 无 body | `{"ok":true,"state":<Player>}` |
| POST | `/api/world/movement/interact_with` | 场景交互/攻击键（触发事件链） | 无 body | `{"ok":true,"state":<Player>}` |

**移动语义**：
- `move_to`/`walk_dir` 都是**正常走路**（后台线程逐帧，非瞬移），到达出口区域自动切图
- 目标不可达 → 走到最近可达点并面向目标
- 剧情/切图状态（screen=story/mapchange）时操作自动终止

#### 静态地图（GET）

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/world/maps/list` | 地图列表 | `{"maps":[{"map_id":3513,"name":"..."}]}` |
| GET | `/api/world/maps/{map_id}` | 指定地图静态信息 | `{"map_id","text_id","name","raw"}` |
| GET | `/api/world/maps/{map_id}/tiles` | 指定地图瓦片矩阵 | `{"map_id","src":"static","size":64,"encoding":"array","tiles":"<64×64 双层数组，bit6=阻挡 bit7=出口>"}` |

> ⚠️ 两套地图编号：静态表 `maps/list` 的 `map_id` 是 **text_id**（3513-3928）；运行时 `/api/world/map/id` 返回 **MAPINFOBASE 记录下标**（0-415）。`maps/{map_id}` 对 0-415 按下标查询，其余按 text_id 兼容。

### 6.4 item（物品与背包）

#### 背包读（GET）

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/item/inventory` | 背包复合（6 袋×16 槽） | `<Inventory>` |
| GET | `/api/item/inventory/money` | 金币 | `{"money":72503}` |
| GET | `/api/item/inventory/items` | 全部物品展平 | `{"items":[...]}` |
| GET | `/api/item/inventory/bag/{bag}/info` | 袋信息 | `{"bag":0,"capacity":16,"slot_count":13}` |
| GET | `/api/item/inventory/bag/{bag}/{slot}` | 袋内指定槽 | `<Item>` 或 `null` |

> ⚠️ `bag/{bag}/{slot}` 按**实际格号**（物品的 `slot` 字段）匹配，不是数组下标。

#### 背包操作（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/item/inventory/use_item` | 使用物品（4 路分派） | `{"bag":0,"slot":3}` | `{"ok":true,"state":<Inventory>}` |
| POST | `/api/item/inventory/accept_dice` | 接受掷骰结果 | 无 body | `{"ok":true,"state":<Player>}` |
| POST | `/api/item/inventory/reject_dice` | 拒绝掷骰结果 | 无 body | `{"ok":true}` |
| POST | `/api/item/inventory/discard_item` | 丢弃物品 | `{"bag":0,"slot":5}` | `{"ok":true,"state":<Inventory>}` |
| POST | `/api/item/inventory/sell_item` | 出售物品（价格=静态表） | `{"bag":0,"slot":5}` | `{"ok":true,"price":15,"state":<Inventory>}` |
| POST | `/api/item/inventory/move_item` | 移动/堆叠物品 | `{"bag":0,"slot":3,"count":1,"to_bag":0,"to_slot":4}` | `{"ok":true,"state":<Inventory>}` |
| POST | `/api/item/inventory/{role}/equip_item` | 穿装备（二选一） | `{"bag":0,"slot":3}` 或 `{"category":512}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/item/inventory/{role}/unequip_item` | 脱装备 | `{"slot":2}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/item/inventory/{role}/put_jewel` | 镶嵌宝石 | `{"bag":0,"slot":3,"equip_slot":3}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/item/inventory/{role}/enchant` | 强化装备（消耗卷轴） | `{"bag":0,"slot":3,"equip_slot":5}` | `{"ok":true,"state":<Party>}` |

**常见错误**：
- use_item：非消耗品 → `item not usable`；冷却中 → `on cooldown`（不消耗）
- 骰子：掷出后返回 `base/pending/delta`（未应用），需 accept_dice/reject_dice 处理；无 pending → `no dice result pending`
- put_jewel：无孔 → `no socket`；非宝石 → `not jewel`；**镶嵌后自动消耗背包宝石**
- enchant：非强化卷轴 → `not enchant scroll`；不可强化 → `cannot enchant`；失败 → `enchant failed`；**成功才消耗卷轴 1 张**

#### 商店

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| GET | `/api/item/shop/items` | 当前商店商品 | — | `{"items":[{"slot":0,"category":5,"count":1,"price":15,"name":"恢复药水"}]}` |
| POST | `/api/item/shop/buy_item` | 购买商品 | `{"slot":0}` | `{"ok":true,"state":<Inventory>}` |

> ⚠️ 非商店界面时商品表为空（`items:[]`）——需先与商人对话进入商店（见 7.5）。
> 金币不足 → `not enough money`；无商品 → `item not found`。

### 6.5 quest（任务）

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/quest` | 任务复合 | `{"active":[...],"list":[...],"completed":[...]}` |
| GET | `/api/quest/active` | 已接任务（含进度，名称注入） | `{"quests":[{"quest_id":381,"id_name":"拯救村子"}]}` |
| GET | `/api/quest/list` | 已接任务轻量列表 | `{"quests":[{"slot":0,"quest_id":381,"name":"拯救村子"}]}` |
| GET | `/api/quest/{id}` | 单任务静态详情 | ⚠️ **恒 `{"error":"not found"}`**（stub） |
| GET | `/api/quest/completed` | 已完成任务 | `{"quests":[...]}`（可能占位空） |
| POST | `/api/quest/quit_quest` | 放弃任务 | `{"quest_id":381}` → `{"ok":true}` |

### 6.6 ui（界面与对话）

#### 界面状态读（GET）

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/ui/` | 界面状态复合 | `<GameState>` |
| GET | `/api/ui/screen` | 当前界面名 | `{"screen":"world"}` |
| GET | `/api/ui/panel` | 当前面板 | `{"panel":"inventory"}` 或 `{"panel":null}` |
| GET | `/api/ui/dialog` | **统一对话/弹窗检测** | `<DialogContent>`（见 5.9） |

#### 对话与弹窗操作（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/ui/start_interact` | 开始与附近 NPC 交互 | 无 body | `{"ok":true}` 或 `{"ok":false,"error":"no npc nearby"}` |
| POST | `/api/ui/dialog/select` | 选择对话选项（唯一选择端点） | `{"action":"next"}` 或 `{"index":0}` | `{"ok":true}` |

**select 支持动作**（依当前对话 type）：
- story：`next` / `skip`
- npc：`next` 或 `index`
- npc_quest：`complete` / `close`
- wipeout：`revive` / `special_revive` / `game_over`
- popup：`ok` / `cancel`
- 面板态：`close`；save_slot 面板另接受 `save`

#### 界面操作（POST）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/ui/go_main_menu` | 回到主菜单 | 无 body | `{"ok":true,"state":<Player>}` |
| POST | `/api/ui/open_panel` | 打开面板 | `{"panel":"inventory"}` | `{"ok":true,"state":<GameState>}` |
| POST | `/api/ui/close_panel` | 关闭当前面板 | 无 body | `{"ok":true,"state":<GameState>}` |

**面板白名单**（真机验证可 API 打开）：`character_info` / `inventory` / `skills` / `mercenary` / `quests` / `settings`。
其他面板（shop/craft/npc/options/world_map 等）需游戏内上下文 → `panel requires in-game context`。

### 6.7 debug（调试）

| 方法 | 路径 | 用途 | 返回 |
|---|---|---|---|
| GET | `/api/debug/ui` | 调试用 UI 原始 JSON（不走 ControllerGuard） | `<原始 gamestate JSON>` |

---

## 7. 玩法指南：用 API 完成游戏内目标

### 7.1 从主菜单进入游戏

```bash
# 1. 查存档
curl http://<手机IP>:8088/api/system/game/info

# 2. 创建新档（或 enter_slot 进已有档）
curl -X POST http://<手机IP>:8088/api/system/create_slot -d '{"slot":1,"class_idx":2}'

# 3. 跳过初始剧情（screen=story 时）
curl -X POST http://<手机IP>:8088/api/ui/dialog/select -d '{"action":"skip"}'

# 4. 确认进入世界
curl http://<手机IP>:8088/api/ui/screen
```

### 7.2 移动与探索

```bash
# 看地图与出口
curl http://<手机IP>:8088/api/world/map

# 计算到出口的距离
curl "http://<手机IP>:8088/api/world/map/distance?tx=24&ty=19"

# 走过去（到达出口 tile 自动切图）
curl -X POST http://<手机IP>:8088/api/world/movement/move_to -d '{"x":384,"y":304}'

# 切图后确认新地图
curl http://<手机IP>:8088/api/world/map/id
```

### 7.3 战斗（打怪刷级）

```bash
# 1. 找敌人
curl http://<手机IP>:8088/api/world/map/enemies

# 2. 靠近敌人
curl -X POST http://<手机IP>:8088/api/world/movement/move_to -d '{"x":480,"y":400}'

# 3. 主控攻击目标
curl -X POST http://<手机IP>:8088/api/character/combat/0/attack_target -d '{"target_slot":12}'

# 4. 战斗中放技能
curl http://<手机IP>:8088/api/character/party/0/skills
curl -X POST http://<手机IP>:8088/api/character/combat/0/cast_skill -d '{"action_id":5}'

# 5. 停止战斗 / 切换主控
curl -X POST http://<手机IP>:8088/api/character/combat/0/stop_combat
curl -X POST http://<手机IP>:8088/api/character/combat/switch_player -d '{"slot":1}'

# 6. 回血
curl -X POST http://<手机IP>:8088/api/item/inventory/use_item -d '{"bag":0,"slot":3}'
```

**挂机战斗**：`set_auto_attack {on:true}` + `attack_target` 一个目标后角色持续作战（受击自动还击）。

### 7.4 装备与背包管理

```bash
# 看背包 / 穿装备 / 脱装备 / 卖垃圾 / 整理
curl http://<手机IP>:8088/api/item/inventory/items
curl -X POST http://<手机IP>:8088/api/item/inventory/0/equip_item -d '{"bag":0,"slot":3}'
curl -X POST http://<手机IP>:8088/api/item/inventory/0/unequip_item -d '{"slot":2}'
curl -X POST http://<手机IP>:8088/api/item/inventory/sell_item -d '{"bag":0,"slot":5}'
curl -X POST http://<手机IP>:8088/api/item/inventory/move_item -d '{"bag":0,"slot":3,"count":5,"to_bag":1,"to_slot":0}'

# 强化装备（消耗背包中的强化卷轴）
curl -X POST http://<手机IP>:8088/api/item/inventory/0/enchant -d '{"bag":0,"slot":3,"equip_slot":5}'
```

### 7.5 商店买卖

```bash
# 1. 找商人（interactives）并走到旁边
curl http://<手机IP>:8088/api/world/map/interactives
curl -X POST http://<手机IP>:8088/api/world/movement/move_to -d '{"x":<商人x>,"y":<商人y>}'

# 2. 交互 → 查看对话 → 选商店选项
curl -X POST http://<手机IP>:8088/api/ui/start_interact
curl http://<手机IP>:8088/api/ui/dialog
curl -X POST http://<手机IP>:8088/api/ui/dialog/select -d '{"index":0}'

# 3. 确认进入商店 → 看商品 → 购买
curl http://<手机IP>:8088/api/ui/screen
curl http://<手机IP>:8088/api/item/shop/items
curl -X POST http://<手机IP>:8088/api/item/shop/buy_item -d '{"slot":0}'

# 4. 关面板离开
curl -X POST http://<手机IP>:8088/api/ui/close_panel
```

### 7.6 任务流程

任务接取/交付通过 NPC 对话完成（无直接接任务端点）：

```bash
# 与 NPC 对话 → 选任务选项 → 查看已接任务
curl -X POST http://<手机IP>:8088/api/ui/start_interact
curl http://<手机IP>:8088/api/ui/dialog
curl -X POST http://<手机IP>:8088/api/ui/dialog/select -d '{"index":0}'
curl http://<手机IP>:8088/api/quest/active

# 交付（npc_quest 面板）
curl -X POST http://<手机IP>:8088/api/ui/dialog/select -d '{"action":"complete"}'

# 放弃任务（可选）
curl -X POST http://<手机IP>:8088/api/quest/quit_quest -d '{"quest_id":381}'
```

### 7.7 角色养成

```bash
# 查看技能点/属性点
curl http://<手机IP>:8088/api/character/party/0/status
curl http://<手机IP>:8088/api/character/party/0/skills

# 批量分配属性点（strength/agility/vitality/intelligence/spirit）
curl -X POST http://<手机IP>:8088/api/character/grow/0/add_stat -d '{"attrs":{"strength":2,"agility":1}}'

# 学习技能（主角）
curl -X POST http://<手机IP>:8088/api/character/grow/add_skill -d '{"action_id":3,"level":1}'

# 重置（API 免费，游戏内需内购）——谨慎使用
curl -X POST http://<手机IP>:8088/api/character/grow/reset_stat
curl -X POST http://<手机IP>:8088/api/character/grow/reset_skill
```

### 7.8 队伍与佣兵

```bash
# 查看佣兵 → 入队 → 离队 → 取装备 → 遣散
curl http://<手机IP>:8088/api/character/mercenary
curl -X POST http://<手机IP>:8088/api/character/party/include -d '{"mercenary_slot":0}'
curl -X POST http://<手机IP>:8088/api/character/party/exclude -d '{"mercenary_slot":0}'
curl -X POST http://<手机IP>:8088/api/character/party/withdraw -d '{"mercenary_slot":0,"equip_slot":3}'
curl -X POST http://<手机IP>:8088/api/character/party/discharge -d '{"mercenary_slot":0}'   # 不可逆！
```

### 7.9 存档与备份

```bash
# 关键操作前存档！
curl -X POST http://<手机IP>:8088/api/system/save

# 导出存档备份（base64 → 解码得 saveN.dat，magic 293a1962）
curl "http://<手机IP>:8088/api/system/export_save_file?slot=0" -o save0.json
```

---

## 8. 越权操作（OP 端点）

> ⚠️ **OP = 游戏内做不到的越权操作**（改数据/强行操作）。**已挂载 HTTP 路由且当前无鉴权开关**，任何局域网客户端可调用——仅限可信环境，滥用会破坏存档与体验。
> ⚠️ 规划中的 OP 权限开关（默认关闭 + 独立鉴权）**尚未实现**（见 backlog P0）。

### 8.1 已实现（6 个）

| 方法 | 路径 | 用途 | 请求 | 返回 |
|---|---|---|---|---|
| POST | `/api/op/character/{role}/hp` | 直写血量 | `{"hp":8000}`（截断 0..max_hp） | `{"ok":true,"state":<Party>}` |
| POST | `/api/op/character/{role}/mp` | 直写魔力 | `{"mp":200}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/op/character/{role}/experience` | 直写经验（不触发升级结算） | `{"exp":12000}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/op/character/{role}/level` | 直写等级（完整升级结算） | `{"level":30}` 或 `{"level":200,"force":true}` | `{"ok":true,"state":<Party>}` |
| POST | `/api/op/character/{role}/set_attr` | 批量直写基础属性 | `{"stats":{"strength":10,"agility":7}}` 或 `{"stats":{"0":10}}` | `{"ok":true,"set":[...]}` |
| POST | `/api/op/inventory/add` | 直接生成物品进背包 | `{"category":1,"count":5}` | `{"ok":true,"state":<Inventory>}` |

**level 注意事项**：
- 默认校验 1-105（游戏上限）；超出 → `level 1-105`
- `force:true` 跳过校验——⚠️ 等级存储为 int8，>127 溢出为负（实测 200 → -56），后果自负
- 降级 → `level down not allowed`

**set_attr 说明**：键用主属性英文名（strength/agility/vitality/intelligence/spirit）或索引 0-4，值 0-255；总属性 = 基础+分配+加成+动态。

### 8.2 定稿未实现（15 个，全部占位）

| 路径 | 用途 | 状态 |
|---|---|---|
| `POST /api/op/quest/accept` | 接取任务（绕过 NPC） | ⏳ `{"ok":false,"error":"not implemented"}` |
| `POST /api/op/quest/complete` | 完成任务（无视条件） | ⏳ 同上 |
| `POST /api/op/character/{role}/status-point` | 设置属性点 | ⏳ 同上 |
| `POST /api/op/character/{role}/skill-point` | 设置技能点 | ⏳ 同上 |
| `POST /api/op/character/{role}/skill-level` | 设置技能等级 | ⏳ 同上 |
| `POST /api/op/party/swap` | 队伍换位 | ⏳ 同上 |
| `POST /api/op/inventory/set-slot` | 格子设物品+数量 | ⏳ 同上 |
| `POST /api/op/inventory/set-equip` | 修改装备属性 | ⏳ 同上 |
| `POST /api/op/inventory/money` | 修改金币 | ⏳ 同上 |
| `POST /api/op/craft/mix-direct` | 免配方机直合成 | ⏳ 同上 |
| `POST /api/op/combat/{role}/heal` | 回血回蓝 | ⏳ 同上 |
| `POST /api/op/combat/{role}/rest` | 休息恢复 | ⏳ 同上 |
| `POST /api/op/combat/{role}/revive` | 复活 | ⏳ 同上 |
| `POST /api/op/combat/{role}/hate` | 仇恨操作 | ⏳ 同上 |
| `POST /api/op/movement/teleport` | 传送/切图 | ⏳ 同上 |

---

## 9. 静态数据表查询

游戏全部数值表已解析为 JSON 数据库（随模块打包），可离线查询。

### 9.1 常用表

| 表名 | 内容 | 记录数 |
|---|---|---|
| `ITEMDATABASE` | 物品主表 | 1,018 |
| `ITEMCLASSBASE` | 物品分类 | — |
| `ITEMOPTINFOBASE` | 词缀定义 | 37 |
| `MONDATABASE` | 怪物 | 553 |
| `QUESTINFOBASE` | 任务 | 507 |
| `QUESTREWARDBASE` | 任务奖励 | 394 |
| `MAPINFOBASE` | 地图 | 416 |
| `CHARCLASSBASE` | 职业 | 6 |
| `SKILLDESCBASE` | 技能 | 114 |

### 9.2 查询示例

```bash
# 表清单
curl http://<手机IP>:8088/api/system/tables

# 物品表搜索
curl "http://<手机IP>:8088/api/system/tables/ITEMDATABASE/search?q=治疗"
# {"items":[{"index":788,"name":"鑫迪的治疗药",...}]}

# 单表全量 / 剧情事件 / 中文字幕
curl http://<手机IP>:8088/api/system/tables/MONDATABASE
curl http://<手机IP>:8088/api/system/tables/story-events
curl "http://<手机IP>:8088/api/system/tables/text?lang=zh-Hans"
```

---

## 10. 事件流（感知游戏变化）

`GET /api/system/events/` 提供**差异检测**事件（零 hook，轮询对比快照）：

```bash
curl http://<手机IP>:8088/api/system/events/
```

```json
{ "events": [
  { "type": "money", "old": 100, "new": 150 },
  { "type": "hp", "role": 0, "old": 10598, "new": 8000 },
  { "type": "level_up", "role": 1, "old": 10, "new": 11 },
  { "type": "inventory", "old": 13, "new": 12 }
] }
```

**使用规则**：
- **500ms-1s 周期性轮询**
- 首次调用仅建立基线，返回空列表
- 事件类型：`money` / `hp` / `mp` / `exp` / `level_up` / `move` / `inventory`

**AI 建议**：轮询事件 + 定期快照（`/api/system/snapshot`），即可不读屏感知游戏状态。

---

## 11. 给 AI 代理的决策建议

### 11.1 感知 → 决策 → 行动循环

```
1. 感知（GET，并行）
   - /api/system/snapshot      # 全局状态
   - /api/ui/                   # 界面状态（screen/dialog_active）
   - /api/world/map/units       # 周围单位（敌人/NPC）
   - /api/quest/active          # 当前任务
   - /api/system/events/        # 变化事件

2. 决策
   - dialog_active=true → 处理对话（GET /api/ui/dialog → select）
   - screen=story       → select action=skip
   - hp 低              → use_item 药水
   - 有敌人且距近       → attack_target → cast_skill
   - 有任务目标         → move_to → start_interact
   - 空闲               → 探索/移动

3. 行动（POST）
   - 移动：move_to / walk_dir+stop_move
   - 战斗：attack_target / cast_skill / stop_combat
   - 物品：use_item / equip_item / sell_item
   - 对话：start_interact → dialog/select
   - 存档：save

4. 校验
   - 检查返回 ok 与 state；失败读 error 修正参数
```

### 11.2 关键原则

1. **操作前检查 dialog**：`dialog.active=true` 时先处理对话，否则操作被阻塞
2. **操作后检查 state**：验证行动生效（如移动后坐标变化）
3. **大动作前先存档**：`POST /api/system/save`
4. **目标定位用 units 的 slot**：攻击/交互都传场景单位的 `slot`
5. **坐标换算**：瓦片坐标 ×16 = 像素坐标（move_to 用像素；map/exits 给 tile 与 px 双份）
6. **慢节奏操作**：移动是逐帧走路，用 `map/distance` 估算时间或轮询坐标变化
7. **善用静态表**：物品/技能/任务/地图数据在 `/api/system/tables` 查询
8. **失败重试**：`{"error":"not ready"}` 等 1-2 秒重试；`not in game` 需先 enter_slot/create_slot

### 11.3 典型 AI 决策伪代码

```text
loop:
    state = GET /api/ui/
    if state.screen == "story":   POST ui/dialog/select {action:"skip"}; continue
    if state.dialog_active:
        dlg = GET /api/ui/dialog
        if dlg.type in (popup, save, sell, quest): POST select {action:"ok"/"confirm"}
        elif dlg.type == "npc":    POST select {index:0}
        elif dlg.type == "wipeout": POST select {action:"game_over"}
        continue
    if state.screen != "world":   continue

    snap = GET /api/system/snapshot
    if party[0].hp < party[0].max_hp * 0.3:
        找到药水 → POST item/inventory/use_item; continue

    units = GET /api/world/map/units
    enemy = 最近的 status==2 且 distance>=0 的单位
    if enemy:
        POST world/movement/move_to {x:enemy.x, y:enemy.y}
        if 距离近: POST character/combat/0/attack_target {target_slot:enemy.slot}
    else:
        找出口/任务点 → POST world/movement/move_to
    sleep 1s
```

---

## 12. 常见问题与注意事项

| 问题 | 处理 |
|---|---|
| 返回 `{"error":"not ready"}` | native 未就绪（游戏刚启动），等 1-2 秒重试 |
| 返回 `{"error":"not in game"}` | 未进入存档，先 enter_slot / create_slot |
| 写操作无反应 | 先 `GET /api/ui/dialog` 检查弹窗，处理后再操作 |
| enter_slot 后崩溃 | 存档槽不存在时调用会崩——先查 `game/info.save_slots` 确认 `exists=true` |
| 移动不生效 | 确认 `screen=world`；剧情/切图中操作自动终止 |
| 商店 items 为空 | 需先与商人交互进入商店界面 |
| attack 返回 `target not found` | target_slot 用 `map/units` 返回的 `slot`（每帧会变，重新查询） |
| cast 返回 `skill not learned` | action_id 必须是已学技能，先查 `party/{slot}/skills` |
| 等级溢出变负数 | 等级字段是 int8，>127 溢出；OP level 勿超 127 |
| 服务无法访问 | 确认游戏进程存活、同局域网、IP 正确 |

**安全与风险**：
- **无鉴权**：所有端点（含 OP）局域网内可访问，勿暴露公网
- **OP 破坏性**：直改等级/属性/物品可能损坏存档，用前先存档
- **死亡处理**：全灭 → `screen=wipeout`；`revive` 在盗版版走网络链会失败，`game_over` 回主菜单可重进档
- **教学状态**：新档可能触发 `tutorial_pause`（药水教学），游戏暂停移动，需使用药水后恢复

---

## 13. 附录：参考表

### 13.1 主属性

| 索引 | 名称 | 效果（大修版） |
|---|---|---|
| 0 | 力量 | 物攻 + HP |
| 1 | 敏捷 | 物攻 + 命中 + 回避 |
| 2 | 体力 | HP + 防御 |
| 3 | 智力 | 魔攻 + MP |
| 4 | 精力 | 魔攻 + 命中 |

> API 参数（add_stat/set_attr）用英文名：`strength`/`agility`/`vitality`/`intelligence`/`spirit` 或索引 0-4。

### 13.2 stats 数组关键索引（战斗属性）

| 索引 | 属性 | 说明 |
|---|---|---|
| 0 | crit_rate | 暴击率 ×10（默认 30） |
| 3 | crit_damage | 暴击伤害 ×10（默认 1000） |
| 4 | attack | 攻击 |
| 8 | magic_attack | 魔攻 |
| 11 | magic_resist | 魔法抵抗 |
| 13 | dexterity | 敏捷（总） |
| 14 | hit_base | 命中率基数 |
| 15 | hit_rate | 命中率百分比 |
| 17 | defense | 防御 |
| 18 | phys_reduce | 物理减伤 |
| 19 | wdr | 武器伤害减免 ×10 |
| 20 | sub_weapon_attack | 副手武器攻击 |
| 28 | level_attr | 等级驱动属性 |
| 30 | max_hp | HP 上限 |
| 31 | max_mp | MP 上限 |

> ⚠️ 其余索引为占位（`attr_<id>`），不可直接套用词缀编码（ITEMOPTINFOBASE 编码与 stats 索引不一致）。

### 13.3 稀有度档位

| rarity | 档位 | 品质 |
|---|---|---|
| 0 | 白 | common |
| 1 | 绿 | uncommon |
| 2 | 蓝 | rare |
| 3 | 黄 | unique |
| 4 | 紫 | epic |

### 13.4 常用物品类别（category）

| category | 物品 |
|---|---|
| 1 | 治疗药水 |
| 5-8 | 恢复药水（小→大） |
| 15 | 超级药水 |
| 26/27 | 复活卷轴 |
| 51 | 技能书 |
| 62 | 佣兵封印卡 |
| 75/76 | 短剑/匕首 |
| 925/926 | 技能重置/属性重置 |
| 934-939 | 解封卷轴 |
| 1007-1009 | 开箱 |
| 16-25 | 强化卷轴（16-19 武器、20 混沌武器、21-24 防具、25 混沌防具） |
| 28-32 | 宝石（28 低级..32 混沌） |
| 52-56 | 骰子（陨子） |

> 完整物品名查询：`GET /api/system/tables/ITEMDATABASE/search?q=<名称>`

---

## 附：实现状态速查

| 端点 | 状态 |
|---|---|
| 全部 GET 读端点（character/world/item/quest/ui/system） | ✅ 实现（`quest/{id}` 恒 not found 除外） |
| POST 操作（移动/战斗/背包/队伍/成长/商店/对话/存档） | ✅ 实现 |
| `POST /api/character/combat/{role}/set_skill_usage` | ⏳ 占位（native 无单技能档位，待前置探索） |
| `GET /api/system/tables/{table}/download`、`/api/system/help`、`/api/system/download` | ⏳ 占位 |
| `GET /api/quest/{id}` 单任务详情 | ⏳ 恒 not found |
| OP 已实现 6 个 + 定稿 15 个 | ✅ 6 个实现 / ⏳ 15 个占位 |

> 本文档数据来源：module controller 实际路由（v0.5.13）、`game_read.cpp` native 字段、`docs/api-reference.md`、`docs/game-systems.md`。若与代码不符，以代码为准。
