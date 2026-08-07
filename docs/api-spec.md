# API 信息清单与接口规格

> 状态：v0.3，基于需求确认（2026-08-05）。静态数据（M3 ✅）+ 运行时只读端点（M4 ✅ 真机验证 v0.2.15→v0.2.34）
> + 操作端点/事件流（v0.3.0 ✅ 实现，**v0.3.2-0.3.6 真机验证修复**）；未实现项见 §4 状态标注与 §7。hook 点详见 `docs/data-sources.md`。

## 0. API 分层设计（目标结构，2026-08-08 用户确认）

> **本节的完整分层是 API 整体开发实现的参考**（重构目标）。当前实现仍为旧分层（§3-§7），
> 按 backlog P0「分拆端点代码」重构后落地本节结构。

### 0.1 分层规则

- `/api` 顶层（全部 API）
- **1 层：类别**——`info`（获取，运行时动态）/ `action`（操作）/ `data`（静态数据）
- **2 层：聚合系统**（如 party、inventory、current-map）
- **3 层+：系统部分层层拆解**（复杂系统继续拆）
- **最后一层 = 简单 API**（单一功能：地图 ID、角色血量、小队数量）
- **倒数第二层 = 复合 API**（组合简单 API：地图基本信息、角色信息）

### 0.2 info 系统结构

```
/api/info/
├── /current-map/                  ← 复合（当前地图，动态）
│   ├── id / tile / units / enemies / interactives / drops     ← 简单
├── /party/                        ← 复合（出战角色，3 槽）
│   ├── count / leader             ← 简单
│   └── /{1..3}                    ← 复合（指定出战槽）
│       ├── id / name / level / exp / hp / mp                  ← 简单
│       ├── stats / stats/{attr}   ← 复合 / 简单
│       ├── equipment / equipment/{slot}                       ← 复合 / 简单
│       └── skills / skills/list  ← 复合 / 简单
├── /mercenary/                    ← 复合（待命佣兵，18 槽）
│   ├── list                       ← 简单（非空槽 id 列表）
│   └── /{1..18}                   ← 复合（结构同 party 槽）
├── /inventory/                    ← 复合（背包）
│   ├── money / items              ← 简单
│   └── /bag/{i}/info / /bag/{i}/{slot}   ← 简单
├── /quest/                        ← 复合（任务）
│   ├── active                     ← 简单（当前任务 id）
│   ├── list / list/{id}           ← 简单（已接受列表 / 详情：要求+进度）
│   └── completed                  ← 简单（已完成列表）
├── /ui/                           ← 复合（界面状态）
│   ├── screen / panel             ← 简单
│   └── /dialog/                   ← 复合（弹窗）
│       ├── active / text / buttons / ok / cancel              ← 简单
├── /game/                         ← 复合（游戏整体）
│   ├── snapshot                   ← 复合（局内全量快照：ui/map/inventory/party/quest/time）
│   └── info                       ← 复合（局外软件信息：version/loggedIn/saveSlots/packageName/paths）
└── /events?since={ts}             ← 简单（事件流：后台采样+缓冲+增量，采样间隔实现时定）
```

### 0.3 data 系统结构

```
/api/data/
├── /map/                          ← 复合（静态地图）
│   ├── list                       ← 简单（地图列表：id+名称）
│   └── /{mapId}                   ← 简单（指定地图静态信息：尺寸/瓦片布局/传送点）
├── /list                          ← 简单（可用静态表列表）
├── /{table}                       ← 简单（静态表：物品/技能/怪物/任务...按表名）
└── /{table}/search?q=             ← 简单（表内搜索：名称模糊 + 可选属性过滤）
```

### 0.4 action 系统结构（待规划）

> 所有操作相关 API 归 `/api/action/*`。内部结构（按哪个系统分/命名）**待规划**
> （信息端点考虑完后回头处理）。已确认：`/api/action/get-path`（寻路，写角色 PATHLIST）。

### 0.5 顶层

```
/api/health                        ← 服务健康（ok/version/game/base）
```

### 0.6 旧系统归位

| 旧系统/端点 | 新归属 |
|---|---|
| /api/info/player（money/mapId/position/activeQuest/mainMercenarySlot/partyCount） | ✅ 已拆：inventory/money、current-map/id、party/leader/position、quest/active、party/leader、party/count |
| /api/info/gamestate、/api/info/ui（旧） | ✅ 已归：ui/screen + ui/panel + ui/dialog |
| /api/info/dialog | ✅ 已归：ui/dialog |
| /api/info/units | ✅ 已归：current-map/units |
| /api/info/snapshot | ✅ 已归：game/snapshot |
| /api/info/path | ✅ 已迁：action/get-path |

> 重构状态：**v0.3.13 全部落地**（旧端点已移除，新分层端点真机验证通过）。

## 1. 概述

模块注入游戏进程（libxposed 101），通过 **native 层 `/proc/self/maps` 基址 + 符号 VMA 直读** libgame.so
的全局变量与 Getter 函数获取运行时数据（**零 hook，不用 dlopen/dlsym**），以 REST + JSON 对外提供。信息分两类：

| 类别 | 来源 | 提供方式 |
|---|---|---|
| 运行时状态（动态） | native 直读 libgame.so（base+VMA） | 实时读取，每次请求取当前值 |
| 静态配置数据（静态） | game_res 一次性解析 | JSON 数据库，模块内嵌子集或独立提供 |

## 2. 信息清单（需求确认版）

### 2.1 运行时状态（✅ 已实现，真机验证实时性）

| 信息 | 说明 | 粒度 | 状态 |
|---|---|---|---|
| 金币/金钱 | 当前持有货币（捡金币实时变化） | 全局 | ✅ 实时 |
| 血量 / MP | 生命值与魔法值（当前/上限） | 每角色 | ✅ 实时 |
| 等级 / 经验 | 等级与经验进度 | 每角色 | ✅ 实时 |
| 坐标 / 地图 | 当前地图 ID + 队伍位置 | 全局 | ✅ 实时（角色 +0x02/+0x04；地图 MAP_nBaseInfo+0） |
| 背包 / 装备 | 道具栏与装备槽（捡/卖物品实时变化） | 背包全局 / 装备每角色 | ✅ 实时 |
| 出战队伍 | 出战角色数（`PARTY_GetSize`） | 全局 | ✅ 实时 |
| 角色技能 | 每名角色已装备/已习得技能 | 每角色 | ✅ v0.2.23（+0x2A0 技能链表） |
| 地图单位位置 | 当前地图敌人/NPC/掉落物位置 | 每单位 | ✅ v0.2.19-21（CHARSYSTEM 池） |
| UI 界面状态 | 主菜单/存档选择/游戏中 | 全局 | ✅ v0.2.22（STATE_nState） |
| 全部佣兵 | 上场 + 未上场佣兵（含名称/等级） | 每佣兵 | ✅ v0.2.30-31 |
| 主属性 | 力量/敏捷/体力/智力/精力 + 能力点 | 每角色 | ✅ v0.2.28-29（CHAR_GetStat） |
| 路径计算 | 角色到目标点的 A* 路径 | 请求级 | ✅ v0.2.33-34（CHAR_SearchPath） |

### 2.2 静态配置数据（全量提取 ✅ M3 完成）

| 信息 | 说明 | 数据表（game.dat.jpg 内） |
|---|---|---|
| 角色数值表 | 职业、等级成长、属性 | `CHARCLASSBASE`(6)、`ATTRINITBASE`(22)、`MAXLEVELBASE`(48) |
| 道具 / 装备配置 | 物品 ID、名称、类型、属性、价格 | `ITEMDATABASE`(1018)、`ITEMCLASSBASE`(36)、`ITEMSTATICBASE`、`ITEMDESCBASE`(152)、`ITEMRARITYGRADEBASE`(5)、`ITEMGRADEBASE`(15)、`ITEMENCHANTBASE`(32) |
| 技能表 | 技能 ID、名称、职业限制、消耗、效果 | `SKILLDESCBASE`(114)、`SKILLTRAINBASE`(93)、`SKILLTRAINPOINTBASE` |
| 佣兵表 | 佣兵信息 / 技能 / 群体技能 | `MERCENARYINFOBASE`(47)、`MERCENARYSKILLBASE`(460)、`MERCENARYGROUPSKILLBASE` |
| 地图数据 | 地图 ID、名称、传送点、特征 | `MAPINFOBASE`(416)、`PORTALINFOBASE`、`MAPFEATUREINFOBASE`、`MAPCOLORBASE` |
| 怪物数据 | 怪物数值、AI、技能、掉落 | `MONDATABASE`(553)、`MONAIINFOBASE`、`MONSKILLBASE`(126)、`MONSTERDROPBASE`(160) |
| 任务数据 | 任务、分组、奖励、文本 | `QUESTINFOBASE`(507)、`QUESTGROUPBASE`(260)、`QUESTREWARDBASE`(394) |
| NPC 数据 | NPC 信息 / 描述 | `NPCINFOBASE`(656)、`NPCDESCBASE`(43) |
| 事件数据 | 剧情事件、命令、条件 | `eventdata.dat.jpg`（608 事件 / 28,598 命令）、`EVTINFOBASE`、`EVTCONDBASE`、`EVTCMDBASE` |
| 文本数据 | 6 语言 35,811 条 | `memorytext_*.dat.jpg`（zh-Hans/zh-Hant/ja/en/de/fr） |
| 全量 100 表 | 全部静态表原始记录 | `game.dat.jpg`（`*BASE_nRecordCount/pData` 对应） |

> 解析说明见 `docs/reference/static-data.md`；产物在 `static-data/json/`。

## 3. 数据模型

### Player（玩家，✅ 已实现）

```json
{
  "money": 72503,
  "mapId": 2056,
  "x": 304,
  "y": 376,
  "activeQuest": 0,
  "mainMercenarySlot": 0,
  "partyCount": 2
}
```
字段：`money` 金币（实时）、`mapId` 实时地图 ID（MAP_nBaseInfo+0）、`x/y` 实时玩家坐标（角色 +0x02/+0x04）、
`mainMercenarySlot` 当前控制角色槽（v0.2.16）、`partyCount` 出战人数。

### Role（出战角色，✅ 已实现 /api/info/player/party 返回数组）

```json
{
  "type": 1, "nameId": 2210, "level": 27,
  "hp": 10598, "mp": 200, "maxHp": 10664, "maxMp": 250,
  "exp": 12000, "expNext": 15000,
  "stats": { "0": 60, "1": 40, ..., "30": 10664, "31": 212 },
  "mainStats": [96, 139, 101, 54, 38],
  "statusPoint": 78,
  "attrs": [
    { "id": 0, "name": "力量", "value": 96 },
    { "id": -1, "name": "能力点", "value": 78 },
    { "id": 30, "name": "HP上限", "value": 10664 }
  ],
  "equipment": [
    { "slot": 0, "typeFlags": 21352, "category": 333, "rarity": 3,
      "damage": 0, "defense": 37, "magicRate": 0, "socket": 69, "enchant": 35072,
      "options": [1, 1, 1, 18, 23, 17, 19, 36], "name": "光荣的火冠" },
    null
  ]
}
```
字段：`type` 角色类型、`nameId` 名称相关 ID（非 text_id；角色名用 CHAR_GetName）、`level/hp/mp/maxHp/maxMp/exp/expNext` 实时、
`stats` 战斗属性数组（0..31）、`mainStats` 主属性（0-4=力量/敏捷/体力/智力/精力，CHAR_GetStat v0.2.29）、
`statusPoint` 剩余能力点（v0.2.29）、`attrs` 带名属性数组（Kotlin 注入 v0.2.28-29）、
`equipment` 10 装备槽（含属性 damage/defense/magicRate/socket/enchant/options 词缀 v0.2.24 + name 联查 v0.2.25）。

### Inventory（背包，✅ 已实现）

```json
{
  "bags": [
    { "bag": 0, "items": [
      { "slot": 3, "typeFlags": 512, "category": 1, "count": 1, "rarity": 0,
        "damage": 0, "defense": 0, "magicRate": 0, "socket": 0, "enchant": 0,
        "options": [], "name": "治疗药水" }
    ], "capacity": 16, "slotCount": 13 }
  ]
}
```
字段：`typeFlags` 类型位域、`category` 类别（UTIL_GetBitValue(flags,15,6) = **ITEMDATABASE itemId**，v0.2.25 实测）、
`count` 数量、`rarity` 稀有度、物品属性（v0.2.24）、`name` 物品名（Kotlin 联查 ITEMDATABASE text_0，v0.2.25-27）、
`capacity` 袋容量 16 格（v0.2.32 修正）、`slotCount` 占用数。

### Skills（角色技能，✅ v0.2.23 /api/info/player/skills）

```json
[
  { "role": 0, "skills": [ { "actionId": 0, "level": 1 }, ... ],
    "unlockBitmap": 65535, "activeSkillId": 0, "skillPoints": 6 },
  null
]
```
来源：角色 +0x2A0 技能链表（节点 action_id/level/next）、+0x2B0 解锁位图、+0x280 当前技能、+0x328 技能点。

### Mercenaries（全部佣兵，✅ v0.2.30-31 /api/info/player/mercenaries）

```json
[
  { "slot": 0, "type": 0, "flags": 219, "inParty": true,
    "name": "凯恩", "level": 27, "x": 320, "y": 480 },
  { "slot": 1, "type": 2, "flags": 77, "inParty": false,
    "name": "西雷斯", "level": 26, "x": 2048, "y": 2048 }
]
```
来源：佣兵槽数组 *(*(0x2f6010))（20B/槽，flags bit0=占用 bit1=在队伍）+ CHARSYSTEM_FindAsMercenarySlot 关联角色
+ CHAR_GetName 名称。未上场佣兵坐标 2048=未激活哨兵。

### Path（寻路，✅ v0.2.33-34 GET /api/info/path?tx=&ty=）

```json
{ "target": { "x": 200, "y": 360 },
  "start": { "x": 320, "y": 472 },
  "found": true,
  "path": [ { "x": 320, "y": 472 }, ... ] }
```
来源：CHAR_SearchPath(hero, tx, ty, 1) 仅计算存储路径（不触发移动），结果存角色 +0x2F0 PATHLIST 链表
（节点网格坐标 ×8 = 像素坐标）。

### GameState（游戏界面状态，✅ v0.3.10 /api/info/gamestate，替代旧 /api/info/ui）

```json
{ "screen": "dialog", "dialogActive": true, "dialog": { "text": "是否出售？", "hasOk": true, "hasCancel": false } }
```
字段：
- `screen`：当前界面（✅ v0.3.9 面板识别经真机验证，基于 g_arrPopupStack 栈顶场景 enter 函数 VMA 匹配）：
  - `"loading"` 初始化 / `"main_menu"` 主菜单 / `"world"` 游戏世界 / `"dialog"` 弹窗激活（popupOn 标志，UIPopupMsg 机制）
  - 面板（popup 栈顶场景）：`"character_info"` 人物属性 / `"inventory"` 背包·装备 / `"skills"` 技能 / `"mercenary"` 佣兵管理 / `"quests"` 任务 / `"settings"` 选项·系统菜单 / `"shop"` 商店 / `"craft"` 合成 / `"npc"`·`"npc_quest"`·`"npc_rest"`·`"npc_revive"` NPC 交互 / `"save_slot"` 存档选择 / `"character_select"` 角色选择 / `"options"` 游戏内选项 / `"shortcut"` 快捷菜单 / `"world_map"` 世界地图 / `"input_count"` 数量输入 / `"choice"` 选择 / `"wipeout"` / `"daily_reward"` 每日奖励 / `"in_app"` 内购 / `"ui_panel"` 其他未匹配面板
- `dialogActive`：是否有阻塞弹窗。**操作前置检查**：调用操作端点前若为 true，操作将被 UI 阻塞
- `dialog`（✅ v0.3.10，仅 dialogActive=true 时出现）：弹窗信息（纯读内存，真机验证）：
  - `text`：弹窗内容文本（UTF-8，UIPopupMsg_pText @0x3070b8 指向，NUL 截断，限 256B）
  - `hasOk`：是否有确认按钮（UIPopupMsg_fpOK @0x3070e0 非空）
  - `hasCancel`：是否有取消按钮（UIPopupMsg_fpCancel @0x3070d8 非空）
  - `buttons`（✅ v0.3.12 实机验证）：按钮文案数组，按弹窗类型推导（popupType @0x712518：1=是/否、0=单确认）——实机验证：出售弹窗 `["是","否"]`、保存成功 `[]`。⚠️ hasCancel 不能反映"否"按钮（出售弹窗 fpCancel=0 但 UI 有是/否）

### Snapshot（快速状态快照，✅ v0.3.7 /api/info/snapshot）

```json
{
  "screen": "world",
  "money": 72503,
  "mapId": 2056, "x": 304, "y": 376,
  "mainMercenarySlot": 0, "partyCount": 2,
  "party": [
    {
      "type": 1, "level": 27,
      "hp": 10598, "mp": 200, "maxHp": 10664, "maxMp": 250,
      "mainStats": [96, 139, 101, 54, 38],
      "equipment": [
        { "slot": 0, "category": 333, "rarity": 3, "name": "光荣的火冠" },
        null
      ],
      "name": "凯恩"
    }
  ],
  "mercenaries": [
    { "slot": 0, "type": 0, "inParty": true, "name": "凯恩", "level": 27, "x": 320, "y": 480 }
  ]
}
```
一站式聚合端点：UI 状态 + 玩家全局（金币/地图/坐标/队伍数）+ 队伍成员摘要（等级/HP MP/主属性/装备名称/角色名）+ 全部佣兵概要。装备名称由 Kotlin 层注入。

### 静态表（由 game_res 解析产出，✅ 已就绪）

每条记录结构见 `static-data/json/tables/<表名>.json`，已验证字段如下（`field_catalog.json` 完整目录）：

**ITEMDATABASE（物品，1,018 条，23B/条）**
```json
{ "itemId": 1, "name": "治疗药水", "raw": { "u16": [1, 25, ...], "hex": "..." } }
```
已验证：`+0` 名称 text_id（中文已解出，命中 100%）。

**MONDATABASE（怪物，553 条，39B/条）**
```json
{ "monsterId": 0, "name": "树丛", "skills": { "start": 0, "count": 1 }, "stats": { "+0x0f~0x14": "6 项成长属性" } }
```
已验证：`+0` 名称、`+5/+6` u8、`+7` u32、`+0x0b` u16、`+0x0f~0x14` 六项属性、`+0x22` 技能起点、`+0x23` 技能数、`+0x24` u8。

**QUESTINFOBASE（任务，507 条，32B/条）**
```json
{ "questId": 0, "title": "任务测试1", "detail": "...", "progress": "...", "completion": "...",
  "reward": { "start": 0, "count": 1 } }
```
已验证：`+2` 标题、`+14` 详情、`+16` 进度、`+18` 完成（文本 id，中文已解出）；`+26` 奖励起点、`+28` 奖励数。

**QUESTREWARDBASE（任务奖励，394 条，5B/条）**
```json
{ "itemId": 0, "itemName": "金币", "quantity": 100, "classMask": 63 }
```
已验证：`+0` item_id、`+2` 数量、`+4` 职业掩码。

**MAPINFOBASE（地图，416 条，11B/条）**
```json
{ "mapId": 3513, "name": "黑暗骑士团营地" }
```
已验证：`+0` 地图 ID（= 文本 id），416/416 名称命中。

**CHARCLASSBASE（职业，6 条，20B/条）**：`+2` 描述文本（已验证）。
**SKILLDESCBASE（技能，114 条，24B/条）**：`+2` 技能文本（已验证）。
**MERCENARYINFOBASE（佣兵，47 条，8B/条）**：`+2` 佣兵文本（已验证）。
**NPCINFOBASE（NPC，656 条，10B/条）**：`+0` 名称（已验证）。

> 其余数值字段语义待逆向（当前 71 个字段已验证，见 `field_catalog.json`）。

## 4. 端点设计（v0.3.13 API 分层重构落地）

**结构原则**（/api 下按 §0 新分层：1 类别 / 2 系统 / 3+ 逐层拆解）：

```
/api/health              服务健康（顶层）
/api/info/*              游戏动态信息获取（GET，只读）—— 按系统拆分
/api/data/*              游戏静态数据获取（GET，只读）
/api/action/*            游戏合法操作端点（POST）—— 玩家游戏内可做的事
/api/op/*                游戏 OP 操作端点（POST，需 OP 权限）—— 未来实现
```

- **动态信息（GET）**：`/api/info/*` —— current-map/party/mercenary/inventory/quest/ui/game/events 按系统组织，复合端点 + 简单子端点
- **静态数据（GET）**：`/api/data/*` —— map/list、{table}、{table}/search、text、events
- **合法操作（POST）**：`/api/action/*` —— 玩家游戏内可做的事（含 get-path 寻路）
- **OP 操作（POST）**：`/api/op/*` —— 改数据/强行操作，需 OP 权限，**未来实现**

> 网络前提：模块 manifest 保留 **INTERNET 权限** + `usesCleartextTraffic=true`（游戏"去除联网"仅断开了功能，权限未删），HTTP 服务可直接用。

**信息获取端点（GET，✅ v0.3.13 重构 + 真机验证）**

| 方法 | 路径 | 说明 | 数据源 | 状态 |
|---|---|---|---|---|
| GET | `/api/health` | 服务健康（ok/version/game/base） | BuildConfig + gamestate + native base | ✅ v0.3.13 |
| GET | `/api/info/current-map` | 当前地图复合（mapId/x/y/tile/units/enemies/interactives/drops） | native 直读/函数 | ✅ v0.3.13 |
| GET | `/api/info/current-map/id` | 地图 ID | MAP_nBaseInfo | ✅ v0.3.13 |
| GET | `/api/info/current-map/tile` | 玩家所在瓦片通行状态 | G_TILE_GOT | ✅ v0.3.13 |
| GET | `/api/info/current-map/units` | 全部场景单位（队伍/NPC/怪物） | CHARSYSTEM 池 | ✅ v0.3.13 |
| GET | `/api/info/current-map/enemies` | 敌人/召唤物（units 过滤 status==2） | CHARSYSTEM 池 | ✅ v0.3.13 |
| GET | `/api/info/current-map/interactives` | 城镇 NPC/佣兵（units 过滤 status==1） | CHARSYSTEM 池 | ✅ v0.3.13 |
| GET | `/api/info/current-map/drops` | 掉落物（数据源未探索，占位空数组） | — | ⏳ 占位 |
| GET | `/api/info/party` | 出战角色复合（3 槽完整，含装备名/属性名注入） | PARTY_GetMember | ✅ v0.3.13 |
| GET | `/api/info/party/count` | 出战人数 | PARTY_GetSize | ✅ v0.3.13 |
| GET | `/api/info/party/leader` | 主控角色（mainMercenarySlot 对应槽） | SAVE_nMainMercenarySlot | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}` | 指定出战槽完整状态 | PARTY_GetMember | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/id` | 角色类型 type | +0x00 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/name` | 角色名 | CHAR_GetName | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/level` | 等级 | +0x0E | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/exp` | 经验/下一级 | CHAR_GetExp | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/hp` | 血量/上限 | +0x1F0/GetAttr | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/mp` | 魔力/上限 | +0x1F4/GetAttr | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/stats` | 战斗属性对象（0..31） | +0x20 数组 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/stats/{attr}` | 单个属性值 | +0x20+attr*4 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/equipment` | 装备列表（含名称注入） | fn_get_equip | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/equipment/{equipSlot}` | 指定装备槽 | fn_get_equip | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/skills` | 技能完整（链表/位图/技能点/当前技能） | +0x2A0 链表 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/skills/list` | 技能列表 | +0x2A0 链表 | ✅ v0.3.13 |
| GET | `/api/info/mercenary` | 全部佣兵（含未上场） | MERCENARYSYSTEM 槽数组 | ✅ v0.3.13 |
| GET | `/api/info/mercenary/list` | 非空佣兵槽 id 列表 | MERCENARYSYSTEM | ✅ v0.3.13 |
| GET | `/api/info/mercenary/{slot}` | 指定佣兵槽 | FindAsMercenarySlot | ✅ v0.3.13 |
| GET | `/api/info/inventory` | 背包复合（bags 完整，含名称注入） | INVEN_pItem | ✅ v0.3.13 |
| GET | `/api/info/inventory/money` | 金币 | fn_get_money | ✅ v0.3.13 |
| GET | `/api/info/inventory/items` | 全部物品展平列表（含 bag 字段） | INVEN_pItem | ✅ v0.3.13 |
| GET | `/api/info/inventory/bag/{i}/info` | 袋信息（容量/占用） | INVEN_pItem | ✅ v0.3.13 |
| GET | `/api/info/inventory/bag/{i}/{slot}` | 指定袋槽物品 | INVEN_pItem | ✅ v0.3.13 |
| GET | `/api/info/quest` | 任务复合（active/list/completed） | QUESTSYSTEM | ✅ v0.3.13（list/completed ⏳ 占位） |
| GET | `/api/info/quest/active` | 当前激活任务 ID | QUESTSYSTEM_nActiveQuest | ✅ v0.3.13 |
| GET | `/api/info/quest/list` | 已接受任务列表（数据源未逆向，占位空） | — | ⏳ 占位 |
| GET | `/api/info/quest/list/{id}` | 任务详情（数据源未逆向，占位） | — | ⏳ 占位 |
| GET | `/api/info/quest/completed` | 已完成任务列表（数据源未逆向，占位空） | — | ⏳ 占位 |
| GET | `/api/info/ui` | 界面状态复合（screen/dialogActive/dialog） | STATE/UIPopupMsg/g_arrPopupStack | ✅ v0.3.13 |
| GET | `/api/info/ui/screen` | 当前界面 | STATE_nState | ✅ v0.3.13 |
| GET | `/api/info/ui/panel` | 当前面板（screen 为面板时） | g_arrPopupStack | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog` | 弹窗复合（active/dialog） | UIPopupMsg | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/active` | 弹窗是否激活 | UIPopupMsg_bOn | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/text` | 弹窗文本 | UIPopupMsg_pText | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/buttons` | 弹窗按钮文案 | popupType 推导 | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/ok` | 是否有确认按钮 | UIPopupMsg_fpOK | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/cancel` | 是否有取消按钮 | UIPopupMsg_fpCancel | ✅ v0.3.13 |
| GET | `/api/info/game` | 游戏整体复合（snapshot+info） | 聚合 | ✅ v0.3.13 |
| GET | `/api/info/game/snapshot` | 局内全量快照 | 聚合 | ✅ v0.3.13 |
| GET | `/api/info/game/info` | 局外软件信息（version/packageName/base） | 常量 + native | ✅ v0.3.13（loggedIn/saveSlots ⏳ 占位） |
| GET | `/api/info/events?since=` | 事件流（轮询差异检测，since 预留） | native 快照对比 | ✅ v0.3.13 |

**静态数据端点（GET，✅ v0.3.13 重构 + 真机验证）**

| 方法 | 路径 | 说明 | 数据源（模块 assets，28 表子集） |
|---|---|---|---|
| GET | `/api/data/map/list` | 地图列表（id+名称） | `tables/MAPINFOBASE` |
| GET | `/api/data/map/{mapId}` | 指定地图静态信息 | `tables/MAPINFOBASE` |
| GET | `/api/data/list` | 可用静态表列表 | `manifest.json` |
| GET | `/api/data/{table}` | 任意内嵌表（表名大写） | `tables/<NAME>.json` |
| GET | `/api/data/{table}/search?q=` | 表内名称模糊搜索 | `tables/<NAME>.json` |
| GET | `/api/data/text?lang=zh-Hans` | 文本（zh-Hans + en 2 语言） | `text/<lang>.json` |
| GET | `/api/data/events` | 剧情事件（命令/条件/文本） | `reverse/events.json` |

> **模块内嵌静态数据为子集**（28 表 + 2 语言，约 13MB，随 APK 打包；清单见
> `module/app/src/main/assets/static-data/manifest.json`）。完整 100 表 + 6 语言以 `static-data/json/` 为准。

**静态数据响应示例**（GET `/api/data/ITEMDATABASE/search?q=治疗`）：
```json
{
  "items": [
    { "index": 788, "name": "鑫迪的治疗药", "raw": { "u16": [818, 65314, ...], "text_0": "鑫迪的治疗药" } }
  ]
}
```

预留扩展：`/api/info/player/party/{index}` 单角色详情、`/api/info/inventory/{itemId}` 单道具。

### 4.1 合法操作端点（POST /api/action/*，✅ v0.3.6，玩家游戏内可做的事）

> 与信息获取端点分离（v0.3.1 API 重构）。写操作签名见 `docs/control-capability.md` §5/§5.1；
> 分级依据见 `docs/player-operations.md`。调用前检查 `STATE_nState==5`（游戏中），操作成功返回 `{"ok":true,"state":<最新状态>}`。
> **v0.3.2-0.3.6 真机验证修复**（逆向结论见 `docs/data-sources.md`）：switch 路由注册、use-item 消耗品校验、discard 返回语义、equip 自动替换、party 边界校验。

| 方法 | 路径 | 操作 | body | 边界校验（v0.3.2+） |
|---|---|---|---|---|
| POST | `/api/action/player/move` | 移动（寻路+沿路径移动） | `{"x":304,"y":376}` | 目标不可达返回 `no path` |
| POST | `/api/action/player/use-item` | 使用物品（药水/卷轴） | `{"bag":0,"slot":3}` | 非消耗品返回 `item not usable`（ITEMDATABASE IsUse，v0.3.2） |
| POST | `/api/action/player/{role}/equip` | 穿装备（背包位置或类别） | `{"bag":0,"slot":3}` 或 `{"category":512}` | 目标槽占用自动替换（先卸后穿，v0.3.3） |
| POST | `/api/action/player/{role}/unequip` | 脱装备（装备槽） | `{"slot":2}` | 空槽返回 `unequip failed` |
| POST | `/api/action/player/{role}/auto-attack` | 自动攻击开关 | `{"on":true}` | — |
| POST | `/api/action/player/{role}/skill` | 学习技能（消耗技能点） | `{"actionId":3,"level":1}` | — |
| POST | `/api/action/player/switch` | 切换主控角色 | `{"slot":1}` | 路由已注册（方法名改为 `switchPlayer`，v0.3.2） |
| POST | `/api/action/inventory/discard` | 丢弃物品（指定槽） | `{"bag":0,"slot":5}` | 按槽位清空判定成功（v0.3.2） |
| POST | `/api/action/party/include` | 佣兵入队 | `{"mercenarySlot":1}` | 已在队→`already in party`；满员→`party full`（v0.3.6） |
| POST | `/api/action/party/exclude` | 佣兵离队 | `{"mercenarySlot":1}` | 主控→`cannot exclude leader`；任务NPC→`cannot exclude quest npc`（v0.3.5） |
| POST | `/api/action/dialog/ok` | 弹窗确定（✅ v0.3.11，调用 UIPopupMsg_ButtonOKExe 无参） | 无 body | 非弹窗→`no dialog`；执行确认动作（如出售/销毁），真机验证金币入账 |
| POST | `/api/action/dialog/cancel` | 弹窗取消（✅ v0.3.11，调用 UIPopupMsg_ButtonCancelExe 无参） | 无 body | 非弹窗→`no dialog`；无取消按钮时仅关闭弹窗（Free 路径），安全 |

> `role` = 0..2（出战槽位）。**依赖 UI 状态的操作（商店购买/任务接交/技能释放/合成执行/强化镶嵌）暂缓**，见 player-operations.md §4.2。
> **审查修正（2026-08-05）**：`inventory/sell`（任意定价=刷钱漏洞）、`teleport`（任意切图/瞬移）已移除并归 OP，见 docs/player-operations.md §4.1。

### 4.1a OP 操作端点（POST /api/op/*，⏳ 未来实现，需 OP 权限）

> v0.3.1 **不暴露 HTTP 端点**（native 函数已实现，见 control-capability.md §5）。未来实现：权限获取机制 + 端点组。
> 规划：`/api/op/player/money`（set/add/minus——直接增减金币，玩家游戏内做不到）、`/api/op/player/experience`（set/add）、`/api/op/player/status-point`（set）、
> `/api/op/player/skill-point`、`/api/op/inventory/sell`（任意定价出售——绕过商店定价）、`/api/op/move/teleport`（任意切图/瞬移）、`/api/op/item/give`（生成物品）、`/api/op/item/attributes`（强制强化/镶嵌）、
> `/api/op/equip/force`（强行装备）、`/api/op/move/through`（无视碰撞）、`/api/op/consume`（消耗不减少数量）。

### 4.2 事件流（✅ v0.3.0 /api/info/events）

```json
{
  "events": [
    { "type": "money", "old": 100, "new": 150 },
    { "type": "hp", "role": 0, "old": 10598, "new": 8000 },
    { "type": "level_up", "role": 1, "old": 10, "new": 11 },
    { "type": "move", "old": 0, "new": 0 },
    { "type": "inventory", "old": 13, "new": 12 }
  ]
}
```

实现方式：**轮询差异检测**（零 hook）——每次 GET 对比 native 上次快照（money/hp/mp/level/exp/坐标/背包物品数），输出变化事件并更新快照。
事件类型：`money`、`hp`、`mp`、`exp`、`level_up`、`move`（old/new 为 0/0 占位）、`inventory`。
调用方需周期性轮询（如 500ms-1s）消费事件；首次调用仅建立基线返回空列表。

### 稀有度（rarity）取值

装备稀有度 5 档：`0=白`、`1=绿`、`2=蓝`、`3=黄（特殊装备）`、`4=紫`（最终映射以 `ITEMRARITYGRADEBASE` 解析结果为准，M3 确认）。

## 5. 数据源映射

> 各端点字段的数据来源（VMA/结构体偏移/调用函数）详细见 `docs/data-sources.md` §2「数据访问清单」，本节不重复。
> **读取方式** = 模块 native 层 base+VMA 直读全局 / 调用 Getter（不用 dlopen/dlsym：Android linker namespace 隔离会加载独立副本读不到数据）。
> 静态数据（物品名/属性名联查）见 `docs/reference/static-data.md`。

## 6. 部署形态差异

| 项 | 手机版（LSPosed 模块） | 服务器版（LSPatch 集成） |
|---|---|---|
| 数据获取 | 同进程 Hook | 同进程 Hook（LSPatch 注入） |
| API 访问 | 监听 0.0.0.0，局域网 Wi-Fi IP | Waydroid NAT，需端口转发 |
| minSdk | Android 11+（Zygisk-LSPosed） | 随游戏 targetSdk 29 |
| 静态数据 | JSON 库随模块打包 | JSON 库随集成 APK 打包 |

## 7. 待确认/待验证项

> 未完成项已统一收录至 `docs/backlog.md`（唯一待办来源），本节仅保留已解决记录。

**已解决**：
- [x] game_res 静态数据提取（✅ M3：100 表 + 6 语言文本 + 事件/SNASYS，见 `docs/reference/static-data.md`）
- [x] 坐标实时源（角色 +0x02/+0x04，真机实测走动变化）
- [x] 地图 ID 实时源（MAP_nBaseInfo+0，切图实测变动；SAVE_nMapID 是存档字段）
- [x] 背包完整物品列表（INVEN_pItem 槽数组，v0.2.15 实测）
- [x] partyCount 用 PARTY_GetSize（此前硬编码 3 已修复）
- [x] 当前控制角色（SAVE_nMainMercenarySlot，v0.2.16）
- [x] 角色技能 + 技能等级（+0x2A0 技能链表，v0.2.23）
- [x] 装备/物品属性（+0x18 魔法倍率/+0x19 宝石/+0x1a 附魔/+0x20 词缀，v0.2.24）
- [x] 物品名称联查（category → ITEMDATABASE itemId → text_0，v0.2.25-27）
- [x] 属性名映射（CHAR_GetStat 主属性 + 能力点，v0.2.28-29）
- [x] 加点数据（能力点 statusPoint + 技能点 skillPoints）
- [x] 全部佣兵（未上场槽 + FindAsMercenarySlot + CHAR_GetName，v0.2.30-31）
- [x] 动态背包袋（INVEN_pItem 6袋×16槽，capacity/slotCount 语义 v0.2.32）
- [x] /api/info/units 单位坐标（CHARSYSTEM 池枚举，v0.2.19-21）
- [x] /api/info/ui UI 界面状态（STATE_nState，v0.2.22）
- [x] /api/info/path 寻路（CHAR_SearchPath + PATHLIST，v0.2.33-34）
- [x] 召唤物识别（units 含 status=2 怪物/召唤物，类型字段已逆向）
- [x] /api/info/events 事件流（v0.3.0 轮询差异检测实现，零 hook；真机验证轮询有效性）
- [x] 操作端点与信息获取分离（v0.3.1：API 四层 /api/info + /api/data + /api/action + /api/op）
- [x] 合法操作端点（10 个：move/use-item/equip/unequip/auto-attack/skill/switch/discard/include/exclude；**v0.3.2-0.3.6 真机逐端点验证**，逆向结论见 docs/data-sources.md）
- [x] 全量 API 审查：sell（任意定价）/teleport（任意传送）/money（直接增减）判定 OP 移除（见 docs/player-operations.md §4.1）
- [x] `/api/info/gamestate` 细粒度游戏状态（v0.3.7：STATE_nPrevState + UIPopupMsg_bOn + UIMainMenu_bDrawFull + g_arrPopupStack）
- [x] `/api/info/snapshot` 快速状态快照（v0.3.7：UI+角色+地图+小队一站式聚合）
