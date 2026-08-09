# API 信息清单与接口规格

> **本文档 = API 规格（面向调用方）**：API 的划分与结构、有哪些 API、如何调用（参数）、如何响应（数据模型）。
> 技术实现细节（VMA/函数签名/调用链/游戏内机制）见 `docs/api-technical-spec.md`（技术实现方案）。
> 状态：v0.3.13（2026-08-08 API 分层重构落地）。静态数据（M3 ✅）+ 运行时只读端点（M4 ✅ 真机验证 v0.2.15→v0.2.34）
> + 操作端点/事件流（v0.3.0 ✅ 实现，**v0.3.2-0.3.6 真机验证修复**）；**v0.3.13 端点全量分层重构 + 真机验证**；
> 未实现项见 §4 状态标注与 §7。

## 0. API 分层设计

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
└── /events?since={ts}             ← 简单（事件流：轮询差异检测，采样间隔 500ms-1s）
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

### 0.4 action 系统结构（✅ 2026-08-08 定稿）

> 所有操作相关 API 归 `/api/action/*`（合法操作）与 `/api/op/*`（OP 操作），按系统拆分（对齐 info 划分）。

```
/api/action/                        ← 合法操作（玩家游戏内可做的事，38 端点）
├── /movement/                      ← 移动/场景
│   ├── move                        ← 寻路移动
│   ├── move/cancel                 ← 打断移动
│   ├── walk                        ← 持续移动（模拟方向键）
│   └── walk/stop                   ← 停止持续移动
├── /combat/                        ← 战斗
│   ├── /{role}/config/auto-attack  ← 自动攻击开关
│   ├── /{role}/config/skill-usage  ← 技能使用策略
│   ├── /{role}/attack              ← 攻击指定目标
│   ├── /{role}/stop                ← 停止攻击
│   ├── /{role}/cast                ← 释放技能（占位）
│   └── /{role}/switch              ← 切换主控
├── /inventory/                     ← 背包/物品
│   ├── use-item                    ← 统一使用（自动分派：药水/开箱/解封/掷骰/配方书/佣兵卡）
│   ├── discard                     ← 丢弃物品
│   ├── move                        ← 移动/整理背包
│   ├── sell                        ← 出售（静态表价格）
│   ├── /{role}/equip               ← 穿装备
│   ├── /{role}/unequip             ← 脱装备
│   └── jewel                       ← 宝石镶嵌（背包宝石→背包装备/已装备）
├── /character/                     ← 角色成长（主角凯恩专用，无 role）
│   ├── stat                        ← 属性加点（能力点校验）
│   ├── skill                       ← 技能加点（技能点校验）
│   ├── stat-reset                  ← 属性重置（还属性点）
│   └── skill-reset                 ← 技能重置（还技能点）
├── /party/                         ← 队伍/佣兵
│   ├── include                     ← 佣兵入队
│   ├── exclude                     ← 佣兵离队
│   ├── discharge                   ← 佣兵遣散
│   └── withdraw                    ← 取出佣兵装备
├── /npc/                           ← NPC 交互
│   ├── interact                    ← 开始交互（对着 NPC 点攻击键，占位）
│   ├── dialog/next                 ← 对话下一步（占位）
│   └── dialog/select               ← 对话选项选择（占位，选项触发任务=合法）
├── /ui/                            ← UI 交互
│   ├── dialog/ok                   ← 弹窗确定
│   ├── dialog/cancel               ← 弹窗取消
│   ├── panel/open                  ← 打开面板
│   ├── panel/close                 ← 关闭当前面板
│   └── panel/close-to              ← 关闭到指定层
├── /shop/                          ← 商店
│   └── buy                         ← 购买（选中+确认，不含开面板）
├── /quest/                         ← 任务
│   └── quit                        ← 放弃任务（占位）
├── /save/                          ← 存档
│   ├── save                        ← 手动存档（静默）
│   └── load                        ← 读档（仅主菜单/选档界面）
└── /craft/                         ← 合成
    └── mix                         ← 配方合成（免 UI）

/api/op/                            ← OP 操作（游戏内做不到，需 OP 权限，16 端点）
├── /quest/                         ← accept(force)/complete（绕过 NPC 直接接交）
├── /character/{role}/              ← experience/status-point/skill-point/skill-level
├── /party/swap                     ← 队伍换位
├── /inventory/                     ← set-slot/set-equip/money
├── /craft/mix-direct               ← 免配方机直合成
├── /combat/                        ← {role}/heal、rest、revive、hate
└── /movement/teleport              ← 传送（force 默认检查目标合法性）
```

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

## 2. 数据模型

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
字段：`money` 金币（实时）、`mapId` 实时地图 ID、`x/y` 实时玩家坐标、`mainMercenarySlot` 当前控制角色槽（v0.2.16）、`partyCount` 出战人数。

### Role（出战角色，✅ 已实现 /api/info/party 返回数组）

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
`stats` 战斗属性数组（0..31）、`mainStats` 主属性（0-4=力量/敏捷/体力/智力/精力，v0.2.29）、
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

### Skills（角色技能，✅ v0.2.23 /api/info/party/{slot}/skills）

```json
[
  { "role": 0, "skills": [ { "actionId": 0, "level": 1 }, ... ],
    "unlockBitmap": 65535, "activeSkillId": 0, "skillPoints": 6 },
  null
]
```
来源：角色技能链表（节点 action_id/level/next）、+0x2B0 解锁位图、+0x280 当前技能、+0x328 技能点。

### Mercenaries（全部佣兵，✅ v0.2.30-31 /api/info/mercenary）

```json
[
  { "slot": 0, "type": 0, "flags": 219, "inParty": true,
    "name": "凯恩", "level": 27, "x": 320, "y": 480 },
  { "slot": 1, "type": 2, "flags": 77, "inParty": false,
    "name": "西雷斯", "level": 26, "x": 2048, "y": 2048 }
]
```
来源：佣兵槽数组（20B/槽，flags bit0=占用 bit1=在队伍）+ CHARSYSTEM_FindAsMercenarySlot 关联角色
+ CHAR_GetName 名称。未上场佣兵坐标 2048=未激活哨兵。

### Path（寻路，✅ v0.2.33-34 POST /api/action/get-path body {tx,ty}，v0.3.13 迁移）

```json
{ "target": { "x": 200, "y": 360 },
  "start": { "x": 320, "y": 472 },
  "found": true,
  "path": [ { "x": 320, "y": 472 }, ... ] }
```
来源：CHAR_SearchPath(hero, tx, ty, 1) 仅计算存储路径（不触发移动），结果存角色 PATHLIST 链表
（节点网格坐标 ×8 = 像素坐标）。

### GameState（游戏界面状态，✅ v0.3.10 /api/info/ui）

```json
{ "screen": "dialog", "dialogActive": true, "dialog": { "text": "是否出售？", "hasOk": true, "hasCancel": false } }
```
字段：
- `screen`：当前界面（✅ v0.3.9 面板识别经真机验证）：
  - `"loading"` 初始化 / `"main_menu"` 主菜单 / `"world"` 游戏世界 / `"dialog"` 弹窗激活（popupOn 标志）
  - 面板（popup 栈顶场景）：`"character_info"` 人物属性 / `"inventory"` 背包·装备 / `"skills"` 技能 / `"mercenary"` 佣兵管理 / `"quests"` 任务 / `"settings"` 选项·系统菜单 / `"shop"` 商店 / `"craft"` 合成 / `"npc"`·`"npc_quest"`·`"npc_rest"`·`"npc_revive"` NPC 交互 / `"save_slot"` 存档选择 / `"character_select"` 角色选择 / `"options"` 游戏内选项 / `"shortcut"` 快捷菜单 / `"world_map"` 世界地图 / `"input_count"` 数量输入 / `"choice"` 选择 / `"wipeout"` / `"daily_reward"` 每日奖励 / `"in_app"` 内购 / `"ui_panel"` 其他未匹配面板
- `dialogActive`：是否有阻塞弹窗。**操作前置检查**：调用操作端点前若为 true，操作将被 UI 阻塞
- `dialog`（✅ v0.3.10，仅 dialogActive=true 时出现）：弹窗信息：
  - `text`：弹窗内容文本（UTF-8，NUL 截断，限 256B）
  - `hasOk`：是否有确认按钮
  - `hasCancel`：是否有取消按钮
  - `buttons`（✅ v0.3.12 实机验证）：按钮文案数组，按弹窗类型推导（1=是/否、0=单确认）——实机验证：出售弹窗 `["是","否"]`、保存成功 `[]`。⚠️ hasCancel 不能反映"否"按钮（出售弹窗 fpCancel=0 但 UI 有是/否）

### Snapshot（快速状态快照，✅ v0.3.7 /api/info/game/snapshot）

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

## 3. 端点设计（v0.3.13 API 分层重构落地）

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

| 方法 | 路径 | 说明 | 状态 |
|---|---|---|---|
| GET | `/api/health` | 服务健康（ok/version/game/base） | ✅ v0.3.13 |
| GET | `/api/info/current-map` | 当前地图复合（mapId/x/y/tile/units/enemies/interactives/drops） | ✅ v0.3.13 |
| GET | `/api/info/current-map/id` | 地图 ID | ✅ v0.3.13 |
| GET | `/api/info/current-map/tile` | 玩家所在瓦片通行状态 | ✅ v0.3.13 |
| GET | `/api/info/current-map/units` | 全部场景单位（队伍/NPC/怪物，含 level/hp/mp/name，v0.3.14 增强） | ✅ v0.3.14 |
| GET | `/api/info/current-map/enemies` | 敌人/召唤物（units 过滤 status==2，含 level/hp/mp/name） | ✅ v0.3.14 |
| GET | `/api/info/current-map/interactives` | 城镇 NPC/佣兵（units 过滤 status==1） | ✅ v0.3.13 |
| GET | `/api/info/current-map/drops` | 掉落物（数据源未探索，占位空数组） | ⏳ 占位 |
| GET | `/api/info/party` | 出战角色复合（3 槽完整，含装备名/属性名注入） | ✅ v0.3.13 |
| GET | `/api/info/party/count` | 出战人数 | ✅ v0.3.13 |
| GET | `/api/info/party/leader` | 主控角色（mainMercenarySlot 对应槽） | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}` | 指定出战槽完整状态 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/id` | 角色类型 type | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/name` | 角色名 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/level` | 等级 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/exp` | 经验/下一级 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/hp` | 血量/上限 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/mp` | 魔力/上限 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/stats` | 战斗属性对象（0..31） | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/stats/{attr}` | 单个属性值 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/equipment` | 装备列表（含名称注入） | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/equipment/{equipSlot}` | 指定装备槽 | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/skills` | 技能完整（链表/位图/技能点/当前技能） | ✅ v0.3.13 |
| GET | `/api/info/party/{slot}/skills/list` | 技能列表 | ✅ v0.3.13 |
| GET | `/api/info/mercenary` | 全部佣兵（含未上场） | ✅ v0.3.13 |
| GET | `/api/info/mercenary/list` | 非空佣兵槽 id 列表 | ✅ v0.3.13 |
| GET | `/api/info/mercenary/{slot}` | 指定佣兵槽 | ✅ v0.3.13 |
| GET | `/api/info/inventory` | 背包复合（bags 完整，含名称注入） | ✅ v0.3.13 |
| GET | `/api/info/inventory/money` | 金币 | ✅ v0.3.13 |
| GET | `/api/info/inventory/items` | 全部物品展平列表（含 bag 字段） | ✅ v0.3.13 |
| GET | `/api/info/inventory/bag/{i}/info` | 袋信息（容量/占用） | ✅ v0.3.13 |
| GET | `/api/info/inventory/bag/{i}/{slot}` | 指定袋槽物品 | ✅ v0.3.13 |
| GET | `/api/info/quest` | 任务复合（active/list/completed） | ✅ v0.3.13（list/completed ⏳ 占位） |
| GET | `/api/info/quest/active` | 当前激活任务 ID | ✅ v0.3.13 |
| GET | `/api/info/quest/list` | 已接受任务列表（数据源未逆向，占位空） | ⏳ 占位 |
| GET | `/api/info/quest/list/{id}` | 任务详情（数据源未逆向，占位） | ⏳ 占位 |
| GET | `/api/info/quest/completed` | 已完成任务列表（数据源未逆向，占位空） | ⏳ 占位 |
| GET | `/api/info/ui` | 界面状态复合（screen/dialogActive/dialog） | ✅ v0.3.13 |
| GET | `/api/info/ui/screen` | 当前界面 | ✅ v0.3.13 |
| GET | `/api/info/ui/panel` | 当前面板（screen 为面板时） | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog` | 弹窗复合（active/dialog） | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/active` | 弹窗是否激活 | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/text` | 弹窗文本 | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/buttons` | 弹窗按钮文案 | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/ok` | 是否有确认按钮 | ✅ v0.3.13 |
| GET | `/api/info/ui/dialog/cancel` | 是否有取消按钮 | ✅ v0.3.13 |
| GET | `/api/info/game` | 游戏整体复合（snapshot+info） | ✅ v0.3.13 |
| GET | `/api/info/game/snapshot` | 局内全量快照 | ✅ v0.3.13 |
| GET | `/api/info/game/info` | 局外软件信息（version/packageName/base） | ✅ v0.3.13（loggedIn/saveSlots ⏳ 占位） |
| GET | `/api/info/events?since=` | 事件流（轮询差异检测，since 预留） | ✅ v0.3.13 |

**静态数据端点（GET，✅ v0.3.13 重构 + 真机验证）**

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/data/map/list` | 地图列表（id+名称） |
| GET | `/api/data/map/{mapId}` | 指定地图静态信息 |
| GET | `/api/data/list` | 可用静态表列表 |
| GET | `/api/data/{table}` | 任意内嵌表（表名大写） |
| GET | `/api/data/{table}/search?q=` | 表内名称模糊搜索 |
| GET | `/api/data/text?lang=zh-Hans` | 文本（zh-Hans + en 2 语言） |
| GET | `/api/data/events` | 剧情事件（命令/条件/文本） |

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

预留扩展：`/api/info/party/{slot}` 已实现（v0.3.13）；`/api/info/inventory/bag/{bag}/{slot}` 单道具已实现（v0.3.13）。

### 3.1 合法操作端点（POST /api/action/*，✅ v0.3.6 起，玩家游戏内可做的事）

> 与信息获取端点分离（v0.3.1 API 重构）。写操作签名见 `docs/control-capability.md` §5/§5.1；
> 分级依据见 `docs/api-technical-spec.md`。**简化假设**：调用前检查 `STATE_nState==5`（游戏中，见 control-capability §0——为减少开发难度与测试广度，非逐操作实证），操作成功返回 `{"ok":true,"state":<最新状态>}`。
> **v0.3.2-0.3.6 真机验证修复**（逆向结论见 `docs/data-sources.md`）：switch 路由注册、use-item 消耗品校验、discard 返回语义、equip 自动替换、party 边界校验。
> **2026-08-08 结构定稿**：按系统拆分 12 类别（结构见 §0.4）。旧 `/api/action/player/*` 路径迁移至对应类别（如 move→movement/move、use-item→inventory/use-item）。

**已实现端点（迁移自旧路径）**：

| 方法 | 路径 | 操作 | body | 边界校验（v0.3.2+） |
|---|---|---|---|---|
| POST | `/api/action/movement/move` | 移动（寻路+沿路径移动） | `{"x":304,"y":376}` | 目标不可达返回 `no path` |
| POST | `/api/action/inventory/use-item` | 使用物品（药水/卷轴） | `{"bag":0,"slot":3}` | 非消耗品返回 `item not usable`（ITEMDATABASE IsUse，v0.3.2） |
| POST | `/api/action/inventory/{role}/equip` | 穿装备（背包位置或类别） | `{"bag":0,"slot":3}` 或 `{"category":512}` | 目标槽占用自动替换（先卸后穿，v0.3.3） |
| POST | `/api/action/inventory/{role}/unequip` | 脱装备（装备槽） | `{"slot":2}` | 空槽返回 `unequip failed` |
| POST | `/api/action/combat/{role}/config/auto-attack` | 自动攻击开关 | `{"on":true}` | — |
| POST | `/api/action/character/skill` | 学习技能（主角专用，消耗技能点） | `{"actionId":3,"level":1}` | — |
| POST | `/api/action/combat/{role}/switch` | 切换主控角色（**v0.4.0 迁移**：目标槽 = 路径 role，无 body） | 无 body | 路由已注册（方法名改为 `switchPlayer`，v0.3.2）；不可切换角色返回 `switch failed` |
| POST | `/api/action/inventory/discard` | 丢弃物品（指定槽） | `{"bag":0,"slot":5}` | 按槽位清空判定成功（v0.3.2） |
| POST | `/api/action/party/include` | 佣兵入队 | `{"mercenarySlot":1}` | 已在队→`already in party`；满员→`party full`（v0.3.6） |
| POST | `/api/action/party/exclude` | 佣兵离队 | `{"mercenarySlot":1}` | 主控→`cannot exclude leader`；任务NPC→`cannot exclude quest npc`（v0.3.5） |
| POST | `/api/action/ui/dialog/ok` | 弹窗确定（✅ v0.3.11） | 无 body | 非弹窗→`no dialog`；执行确认动作（如出售/销毁），真机验证金币入账 |
| POST | `/api/action/ui/dialog/cancel` | 弹窗取消（✅ v0.3.11） | 无 body | 非弹窗→`no dialog`；无取消按钮时仅关闭弹窗（Free 路径），安全 |
| POST | `/api/action/combat/{role}/attack` | 攻击指定目标（✅ v0.4.2，CHAR_SetTarget+CHAR_MakeDefaultAttack） | `{"targetSlot":5}` | 目标无效→`target not found`（角色池解析）；缺参→`targetSlot required` |
| POST | `/api/action/combat/{role}/stop` | 停止战斗（✅ v0.4.2，CHAR_StopCombat） | 无 body | 非战斗态调用安全（清标志幂等） |

**已实现待迁移（v0.3.14 新增）**：
| POST | `/api/action/party/swap` → **迁移至 /api/op/party/swap** | 队伍换位（**游戏内做不到=OP 操作**，2026-08-08 用户确认） | `{"a":0,"b":1}` | 槽位越界→`bad slot`；缺参→`a/b required`（v0.3.14） |
| POST | `/api/action/get-path` → **内部化**（寻路仅 move 内部调用，不暴露） | 寻路 | `{"tx","ty"}` | v0.3.13 迁入 |

**新增设计端点（⏳ 实现时探索，结构见 §0.4）**：
- movement：~~move/cancel、walk、walk/stop~~ → **✅ v0.4.1 全部实现**（move/cancel=CHAR_RemovePath 清路径；walk=每帧 CHAR_Move 60 帧；walk/stop=同上清理）
- combat：{role}/config/skill-usage、cast（占位）——**attack/stop 已实现（v0.4.2）**
- inventory：move、sell（静态表价格）、jewel
- character：stat、stat-reset、skill-reset
- party：discharge、withdraw
- npc：interact、dialog/next、dialog/select
- ui：panel/open、panel/close、panel/close-to
- shop：buy（选中+确认，不含开面板）
- quest：quit
- save：save（静默）、load（仅主菜单/选档）
- craft：mix（免 UI）

> `role` = 0..2（出战槽位）。**对话选项触发任务 = 合法**（走游戏 NPC 对话流程，npc/dialog/select）；**不经过 NPC 直接接/交任务 = OP**（/api/op/quest/*）。
> **审查修正（2026-08-05）**：`inventory/sell`（任意定价=刷钱漏洞）原归 OP——**2026-08-08 修正：价格由 ITEMDATABASE 静态表决定（ITEM_GetPrice），非调用方传入，转普通合法 API**；`teleport`（任意切图/瞬移）仍归 OP（/api/op/movement/teleport，force 默认检查目标合法性）。

### 3.2 OP 操作端点（POST /api/op/*，⏳ 未来实现，需 OP 权限）

> v0.3.1 **不暴露 HTTP 端点**（native 函数已实现，见 control-capability.md §5）。未来实现：权限获取机制 + 端点组。
> **2026-08-08 结构定稿**（16 端点，结构见 §0.4）：

| 方法 | 路径 | 操作 | body | 说明 |
|---|---|---|---|---|
| POST | `/api/op/quest/accept` | 接取任务（绕过 NPC） | `{"questId","force":false}` | 默认校验可接性（CheckPrepare）；`force:true` 跳过校验接取任意任务 |
| POST | `/api/op/quest/complete` | 完成任务（无视完成条件） | `{"questId"}` | 绕过 NPC 对话直接完成 |
| POST | `/api/op/character/{role}/experience` | 设置经验 | `{"set":N}` 或 `{"add":N}` | CHAR_SetExperience/AddExperience |
| POST | `/api/op/character/{role}/status-point` | 设置属性点 | `{"points":N}` | CHAR_SetStatusPoint（绕过升级） |
| POST | `/api/op/character/{role}/skill-point` | 设置技能点 | `{"points":N}` | CHAR_SetSkillPoint |
| POST | `/api/op/character/{role}/skill-level` | 设置技能等级 | `{"actionId","level":N}` | CHAR_LearnActionDirect（直调版） |
| POST | `/api/op/party/swap` | 队伍换位 | `{"a","b"}` | PARTY_Swap（游戏内做不到，迁移自 action） |
| POST | `/api/op/inventory/set-slot` | 格子设物品+数量 | `{"bag","slot","itemId","count"}` | ITEMSYSTEM_MakeItem+写入（空格生成，有物替换） |
| POST | `/api/op/inventory/set-equip` | 修改装备属性 | `{"bag","slot"}`+属性参数 | ApplyGrade/ApplyEnchantValue/ApplySocket+结构体直写 |
| POST | `/api/op/inventory/money` | 修改金币 | `{"set":N}` 或 `{"add":N}` | INVEN_SetMoney/AddMoney |
| POST | `/api/op/craft/mix-direct` | 免配方机直合成 | `{"recipeId","resultCount"}` | 跳过材料/配方机位置检查直接生成 |
| POST | `/api/op/combat/{role}/heal` | 回血回蓝 | `{"hp","mp"}` | PARTY_AddHPMP |
| POST | `/api/op/combat/{role}/rest` | 休息恢复 | — | PARTY_ApplyRest（跳过位置限制） |
| POST | `/api/op/combat/{role}/revive` | 复活 | — | 复活链（PARTY_AddHPMP+状态清） |
| POST | `/api/op/combat/{role}/hate` | 仇恨操作 | `{"targetId","value"}` | HATESYSTEM_Add |
| POST | `/api/op/movement/teleport` | 传送/切图 | `{"x","y"}` 或 `{"mapId","x","y","force":false}` | 默认检查（MAP_IsBlockingByPixel+地图范围）；`force:true` 跳过传非法位置 |

### 3.3 事件流（✅ v0.3.0 /api/info/events）

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

## 4. 部署形态差异

| 项 | 手机版（LSPosed 模块） | 服务器版（LSPatch 集成） |
|---|---|---|
| 数据获取 | 同进程 Hook | 同进程 Hook（LSPatch 注入） |
| API 访问 | 监听 0.0.0.0，局域网 Wi-Fi IP | Waydroid NAT，需端口转发 |
| minSdk | Android 11+（Zygisk-LSPosed） | 随游戏 targetSdk 29 |
| 静态数据 | JSON 库随模块打包 | JSON 库随集成 APK 打包 |

## 5. 待确认/待验证项

> 未完成项已统一收录至 `docs/backlog.md`（唯一待办来源），本节仅保留已解决记录。
> 注：以下记录中的旧端点路径（如 /api/info/player、/api/info/gamestate 等）均为当时实现名，v0.3.13 分层重构后已迁移/拆分（见 §0.6 迁移对照表），此处保留历史事实。

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
- [x] 全量 API 审查：sell（任意定价）/teleport（任意传送）/money（直接增减）判定 OP 移除（见 docs/api-technical-spec.md §4.1）
- [x] `/api/info/gamestate` 细粒度游戏状态（v0.3.7：STATE_nPrevState + UIPopupMsg_bOn + UIMainMenu_bDrawFull + g_arrPopupStack）
- [x] `/api/info/snapshot` 快速状态快照（v0.3.7：UI+角色+地图+小队一站式聚合）
