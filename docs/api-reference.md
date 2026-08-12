# API 参考手册

> **本文档 = API 规格（面向调用方）**：每个 API 的路径、用途、请求格式、返回格式与注意事项。
> 技术实现细节（VMA/函数签名/调用链/游戏内机制）见 `docs/api-technical-spec.md`。
> 状态：**v0.5.0（2026-08-13 按 7 域分组重构）**。所有端点路径已对照 controller 真实路由逐条核对。
>
> 通用约定：
> - 服务地址：`http://<设备IP>:8088`（局域网，模块监听 0.0.0.0）
> - 请求/响应均为 JSON；写操作（POST）的 body 是 JSON 字符串
> - `role` = 出战槽位 0..2；`bag` = 背包袋 0..5；`slot` = 袋内槽位 0..15
> - 写操作成功返回 `{"ok":true,...}`；失败返回 `{"ok":false,"error":"<原因>"}`
> - native 未就绪（模块初始化中）时所有端点返回 `{"error":"not ready"}`
> - 写操作返回会附带 `state` 字段 = 操作后的最新状态快照（类型见各端点说明）

---

## 0. 分组总览（v0.5.0 起）

> **v0.5.0 起 API 按游戏实体/领域分 7 组**，不再按读写性质划分（info/data/action 前缀已废弃）：
> 每组内 GET（读）与 POST（写）混合，读写同域，GET/POST 由 HTTP 方法区分。OP（越权操作）独立保留。
>
> **旧→新路径迁移表见文末「十一、迁移对照表（v0.4.65 → v0.5.0）」**。

| 分组 | 顶层路径 | 覆盖实体 | 端点数 |
|---|---|---|---|
| **character**（角色与队伍） | `/api/character/*` | 出战角色 party、佣兵 mercenary、战斗 combat、角色成长 grow | 33 |
| **world**（地图与移动） | `/api/world/*` | 当前地图 map、移动操作 movement、静态地图 maps | 17 |
| **item**（物品与背包） | `/api/item/*` | 背包 inventory、商店 shop | 16 |
| **quest**（任务） | `/api/quest/*` | 任务 | 6 |
| **ui**（界面与对话） | `/api/ui/*` | 界面状态 ui、对话 dialog | 17 |
| **system**（系统与会话） | `/api/system/*` | 健康 health、游戏整体 game、事件流 events、存档 save、静态数据表 tables、多语言文本 text、剧情事件 story-events | 16 |
| **op**（越权操作） | `/api/op/*` | 改数据/强行操作（需独立权限，默认关闭） | 6 已实现 + 15 定稿 |
| debug（调试） | `/api/debug/*` | 开发期调试 | 1 |

> 全量端点 = 112（character 33 + world 17 + item 16 + quest 6 + ui 17 + system 16 + op 6 + debug 1；其中 GET 65 / POST 47）。

---

## 一、数据模型

各端点返回的 JSON 结构引用以下数据模型。

### Player（玩家）

```json
{
  "money": 72503,
  "map_id": 30,
  "x": 304,
  "y": 376,
  "active_quest": 0,
  "main_mercenary_slot": 0,
  "party_count": 2
}
```

| 字段 | 说明 |
|---|---|
| `money` | 金币（实时） |
| `map_id` | 实时地图 ID（MAPINFOBASE 记录下标，0-415） |
| `x`/`y` | 玩家实时坐标（像素） |
| `active_quest` | 当前激活任务 ID |
| `main_mercenary_slot` | 当前控制角色槽 |
| `party_count` | 出战人数 |

### Role（出战角色）

```json
{
  "type": 1, "name_id": 2210, "level": 27,
  "hp": 10598, "mp": 200, "max_hp": 10664, "max_mp": 250,
  "exp": 12000, "exp_next": 15000,
  "stats": { "0": 60, "1": 40, "30": 10664, "31": 212 },
  "main_stats": [96, 139, 101, 54, 38],
  "status_point": 78,
  "attrs": [
    { "id": 0, "name": "力量", "value": 96 },
    { "id": -1, "name": "能力点", "value": 78 },
    { "id": 30, "name": "HP上限", "value": 10664 }
  ],
  "equipment": [
    { "slot": 0, "type_flags": 21352, "category": 333, "rarity": 3,
      "damage": 0, "defense": 37, "magic_rate": 0, "socket": 69, "enchant": 35072,
      "options": [1, 1, 1, 18, 23, 17, 19, 36], "name": "光荣的火冠" },
    null
  ]
}
```

- `type` 角色类型；`level/hp/mp/max_hp/max_mp/exp/exp_next` 实时
- `stats` 战斗属性（0..31）；`main_stats` 主属性（0-4=力量/敏捷/体力/智力/精力）
- `status_point` 剩余能力点；`attrs` 带名属性数组（Kotlin 注入）
- `equipment` 10 装备槽（damage/defense/magic_rate/socket/enchant/options 词缀 + name 联查）

### Inventory（背包）

```json
{
  "bags": [
    { "bag": 0, "items": [
      { "slot": 3, "type_flags": 512, "category": 1, "count": 1, "rarity": 0,
        "damage": 0, "defense": 0, "magic_rate": 0, "socket": 0, "enchant": 0,
        "options": [], "name": "治疗药水" }
    ], "capacity": 16, "slot_count": 13 }
  ]
}
```

- `category` = ITEMDATABASE itemId（`UTIL_GetBitValue(flags,15,6)`）；`name` 由 Kotlin 联查注入
- `capacity` 袋容量 16 格；`slot_count` 占用数
- 附加字段（v0.4.64）：`option_ids`/`options` 词缀 ID 与值、`socket_filled`/`socket_total` 宝石孔、`enchant_id`/`enchant_level`/`chaos` 附魔、`chaos_level`/`chaos_rate` 混沌

### Skills（角色技能）

```json
[
  { "role": 0, "skills": [ { "action_id": 0, "level": 1 }, ... ],
    "unlock_bitmap": 65535, "active_skill_id": 0, "skill_points": 6 },
  null
]
```

来源：技能链表（action_id/level）、+0x2B0 解锁位图、+0x280 当前技能、+0x328 技能点。

### Mercenaries（全部佣兵）

```json
[
  { "slot": 0, "type": 0, "flags": 219, "in_party": true,
    "name": "凯恩", "level": 27, "x": 320, "y": 480 },
  { "slot": 1, "type": 2, "flags": 77, "in_party": false,
    "name": "西雷斯", "level": 26, "x": 2048, "y": 2048 }
]
```

- `flags` bit0=占用 bit1=在队伍；未上场佣兵坐标 2048 = 未激活哨兵

### Path（寻路）

```json
{ "target": { "x": 600, "y": 400 },
  "start": { "x": 40, "y": 168 },
  "in_map": true,
  "found": true,
  "distance": 72,
  "nearest": null,
  "path": [ { "x": 40, "y": 168 }, ... ] }
```

- 自研 BFS 导航（v0.4.29，替代游戏 CHAR_SearchPath）：瓦片矩阵（bit3=阻挡 bit7=出口）+ 单位占用（NPC/怪物不可穿越，v0.4.30）
- `found=false` 时返回 `nearest`（最近可达 tile）与最近可达路径；路径节点 = tile 中心像素（tile×16+8）

### GameState（游戏界面状态）

```json
{ "screen": "dialog", "dialog_active": true, "dialog": { "text": "是否出售？", "has_ok": true, "has_cancel": false } }
```

| 字段 | 说明 |
|---|---|
| `screen` | `"loading"` / `"main_menu"` / `"world"` / `"dialog"`（弹窗）/ `"story"`（剧情 AVG，v0.4.27）/ 面板名（character_info/inventory/skills/mercenary/quests/settings/shop/craft/npc/npc_quest/npc_rest/npc_revive/save_slot/character_select/options/shortcut/world_map/input_count/choice/wipeout/daily_reward/in_app/ui_panel） |
| `story` | 仅 screen=story：`active`/`speaker`/`text`/`index`/`count` |
| `dialog_active` | 是否有阻塞弹窗。**操作前置检查**：为 true 时操作会被 UI 阻塞 |
| `dialog` | 仅 dialog_active=true：`text`/`has_ok`/`has_cancel`/`buttons`（按钮文案数组） |

### Snapshot（快速状态快照）

```json
{
  "screen": "world",
  "money": 72503,
  "map_id": 30, "x": 304, "y": 376,
  "main_mercenary_slot": 0, "party_count": 2,
  "party": [ { "type": 1, "level": 27, "hp": 10598, "mp": 200,
    "max_hp": 10664, "max_mp": 250, "main_stats": [96, 139, 101, 54, 38],
    "equipment": [ { "slot": 0, "category": 333, "rarity": 3, "name": "光荣的火冠" }, null ],
    "name": "凯恩" } ],
  "mercenaries": [ { "slot": 0, "type": 0, "in_party": true, "name": "凯恩", "level": 27, "x": 320, "y": 480 } ]
}
```

一站式聚合：UI 状态 + 玩家全局 + 队伍摘要（等级/HP MP/主属性/装备名/角色名）+ 全部佣兵概要。装备名称由 Kotlin 注入。

### DialogContent（对话内容）

`GET /api/ui/dialog/content` 返回的五态结构（v0.4.27/v0.4.31/v0.4.35）：

| type | 结构 | 选项 |
|---|---|---|
| `story` | 剧情对话（AVG） | `[{id:"next",label:"下一句"},{id:"skip",label:"跳过"}]` |
| `npc` | NPC 对话 | `[{id:"next",label:"下一句"}]` + 分支选项 |
| `popup` | 弹窗 | `[{id:"ok",...},{id:"cancel",...}]` |
| `wipeout` | 死亡面板 | `[{id:"revive",label:"复活"},{id:"special_revive",label:"特殊复活"},{id:"game_over",label:"游戏结束"}]` |
| `none` | 无对话 | — |

通用字段：`type`、`options`（选项数组）、`active`、对话态附加 `speaker`/`text`/`index`/`count`。

### 静态数据表

每条记录结构见 `static-data/json/tables/<表名>.json`，已验证主要表：
- **ITEMDATABASE**（物品，1,018 条）：记录 = `{ "hex", "u16", "text_0" }`，+0 名称 text_id
- **MONDATABASE**（怪物，553 条）：+0 名称、+0x0f~0x14 六项成长属性、+0x22 技能起点、+0x23 技能数
- **QUESTINFOBASE**（任务，507 条）：+2 标题、+14 详情、+16 进度、+18 完成、+26/+28 奖励起止
- **QUESTREWARDBASE**（任务奖励，394 条）：+0 item_id、+2 数量、+4 职业掩码
- **MAPINFOBASE**（地图，416 条）：+0 地图 ID（text_id 3513-3928）、名称
- **CHARCLASSBASE**（职业，6 条）：+2 描述文本
- **SKILLDESCBASE**（技能，114 条）：+2 技能文本

> ⚠️ 两套地图编号：静态表 MAPINFOBASE 的 `map_id` 是 **text_id**（3513-3928）；运行时 `/api/world/map/id` 返回 **MAPINFOBASE 记录下标**（0-415，如 30=影子丛林1）。`/api/world/maps/{map_id}` 对 0-415 按下标查询，其余按 text_id 兼容。

---

## 二、character（角色与队伍）— GET/POST /api/character/*

**角色实体全生命周期**：出战角色（party）状态与操控、佣兵（mercenary）管理、战斗行为（combat）、角色成长（grow）。

### 2.1 出战角色 party

#### 出战角色复合

`GET /api/character/party`

**用途**：获取 3 槽出战角色完整状态（含装备名/属性名注入）。

**返回格式**：`[ <Role>, <Role>, ... ]`（Role 模型数组）

#### 出战人数

`GET /api/character/party/count`

**用途**：获取出战人数。

**返回格式**：`{ "count": 2 }`

#### 主控角色

`GET /api/character/party/leader`

**用途**：获取当前主控角色（main_mercenary_slot 对应槽）。

**返回格式**：`<Role 模型>` 或 `{"error":"not found"}`

#### 指定出战槽

`GET /api/character/party/{slot}`

**用途**：获取指定出战槽（0..2）完整状态。

**返回格式**：`<Role 模型>` 或 `{"error":"not found"}`

#### 角色类型

`GET /api/character/party/{slot}/id`

**用途**：获取指定出战槽角色类型。

**返回格式**：`{ "type": 1 }`

#### 角色名

`GET /api/character/party/{slot}/name`

**用途**：获取指定出战槽角色名。

**返回格式**：`{ "name": "凯恩" }`

#### 等级

`GET /api/character/party/{slot}/level`

**用途**：获取指定出战槽等级。

**返回格式**：`{ "level": 27 }`

#### 经验

`GET /api/character/party/{slot}/exp`

**用途**：获取指定出战槽经验/下一级。

**返回格式**：`{ "exp": 12000, "exp_next": 15000 }`

#### 血量

`GET /api/character/party/{slot}/hp`

**用途**：获取指定出战槽血量/上限。

**返回格式**：`{ "hp": 10598, "max_hp": 10664 }`

#### 魔力

`GET /api/character/party/{slot}/mp`

**用途**：获取指定出战槽魔力/上限。

**返回格式**：`{ "mp": 200, "max_mp": 250 }`

#### 战斗属性

`GET /api/character/party/{slot}/stats`

**用途**：获取指定出战槽战斗属性对象（0..31）。

**返回格式**：`{ "stats": { "0": 60, "1": 40, "30": 10664, "31": 212 } }`

#### 单个属性值

`GET /api/character/party/{slot}/stats/{attr}`

**用途**：获取指定出战槽单个属性值。

**返回格式**：`{ "attr": 0, "value": 60 }`

#### 装备列表

`GET /api/character/party/{slot}/equipment`

**用途**：获取指定出战槽装备列表（含名称注入）。

**返回格式**：`{ "equipment": [ <Item>, null, ... ] }`

#### 指定装备槽

`GET /api/character/party/{slot}/equipment/{equip_slot}`

**用途**：获取指定出战槽指定装备（0..9）。

**返回格式**：`<Item 对象>`（含 name）

#### 技能完整

`GET /api/character/party/{slot}/skills`

**用途**：获取指定出战槽技能完整信息（链表/位图/技能点/当前技能）。

**返回格式**：`{ "role": 0, "skills": [...], "unlock_bitmap": 65535, "active_skill_id": 0, "skill_points": 6 }`

#### 技能列表

`GET /api/character/party/{slot}/skills/list`

**用途**：获取指定出战槽技能列表。

**返回格式**：`{ "skills": [ { "action_id": 0, "level": 1 }, ... ] }`

#### 佣兵入队

`POST /api/character/party/include`

**用途**：把待命佣兵编入出战队伍。

**请求格式**：`{ "mercenary_slot": 1 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：已在队→`already in party`；满员→`party full`。

#### 佣兵离队

`POST /api/character/party/exclude`

**用途**：把出战佣兵移回待命。

**请求格式**：`{ "mercenary_slot": 1 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：主控→`cannot exclude leader`；任务 NPC→`cannot exclude quest npc`。

#### 佣兵遣散

`POST /api/character/party/discharge`

**用途**：遣散佣兵（MERCENARYSYSTEM_Release）。

**请求格式**：`{ "mercenary_slot": 27 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：无该槽角色→`mercenary not found`；主控/任务 NPC→不可遣散；⚠️ mercenary 端点 slot ≠ 参数 slot（两套索引）。

#### 取出佣兵装备

`POST /api/character/party/withdraw`

**用途**：取出佣兵指定装备槽到背包（CHAR_UnequipItemToInven）。

**请求格式**：`{ "mercenary_slot": 0, "equip_slot": 3 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：无佣兵→`mercenary not found`；装备槽越界→`bad slot`。

### 2.2 佣兵 mercenary

#### 全部佣兵

`GET /api/character/mercenary`

**用途**：获取全部佣兵（含未上场）。

**返回格式**：`[ <Mercenary>... ]`（Mercenaries 模型数组）

#### 佣兵槽列表

`GET /api/character/mercenary/list`

**用途**：获取非空佣兵槽 id 列表。

**返回格式**：`{ "slots": [0, 1, 3] }`

#### 指定佣兵槽

`GET /api/character/mercenary/{slot}`

**用途**：获取指定佣兵槽信息。

**返回格式**：`<Mercenary 对象>` 或 `{"error":"not found"}`

### 2.3 战斗 combat

#### 自动攻击开关

`POST /api/character/combat/{role}/config/auto-attack`

**用途**：开关指定出战槽的自动攻击。

**请求格式**：`{ "on": true }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

#### 技能使用开关

`POST /api/character/combat/{role}/config/skill-usage`

**用途**：开关战斗 AI 技能使用（CHAR_SetSkillUsage 写 [ch+0x3a0] bit0-2）。

**请求格式**：`{ "on": true }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：role 越界→`role not found`；缺 body→`bad body`。

#### 切换主控

`POST /api/character/combat/{role}/switch`

**用途**：切换主控角色到指定出战槽。

**请求格式**：无 body（role 在路径）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：不可切换角色返回 `switch failed`。

#### 释放技能

`POST /api/character/combat/{role}/cast`

**用途**：指定出战槽释放技能（CHAR_GetEnemyTarget + CHAR_SetActionID）。

**请求格式**：`{ "action_id": 5 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：未学技能→`skill not learned`；无目标→`no target`。

#### 攻击目标

`POST /api/character/combat/{role}/attack`

**用途**：指定出战槽攻击指定目标（CHAR_SetTarget+CHAR_MakeDefaultAttack）。

**请求格式**：`{ "target_slot": 5 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：目标无效→`target not found`；缺参→`target_slot required`。

#### 停止战斗

`POST /api/character/combat/{role}/stop`

**用途**：停止指定出战槽战斗（CHAR_StopCombat）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：非战斗态调用安全（清标志幂等）。

### 2.4 角色成长 grow

#### 学习技能

`POST /api/character/grow/skill`

**用途**：主角学习技能（消耗技能点）。

**请求格式**：`{ "action_id": 3, "level": 1 }`

**返回格式**：`{"ok":true,"state":<Skills 模型>}`

**注意**：主角专用，无 role 路径段。

#### 分配属性点

`POST /api/character/grow/{role}/stat`

**用途**：指定出战槽分配属性点（属性+1/能力点-1）。

**请求格式**：`{ "attr": 0 }`（0=力量 1=敏捷 2=体力 3=智力 4=精力）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：无能力点→`no status point`；attr 越界→`bad attr`。

#### 属性重置

`POST /api/character/grow/{role}/stat-reset`

**用途**：指定出战槽重置分配属性（CHAR_InitializeStatus：分配点归零+能力点按公式还原）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

#### 技能重置

`POST /api/character/grow/{role}/skill-reset`

**用途**：指定出战槽重置技能（CHAR_InitializeSkill：移除非基础技能+技能点还原）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

---

## 三、world（地图与移动）— GET/POST /api/world/*

**空间实体**：当前地图感知（map）、移动操作（movement）、静态地图查询（maps）。位置感知与位置操作一体。

### 3.1 当前地图 map

#### 当前地图复合信息

`GET /api/world/map`

**用途**：获取当前地图完整信息（地图 ID、坐标、瓦片、出口、全部场景单位）。

**返回格式**：
```json
{
  "map_id": 0, "x": 120, "y": 312,
  "tile": { "tx": 7, "ty": 19, "blocking": false },
  "exits": [ { "tx": 24, "ty": 19, "px": 384, "py": 304 }, ... ],
  "map_data": { "text_id": 3513, "name": "黑暗骑士团营地", "u16": [...], "hex": "..." },
  "units": [ <Unit>... ],
  "enemies": [ <Unit status==2>... ],
  "interactives": [ <Unit status==1>... ],
  "drops": []
}
```

**注意**：
- `exits` 出口区域（瓦片网格 bit7=1，切图用，v0.4.25）；`map_data` 静态信息（v0.4.58 附加）
- `units` 仅取一次本地复用（惰性缓存下重复调用会多次触发刷新）

#### 地图 ID

`GET /api/world/map/id`

**用途**：获取当前地图 ID。

**返回格式**：`{ "map_id": 30 }`

#### 所在瓦片

`GET /api/world/map/tile`

**用途**：获取玩家当前所在瓦片及通行状态。

**返回格式**：`{ "tile": { "tx": 7, "ty": 19, "blocking": false } }`

#### 出口区域

`GET /api/world/map/exits`

**用途**：获取当前地图全部出口区域。

**返回格式**：`{ "exits": [ { "tx": 24, "ty": 19, "px": 384, "py": 304 }, ... ] }`

#### 场景单位

`GET /api/world/map/units`

**用途**：获取当前地图全部场景单位（队伍/NPC/怪物，含 level/hp/mp/name）。

**返回格式**：
```json
{ "units": [ { "slot": 0, "status": 1, "type": 1, "level": 27, "hp": 10598, "mp": 200,
  "x": 320, "y": 480, "name": "凯恩", "distance": 0, "nearest_distance": -1 }, ... ] }
```

**注意**：v0.4.29 附加 `distance`（玩家 BFS 可达距离）/`nearest_distance`（不可达时最近距离）。

#### 敌人/召唤物

`GET /api/world/map/enemies`

**用途**：units 过滤 status==2。

**返回格式**：`{ "units": [ ... ] }`

#### 城镇 NPC/佣兵

`GET /api/world/map/interactives`

**用途**：units 过滤 status==1。

**返回格式**：`{ "units": [ ... ] }`

#### 掉落物

`GET /api/world/map/drops`

**用途**：掉落物列表。

**返回格式**：`{ "drops": [] }`

**注意**：数据源未探索，恒返回空数组（⏳ 占位）。

#### 全图瓦片矩阵

`GET /api/world/map/tiles`

**用途**：获取当前地图瓦片矩阵（寻路/切图判定用）。

**返回格式**：`{ "map_id": 0, "width": 64, "height": 64, "tiles": [ { "tx":0, "ty":0, "blocked":true, "exit":false }, ... ] }`

**注意**：v0.4.63 起优先使用静态瓦片（assets maps/tiles.json），缺失时回退运行时读 [0x2f3000+0xf48]。

#### 寻路距离

`GET /api/world/map/distance?tx=600&ty=400`

**用途**：玩家→目标的 BFS 距离（只计算不移动）。

**返回格式**：
```json
{ "target": { "x": 600, "y": 400 }, "start": { "x": 40, "y": 168 },
  "in_map": true, "found": true, "distance": 72, "nearest": null }
```

### 3.2 移动操作 movement

#### 寻路移动

`POST /api/world/movement/move`

**用途**：寻路移动玩家到目标像素坐标（自研 BFS 导航，后台线程逐帧移动）。

**请求格式**：
```json
{ "x": 600, "y": 400 }
```

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：
- 目标不可达时走到最近可达点并转身面向目标（face-target，v0.4.30）
- 单位/墙阻挡自动绕行；到达出口 tile 自动切图
- 剧情/切图状态（GAMESTATE_nState!=0）时操作自终止（v0.4.37）

#### 寻路计算

`POST /api/world/movement/path`

**用途**：只计算 BFS 路径，不实际移动。

**请求格式**：`{ "tx": 600, "ty": 400 }`

**返回格式**：`<Path 模型>`

#### 持续移动

`POST /api/world/movement/walk`

**用途**：方向键持续移动（后台线程每帧 CHAR_Move）。

**请求格式**：`{ "direction": 1 }`（0=下 1=左 2=上 3=右）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：撞墙即停；direction 越界返回 `direction 0-3 required`。

#### 停止移动

`POST /api/world/movement/stop`

**用途**：停止所有移动（停后台线程+清路径）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

#### 场景交互

`POST /api/world/movement/interact`

**用途**：场景交互/攻击键（复现官方 EVTSYSTEM_DoCheckAllEvent(2) 事件触发链）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：v0.4.41；与 dialog/interact 等价（内部同链）。

### 3.3 静态地图 maps

#### 地图列表

`GET /api/world/maps/list`

**用途**：获取地图列表（id+名称）。

**返回格式**：`{ "maps": [ { "map_id": 3513, "name": "黑暗骑士团营地" }, ... ] }`

#### 指定地图

`GET /api/world/maps/{map_id}`

**用途**：获取指定地图静态信息。

**返回格式**：
```json
{ "map_id": 30, "text_id": 3513, "name": "影子丛林1", "raw": { "u16": [...], "text_0": "..." } }
```

**注意**：0-415 按下标查询（text_id 为对应 text_id）；其他值按 text_id 反向匹配（兼容旧语义，v0.4.28）。

---

## 四、item（物品与背包）— GET/POST /api/item/*

**物品实体全生命周期**：背包读写（inventory）、商店交易（shop）。物品从获得到消耗/处置全在一个组。

### 4.1 背包读 inventory

#### 背包复合

`GET /api/item/inventory`

**用途**：获取背包完整信息（6 袋 × 16 槽，含名称注入）。

**返回格式**：`<Inventory 模型>`

#### 金币

`GET /api/item/inventory/money`

**用途**：获取当前金币。

**返回格式**：`{ "money": 72503 }`

#### 全部物品展平

`GET /api/item/inventory/items`

**用途**：获取全部物品展平列表（每项附 bag 字段）。

**返回格式**：`{ "items": [ { "bag": 0, "slot": 3, "category": 1, "count": 1, "name": "治疗药水" }, ... ] }`

#### 袋信息

`GET /api/item/inventory/bag/{bag}/info`

**用途**：获取指定袋（0..5）信息（容量/占用）。

**返回格式**：`{ "bag": 0, "capacity": 16, "slot_count": 13 }`

#### 袋内指定槽

`GET /api/item/inventory/bag/{bag}/{slot}`

**用途**：获取指定袋内指定槽物品。

**返回格式**：`<Item 对象>`（含 name）

### 4.2 背包操作 inventory

#### 使用物品

`POST /api/item/inventory/use-item`

**用途**：使用指定背包物品（药水/卷轴/开箱/解封等）。

**请求格式**：`{ "bag": 0, "slot": 3 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：骰子→掷骰预览返回 `base/pending/delta`（不应用，v0.4.22）；非消耗品返回 `item not usable`。

#### 接受掷骰

`POST /api/item/inventory/dice-accept`

**用途**：接受未确认的掷骰结果（应用 pending 到基础属性）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：无未确认结果→`no dice result pending`。

#### 拒绝掷骰

`POST /api/item/inventory/dice-reject`

**用途**：拒绝掷骰结果（仅清 flag，骰子不退回）。

**请求格式**：无 body

**返回格式**：`{"ok":true}`

**注意**：无未确认结果→`no dice result pending`。

#### 丢弃物品

`POST /api/item/inventory/discard`

**用途**：丢弃指定背包物品。

**请求格式**：`{ "bag": 0, "slot": 5 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：按槽位清空判定成功（v0.3.2）。

#### 移动/整理物品

`POST /api/item/inventory/move`

**用途**：移动物品或堆叠合并（INVEN_MoveItem）。

**请求格式**：`{ "bag": 0, "slot": 3, "count": 1, "to_bag": 0, "to_slot": 4 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：源空槽→`slot empty`；count≤0→参数错；同槽→`same slot`；目标越界→`bad target`。

#### 出售物品

`POST /api/item/inventory/sell`

**用途**：出售指定背包物品（价格=ITEM_GetPrice 静态表）。

**请求格式**：`{ "bag": 0, "slot": 5 }`

**返回格式**：`{"ok":true,"price":15,"state":<Inventory 模型>}`

**注意**：空槽→`slot empty`；价格由静态表决定（防刷钱）。

#### 穿装备

`POST /api/item/inventory/{role}/equip`

**用途**：指定出战槽穿上装备（背包位置或类别）。

**请求格式**（二选一）：
```json
{ "bag": 0, "slot": 3 }
```
或
```json
{ "category": 512 }
```

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：目标槽占用自动替换（先卸后穿，v0.3.3）。

#### 脱装备

`POST /api/item/inventory/{role}/unequip`

**用途**：指定出战槽脱下装备。

**请求格式**：`{ "slot": 2 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：空槽返回 `unequip failed`。

#### 镶嵌宝石

`POST /api/item/inventory/{role}/jewel`

**用途**：把背包宝石镶嵌到指定装备槽（ITEMSYSTEM_PutJewel）。

**请求格式**：`{ "bag": 0, "slot": 3, "equip_slot": 3 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：无孔→`no socket`；非宝石→`not jewel`；空装备槽→`equip slot empty`；**镶嵌后自动消耗背包宝石（防刷）**。

### 4.3 商店 shop

#### 商店商品列表

`GET /api/item/shop/items`

**用途**：获取当前商店商品列表。

**返回格式**：`{ "items": [ { "slot": 0, "category": 5, "count": 1, "price": 15, "name": "恢复药水" }, ... ] }`

**注意**：price = ITEM_GetBuyPrice（v0.4.14）。

#### 购买商品

`POST /api/item/shop/buy`

**用途**：购买当前商店指定商品（绕过 cursor：DEALSYSTEM 表定位 + ITEM_GetBuyPrice + INVEN_SaveItem + MinusMoney）。

**请求格式**：`{ "slot": 0 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：金币不足→`not enough money`；无商品→`item not found`。

---

## 五、quest（任务）— GET/POST /api/quest/*

**任务链路**：接受→进度→交付→放弃，读写围绕同一套任务槽数据（QUESTSYSTEM）。

#### 任务复合

`GET /api/quest`

**用途**：获取任务信息复合（active/list/completed）。

**返回格式**：`{ "active": 0, "list": [], "completed": [] }`

**注意**：list/completed 为占位空数组。

#### 当前激活任务

`GET /api/quest/active`

**用途**：获取当前激活任务 ID。

**返回格式**：`{ "active_quest": 381 }`

#### 已接受任务列表

`GET /api/quest/list`

**用途**：获取已接受任务列表。

**返回格式**：`{ "quests": [ { "slot": 0, "quest_id": 381 } ] }`（QUESTSYSTEM 槽数组 12B/槽，v0.4.39）

#### 任务详情

`GET /api/quest/list/{id}`

**用途**：获取任务详情。

**返回格式**：`{"error":"not found"}`

**注意**：数据源未逆向，恒占位（⏳）。

#### 已完成任务

`GET /api/quest/completed`

**用途**：获取已完成任务列表。

**返回格式**：`{ "quests": [] }`

**注意**：数据源未逆向，恒占位空（⏳）。

#### 放弃任务

`POST /api/quest/quit`

**用途**：放弃指定任务（QUESTSYSTEM_Find + RemoveSlot）。

**请求格式**：`{ "quest_id": 381 }`

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：无任务→`quest not found`。

---

## 六、ui（界面与对话）— GET/POST /api/ui/*

**界面交互**：界面状态感知（ui）、对话交互（dialog）。对话是界面状态的一种（screen=story/npc/popup/wipeout），操作前置检查统一看 `dialog_active`。

### 6.1 界面状态 ui

#### 界面状态复合

`GET /api/ui`

**用途**：获取界面状态复合（screen/dialog_active/dialog）。

**返回格式**：`<GameState 模型>`

#### 当前界面

`GET /api/ui/screen`

**用途**：获取当前界面名。

**返回格式**：`{ "screen": "world" }`

#### 当前面板

`GET /api/ui/panel`

**用途**：获取当前面板（screen 为面板时）。

**返回格式**：`{ "panel": "inventory" }` 或 `{ "panel": null }`

#### 弹窗复合

`GET /api/ui/dialog`

**用途**：获取弹窗状态复合（active/dialog）。

**返回格式**：`{ "active": true, "dialog": { "text": "...", "has_ok": true, "has_cancel": false } }`

#### 弹窗单字段

`GET /api/ui/dialog/active|text|buttons|ok|cancel`

**用途**：获取弹窗各字段。

**返回格式**：
- `active`：`{ "active": true }`
- `text`：`{ "text": "是否出售？" }`
- `buttons`：`{ "buttons": ["是", "否"] }`
- `ok`：`{ "has_ok": true }`
- `cancel`：`{ "has_cancel": false }`

#### 回到主菜单

`POST /api/ui/main-menu`

**用途**：从世界回到主菜单（GAMESTATE_SetState(4) 正规状态切换）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：非 world→`not in game`；纯 API 切换无崩溃（v0.4.17）。

#### 打开面板

`POST /api/ui/panel/open`

**用途**：打开指定面板。

**请求格式**：`{ "panel": "inventory" }`

**返回格式**：`{"ok":true,"state":<GameState 模型>}`

**注意**：⛔ 卡点（v0.4.5）：依赖 popup 节点结构逆向（POPUPSTATE_Create+Push+场景回调），待探索。

#### 关闭面板

`POST /api/ui/panel/close`

**用途**：关闭当前面板。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<GameState 模型>}`

**注意**：⛔ 卡点（v0.4.5 已撤销）：POPUPSTATE_Pop 在 settings 场景 SIGSEGV（popup 栈状态机对 pop 顺序敏感），暂不使用。

### 6.2 对话 dialog

#### 开始交互

`POST /api/ui/dialog/interact`

**用途**：开始与附近 NPC 对话（PLAYER_DoCheckNearNPC + UINpc_InitNPC）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：无 NPC 附近→`no npc nearby`；切图触发的剧情对话无需 interact（自动激活）。

#### 对话内容

`GET /api/ui/dialog/content`

**用途**：获取当前对话内容与选项（统一五态，v0.4.27）。

**返回格式**：`<DialogContent 模型>`（story/npc/popup/wipeout/none）

**注意**：配合 `POST /api/ui/dialog/select` 使用；wipeout 态选项走死亡面板按钮（v0.4.35）。

#### 选择对话选项

`POST /api/ui/dialog/select`

**用途**：选择对话选项（剧情推进/跳过/弹窗确认/选项分支/死亡面板按钮）。

**请求格式**：
```json
{ "action": "next", "index": 0 }
```

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：
- `action` 必须匹配当前对话态（五态白名单，v0.4.39）：
  - story→`next`/`skip`；popup→`ok`/`cancel`；wipeout→`revive`/`special_revive`/`game_over`；npc→`next`/index
- 缺参→`action or index required`；索引越界→`bad index`；无对话→`no dialog`

#### 弹窗确定

`POST /api/ui/dialog/ok`

**用途**：点击弹窗确定按钮（执行确认动作，如出售/销毁）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<GameState 模型>}`

**注意**：非弹窗→`no dialog`。

#### 弹窗取消

`POST /api/ui/dialog/cancel`

**用途**：点击弹窗取消按钮。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<GameState 模型>}`

**注意**：非弹窗→`no dialog`；无取消按钮时仅关闭弹窗（Free 路径）。

---

## 七、system（系统与会话）— GET/POST /api/system/*

**会话/进程级能力**：服务健康、游戏整体快照、事件流通知、存档（会话持久化）、静态知识库查询。与具体游戏系统解耦。

### 7.1 服务健康 health

`GET /api/system/health`

**用途**：模块服务存活检查 + 版本/游戏状态探测。

**返回格式**：
```json
{ "ok": true, "version": "0.5.0", "game": "main_menu", "base": 532410707968 }
```

**注意**：
- `game` 取值同 GameState 的 `screen`
- `base` 是 libgame.so 基址（0 表示未解析）

### 7.2 游戏整体 game

#### 游戏复合

`GET /api/system/game`

**用途**：获取游戏整体信息（snapshot+info）。

**返回格式**：`{ "snapshot": <Snapshot 模型>, "info": <info 对象> }`

#### 全量快照

`GET /api/system/game/snapshot`

**用途**：获取局内全量快照。

**返回格式**：`<Snapshot 模型>`（含 name 注入）

#### 帧计数

`GET /api/system/game/frame`

**用途**：获取游戏帧计数（每帧 +1）。

**返回格式**：`{ "frame": 12345 }`

**注意**：源 [0x2f5648] GOT 槽 u64（v0.4.26）。

#### 软件信息

`GET /api/system/game/info`

**用途**：获取模块/软件信息。

**返回格式**：`{ "version": "0.5.0", "logged_in": null, "save_slots": [], "package_name": "com.com2us...", "base": 532410707968 }`

**注意**：logged_in/save_slots 为占位。

### 7.3 事件流 events

`GET /api/system/events`

**用途**：轮询获取游戏事件（差异检测，零 hook）。

**返回格式**：
```json
{ "events": [
  { "type": "money", "old": 100, "new": 150 },
  { "type": "hp", "role": 0, "old": 10598, "new": 8000 },
  { "type": "level_up", "role": 1, "old": 10, "new": 11 },
  { "type": "inventory", "old": 13, "new": 12 }
] }
```

**注意**：
- 事件类型：`money`/`hp`/`mp`/`exp`/`level_up`/`move`/`inventory`
- 需周期性轮询（500ms-1s）；首次调用仅建立基线返回空列表（v0.3.0）

### 7.4 存档 save

#### 存档槽信息

`GET /api/system/save/slots`

**用途**：获取 3 个存档槽的存在状态与主控等级。

**返回格式**：`{ "slots": [ { "slot": 0, "exists": true, "hero_level": 27 }, ... ] }`

**注意**：读槽区 b2 存在标志 + SAVESLOT_GetHero 主控等级（v0.4.18）。

#### 手动存档

`POST /api/system/save/save`

**用途**：手动保存当前游戏（SAVE_Save 无参静默保存：SV 校验→全量序列化）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：非 world→`not in game`。

#### 进入存档槽

`POST /api/system/save/enter-slot`

**用途**：直接进入指定存档槽（复现 SaveSlot_SlotButtonExe 链）。

**请求格式**：`{ "slot": 0 }`（0/1/2）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：
- 非 world 才可调（world 中→`already in game`）；slot 越界→`bad slot`；空槽→`slot empty`
- ⚠️ **存档不存在时调用会崩溃**——先查 `/api/system/save/slots` 确认 exists=true
- 付费弹窗已 hook 阻断（v0.4.18，游戏直接进 world）

#### 创建新存档

`POST /api/system/save/create`

**用途**：创建新角色存档并自动进初始营地（复现 SaveSlot_GoToNewGame + SelectCharacter_ButtonStartExe 链）。

**请求格式**：`{ "slot": 1, "class_idx": 2 }`（slot 0/1/2；class_idx 0-5）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：
- 创建后自动进初始营地（map_id=0）+ 剧情对话激活（dialog/content type=story，可 skip）
- 职业映射：0=黑暗骑士 1=忍者 2=黑魔导法师 3=祭司 4=暗影射手 5=狂战士
- 新档未保存前槽区 exists=false，SAVE_Save 后落盘

#### 读档

`POST /api/system/save/load`

**用途**：读档。

**请求格式**：`{ "slot": N }`

**返回格式**：`{"ok":false,"error":"not implemented"}`

**注意**：⛔ 未实现（仅主菜单/选档界面可用，GAMELOADER 状态限制，P3 暂缓）。

### 7.5 静态数据表 tables

#### 静态表列表

`GET /api/system/tables`

**用途**：获取可用静态表列表。

**返回格式**：`{ "tables": [ "ITEMDATABASE", "MONDATABASE", ... ] }`（来自 manifest.json）

#### 指定静态表

`GET /api/system/tables/{table}`

**用途**：获取任意内嵌静态表全量数据（表名大写，如 `ITEMDATABASE`）。

**返回格式**：`{ "records": [ <表记录>... ] }`

**注意**：表名自动转大写；不存在的表返回 `{"error":"not found"}`。模块内仅 28 表子集，其余返回 404。

#### 表内搜索

`GET /api/system/tables/{table}/search?q=治疗`

**用途**：表内名称模糊搜索。

**返回格式**：`{ "items": [ { "index": 788, "name": "鑫迪的治疗药", "raw": { "u16": [818, ...], "text_0": "鑫迪的治疗药" } } ] }`

**注意**：参数 `q` 必填；搜索字段为表记录名称字段（text_0）。

### 7.6 多语言文本 text

`GET /api/system/text?lang=zh-Hans`

**用途**：获取指定语言的全部文本。

**返回格式**：`<text/{lang}.json 内容>`

**注意**：参数 `lang` 必填；支持 `zh-Hans`/`en`。

### 7.7 剧情事件 story-events

`GET /api/system/story-events`

**用途**：获取剧情事件数据（命令/条件/文本）。

**返回格式**：`<events.json 内容>`（来自 assets reverse/events.json）

---

## 八、op（越权操作）— POST /api/op/*

游戏内做不到的操作（改数据/强行操作）。**权限独立**：需全局开关（默认关闭）+ 独立鉴权；OP 端点与合法操作物理隔离。

### 8.1 已实现

#### 设置血量

`POST /api/op/character/{role}/hp`

**请求格式**：`{ "hp": 8000 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：截断到 0..max_hp。

#### 设置魔力

`POST /api/op/character/{role}/mp`

**请求格式**：`{ "mp": 200 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：截断到 0..max_mp。

#### 设置经验

`POST /api/op/character/{role}/experience`

**请求格式**：`{ "exp": 12000 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：CHAR_SetExperience 直接写经验，不触发升级结算。

#### 设置等级

`POST /api/op/character/{role}/level`

**请求格式**：`{ "level": 30 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：CHAR_SetLevel 完整升级结算（写等级+重算 nextExp+InitializeFromLevel+升级加点+回满血蓝）；降级→`level down not allowed`。

#### 设置基础属性

`POST /api/op/character/{role}/attr/{index}`

**请求格式**：`{ "value": 100 }`（index 0-4）

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：CHAR_SetStatBase 直写基础属性（0..255）；总属性 = 基础+分配+加成+动态。

#### 添加物品

`POST /api/op/inventory/add`

**请求格式**：`{ "category": 1, "count": 5 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：ITEMSYSTEM_CreateItem + INVEN_SaveItem；可堆叠上限 99；背包满→`inventory full`。

### 8.2 未实现

以下端点结构已定稿，待开发（需权限获取机制 + 端点组）：

| 路径 | 用途 | body |
|---|---|---|
| `POST /api/op/quest/accept` | 接取任务（绕过 NPC） | `{"quest_id","force":false}` |
| `POST /api/op/quest/complete` | 完成任务（无视条件） | `{"quest_id"}` |
| `POST /api/op/character/{role}/status-point` | 设置属性点 | `{"points":N}` |
| `POST /api/op/character/{role}/skill-point` | 设置技能点 | `{"points":N}` |
| `POST /api/op/character/{role}/skill-level` | 设置技能等级 | `{"action_id","level":N}` |
| `POST /api/op/party/swap` | 队伍换位 | `{"a","b"}` |
| `POST /api/op/inventory/set-slot` | 格子设物品+数量 | `{"bag","slot","itemId","count"}` |
| `POST /api/op/inventory/set-equip` | 修改装备属性 | `{"bag","slot"}`+属性参数 |
| `POST /api/op/inventory/money` | 修改金币 | `{"set":N}` 或 `{"add":N}` |
| `POST /api/op/craft/mix-direct` | 免配方机直合成 | `{"recipeId","resultCount"}` |
| `POST /api/op/combat/{role}/heal` | 回血回蓝 | `{"hp","mp"}` |
| `POST /api/op/combat/{role}/rest` | 休息恢复 | — |
| `POST /api/op/combat/{role}/revive` | 复活 | — |
| `POST /api/op/combat/{role}/hate` | 仇恨操作 | `{"targetId","value"}` |
| `POST /api/op/movement/teleport` | 传送/切图 | `{"x","y"}` 或 `{"map_id","x","y","force":false}` |

---

## 九、debug（调试）

`GET /api/debug/ui`

**用途**：获取调试用 UI 状态原始 JSON。

**返回格式**：`<原始 gamestate JSON>`（含全部 native 字段）

**注意**：DebugController，不走 ControllerGuard（native 未就绪时直接返回原始数据）。

---

## 十、部署形态差异

| 项 | 手机版（LSPosed 模块） | 服务器版（LSPatch 集成） |
|---|---|---|
| 数据获取 | 同进程 Hook | 同进程 Hook（LSPatch 注入） |
| API 访问 | 监听 0.0.0.0，局域网 Wi-Fi IP | Waydroid NAT，需端口转发 |
| minSdk | Android 11+（Zygisk-LSPosed） | 随游戏 targetSdk 29 |
| 静态数据 | JSON 库随模块打包 | JSON 库随集成 APK 打包 |

---

## 十一、迁移对照表（v0.4.65 → v0.5.0）

> v0.5.0 起 API 按 7 域分组，旧前缀 `/api/info/*`、`/api/data/*`、`/api/action/*` 全部废弃。
> 下表为逐端点新旧路径对照（调用方迁移蓝本）。

### character 域

| 旧路径（v0.4.65） | 新路径（v0.5.0） |
|---|---|
| `GET /api/info/party` | `GET /api/character/party` |
| `GET /api/info/party/count` | `GET /api/character/party/count` |
| `GET /api/info/party/leader` | `GET /api/character/party/leader` |
| `GET /api/info/party/{slot}` | `GET /api/character/party/{slot}` |
| `GET /api/info/party/{slot}/id` | `GET /api/character/party/{slot}/id` |
| `GET /api/info/party/{slot}/name` | `GET /api/character/party/{slot}/name` |
| `GET /api/info/party/{slot}/level` | `GET /api/character/party/{slot}/level` |
| `GET /api/info/party/{slot}/exp` | `GET /api/character/party/{slot}/exp` |
| `GET /api/info/party/{slot}/hp` | `GET /api/character/party/{slot}/hp` |
| `GET /api/info/party/{slot}/mp` | `GET /api/character/party/{slot}/mp` |
| `GET /api/info/party/{slot}/stats` | `GET /api/character/party/{slot}/stats` |
| `GET /api/info/party/{slot}/stats/{attr}` | `GET /api/character/party/{slot}/stats/{attr}` |
| `GET /api/info/party/{slot}/equipment` | `GET /api/character/party/{slot}/equipment` |
| `GET /api/info/party/{slot}/equipment/{equip_slot}` | `GET /api/character/party/{slot}/equipment/{equip_slot}` |
| `GET /api/info/party/{slot}/skills` | `GET /api/character/party/{slot}/skills` |
| `GET /api/info/party/{slot}/skills/list` | `GET /api/character/party/{slot}/skills/list` |
| `GET /api/info/mercenary` | `GET /api/character/mercenary` |
| `GET /api/info/mercenary/list` | `GET /api/character/mercenary/list` |
| `GET /api/info/mercenary/{slot}` | `GET /api/character/mercenary/{slot}` |
| `POST /api/action/combat/{role}/config/auto-attack` | `POST /api/character/combat/{role}/config/auto-attack` |
| `POST /api/action/combat/{role}/config/skill-usage` | `POST /api/character/combat/{role}/config/skill-usage` |
| `POST /api/action/combat/{role}/switch` | `POST /api/character/combat/{role}/switch` |
| `POST /api/action/combat/{role}/cast` | `POST /api/character/combat/{role}/cast` |
| `POST /api/action/combat/{role}/attack` | `POST /api/character/combat/{role}/attack` |
| `POST /api/action/combat/{role}/stop` | `POST /api/character/combat/{role}/stop` |
| `POST /api/action/character/skill` | `POST /api/character/grow/skill` |
| `POST /api/action/character/{role}/stat` | `POST /api/character/grow/{role}/stat` |
| `POST /api/action/character/{role}/stat-reset` | `POST /api/character/grow/{role}/stat-reset` |
| `POST /api/action/character/{role}/skill-reset` | `POST /api/character/grow/{role}/skill-reset` |
| `POST /api/action/party/include` | `POST /api/character/party/include` |
| `POST /api/action/party/exclude` | `POST /api/character/party/exclude` |
| `POST /api/action/party/discharge` | `POST /api/character/party/discharge` |
| `POST /api/action/party/withdraw` | `POST /api/character/party/withdraw` |

### world 域

| 旧路径（v0.4.65） | 新路径（v0.5.0） |
|---|---|
| `GET /api/info/map` | `GET /api/world/map` |
| `GET /api/info/map/id` | `GET /api/world/map/id` |
| `GET /api/info/map/tile` | `GET /api/world/map/tile` |
| `GET /api/info/map/exits` | `GET /api/world/map/exits` |
| `GET /api/info/map/units` | `GET /api/world/map/units` |
| `GET /api/info/map/enemies` | `GET /api/world/map/enemies` |
| `GET /api/info/map/interactives` | `GET /api/world/map/interactives` |
| `GET /api/info/map/drops` | `GET /api/world/map/drops` |
| `GET /api/info/map/tiles` | `GET /api/world/map/tiles` |
| `GET /api/info/map/distance?tx=&ty=` | `GET /api/world/map/distance?tx=&ty=` |
| `POST /api/action/movement/move` | `POST /api/world/movement/move` |
| `POST /api/action/movement/path` | `POST /api/world/movement/path` |
| `POST /api/action/movement/walk` | `POST /api/world/movement/walk` |
| `POST /api/action/movement/stop` | `POST /api/world/movement/stop` |
| `POST /api/action/movement/interact` | `POST /api/world/movement/interact` |
| `GET /api/data/map/list` | `GET /api/world/maps/list` |
| `GET /api/data/map/{map_id}` | `GET /api/world/maps/{map_id}` |

### item 域

| 旧路径（v0.4.65） | 新路径（v0.5.0） |
|---|---|
| `GET /api/info/inventory` | `GET /api/item/inventory` |
| `GET /api/info/inventory/money` | `GET /api/item/inventory/money` |
| `GET /api/info/inventory/items` | `GET /api/item/inventory/items` |
| `GET /api/info/inventory/bag/{bag}/info` | `GET /api/item/inventory/bag/{bag}/info` |
| `GET /api/info/inventory/bag/{bag}/{slot}` | `GET /api/item/inventory/bag/{bag}/{slot}` |
| `POST /api/action/inventory/use-item` | `POST /api/item/inventory/use-item` |
| `POST /api/action/inventory/dice-accept` | `POST /api/item/inventory/dice-accept` |
| `POST /api/action/inventory/dice-reject` | `POST /api/item/inventory/dice-reject` |
| `POST /api/action/inventory/discard` | `POST /api/item/inventory/discard` |
| `POST /api/action/inventory/sell` | `POST /api/item/inventory/sell` |
| `POST /api/action/inventory/move` | `POST /api/item/inventory/move` |
| `POST /api/action/inventory/{role}/equip` | `POST /api/item/inventory/{role}/equip` |
| `POST /api/action/inventory/{role}/unequip` | `POST /api/item/inventory/{role}/unequip` |
| `POST /api/action/inventory/{role}/jewel` | `POST /api/item/inventory/{role}/jewel` |
| `GET /api/info/shop/items` | `GET /api/item/shop/items` |
| `POST /api/action/shop/buy` | `POST /api/item/shop/buy` |

### quest 域

| 旧路径（v0.4.65） | 新路径（v0.5.0） |
|---|---|
| `GET /api/info/quest` | `GET /api/quest` |
| `GET /api/info/quest/active` | `GET /api/quest/active` |
| `GET /api/info/quest/list` | `GET /api/quest/list` |
| `GET /api/info/quest/list/{id}` | `GET /api/quest/list/{id}` |
| `GET /api/info/quest/completed` | `GET /api/quest/completed` |
| `POST /api/action/quest/quit` | `POST /api/quest/quit` |

### ui 域

| 旧路径（v0.4.65） | 新路径（v0.5.0） |
|---|---|
| `GET /api/info/ui` | `GET /api/ui` |
| `GET /api/info/ui/screen` | `GET /api/ui/screen` |
| `GET /api/info/ui/panel` | `GET /api/ui/panel` |
| `GET /api/info/ui/dialog` | `GET /api/ui/dialog` |
| `GET /api/info/ui/dialog/active|text|buttons|ok|cancel` | `GET /api/ui/dialog/active|text|buttons|ok|cancel` |
| `POST /api/action/ui/dialog/ok` | `POST /api/ui/dialog/ok` |
| `POST /api/action/ui/dialog/cancel` | `POST /api/ui/dialog/cancel` |
| `POST /api/action/ui/main-menu` | `POST /api/ui/main-menu` |
| `POST /api/action/ui/panel/open` | `POST /api/ui/panel/open` |
| `POST /api/action/ui/panel/close` | `POST /api/ui/panel/close` |
| `POST /api/action/dialog/interact` | `POST /api/ui/dialog/interact` |
| `GET /api/info/dialog/content` | `GET /api/ui/dialog/content` |
| `POST /api/action/dialog/select` | `POST /api/ui/dialog/select` |

### system 域

| 旧路径（v0.4.65） | 新路径（v0.5.0） |
|---|---|
| `GET /api/health` | `GET /api/system/health` |
| `GET /api/info/game` | `GET /api/system/game` |
| `GET /api/info/game/snapshot` | `GET /api/system/game/snapshot` |
| `GET /api/info/game/frame` | `GET /api/system/game/frame` |
| `GET /api/info/game/info` | `GET /api/system/game/info` |
| `GET /api/info/events` | `GET /api/system/events` |
| `GET /api/info/save/slots` | `GET /api/system/save/slots` |
| `POST /api/action/save/save` | `POST /api/system/save/save` |
| `POST /api/action/save/enter-slot` | `POST /api/system/save/enter-slot` |
| `POST /api/action/save/create` | `POST /api/system/save/create` |
| `POST /api/action/save/load` | `POST /api/system/save/load` |
| `GET /api/data/list` | `GET /api/system/tables` |
| `GET /api/data/{table}` | `GET /api/system/tables/{table}` |
| `GET /api/data/{table}/search?q=` | `GET /api/system/tables/{table}/search?q=` |
| `GET /api/data/text?lang=` | `GET /api/system/text?lang=` |
| `GET /api/data/events` | `GET /api/system/story-events` |

### 不变路径

| 路径 | 说明 |
|---|---|
| `POST /api/op/*` | 全部 OP 端点（character/inventory/quest/party/craft/combat/movement） |
| `GET /api/debug/ui` | 调试端点 |

---

## 十二、版本历史

- **v0.5.0**（2026-08-13）：**API 按 7 域分组重构**——废弃 `/api/info/*`、`/api/data/*`、`/api/action/*` 三层前缀，改为按实体领域分组（character/world/item/quest/ui/system/op），GET/POST 由 HTTP 方法区分；op 独立保留；同步更新全部 controller 路由（迁移对照见第十一节）
- **v0.4.65**（2026-08-13）：文档按真实代码重写为统一格式（路径/用途/请求/返回/注意）
- **v0.4.64**（2026-08-12）：create-slot 创建新档端点（全职业真机验证）
- **v0.4.63**：静态瓦片数据源（tiles.json）
- **v0.4.62**：backlog（切图/剧情）
- **v0.3.13**（2026-08-08）：端点全量分层重构（info/action/op）
- **v0.3.0**：操作端点/事件流实现
