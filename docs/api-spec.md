# API 信息清单与接口规格

> 状态：v0.3，基于需求确认（2026-08-05）。静态数据（M3 ✅）+ 运行时只读端点（M4 ✅ 真机验证 v0.2.15→v0.2.34）
> 均已实现；未实现项见 §4 状态标注与 §7。hook 点详见 `docs/notes/hook-points.md`。

## 1. 概述

模块通过 Hook 游戏进程内 Java 对象，将游戏数据以 REST + JSON 对外提供。信息分两类：

| 类别 | 来源 | 提供方式 |
|---|---|---|
| 运行时状态（动态） | Hook 游戏对象字段 | 实时读取，每次请求取当前值 |
| 静态配置数据（静态） | game_res 一次性解析 | JSON 数据库，模块内嵌或独立提供 |

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
| 文本数据 | 7 语言 35,811 条 | `memorytext_*.dat.jpg`（zh-Hans/zh-Hant/ja/en/de/fr） |
| 全量 100 表 | 全部静态表原始记录 | `game.dat.jpg`（`*BASE_nRecordCount/pData` 对应） |

> 解析说明见 `docs/notes/static-data.md`；产物在 `static-data/json/`。

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

### Role（出战角色，✅ 已实现 /api/player/party 返回数组）

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

### Skills（角色技能，✅ v0.2.23 /api/player/skills）

```json
[
  { "role": 0, "skills": [ { "actionId": 0, "level": 1 }, ... ],
    "unlockBitmap": 65535, "activeSkillId": 0, "skillPoints": 6 },
  null
]
```
来源：角色 +0x2A0 技能链表（节点 action_id/level/next）、+0x2B0 解锁位图、+0x280 当前技能、+0x328 技能点。

### Mercenaries（全部佣兵，✅ v0.2.30-31 /api/player/mercenaries）

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

### Path（寻路，✅ v0.2.33-34 GET /api/path?tx=&ty=）

```json
{ "target": { "x": 200, "y": 360 },
  "start": { "x": 320, "y": 472 },
  "found": true,
  "path": [ { "x": 320, "y": 472 }, ... ] }
```
来源：CHAR_SearchPath(hero, tx, ty, 1) 仅计算存储路径（不触发移动），结果存角色 +0x2F0 PATHLIST 链表
（节点网格坐标 ×8 = 像素坐标）。

### UI（界面状态，✅ v0.2.22 /api/ui）

```json
{ "state": 5, "stateName": "in_game", "inGame": true, "gamestate": 0, "initstate": 2 }
```
`state`：4=主菜单流程、5=游戏中（STATE_nState frida 实测）。

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

> 其余数值字段语义待逆向（当前 48 个字段已验证，见 `field_catalog.json`）。

## 4. 端点设计（✅ 已实现 / ⏳ 未实现）

**运行时端点**

| 方法 | 路径 | 说明 | 数据源 | 状态 |
|---|---|---|---|---|
| GET | `/api/player` | 玩家总览（金币、地图、坐标、队伍、控制角色） | native 直读/函数 | ✅ v0.2.16 |
| GET | `/api/player/party` | 出战角色完整状态（HP/MP/属性/主属性/装备/名称） | `PARTY_GetMember` + 结构体 | ✅ v0.2.29 |
| GET | `/api/player/skills` | 每角色技能链表 + 位图 + 技能点 + 当前技能 | 角色 +0x2A0 链表 | ✅ v0.2.23 |
| GET | `/api/player/mercenaries` | 全部佣兵（含未上场：槽/名称/等级/坐标） | MERCENARYSYSTEM 槽数组 + FindAsMercenarySlot | ✅ v0.2.31 |
| GET | `/api/inventory` | 背包物品列表（属性 + 名称 + 容量） | `INVEN_pItem` 槽数组 | ✅ v0.2.32 |
| GET | `/api/map` | 地图 ID + 玩家坐标 | `MAP_nBaseInfo` + 角色位置 | ✅ v0.2.11 |
| GET | `/api/quest` | 当前激活任务 ID | `QUESTSYSTEM_nActiveQuest` | ✅（待任务实测） |
| GET | `/api/units` | 场景单位（敌人/NPC + 坐标 + 类型/状态） | CHARSYSTEM 角色池 | ✅ v0.2.21 |
| GET | `/api/path?tx=&ty=` | 路径计算（引擎 A*，参数为目标像素坐标） | CHAR_SearchPath + PATHLIST | ✅ v0.2.34 |
| GET | `/api/events` | 事件流：战斗/拾取/升级 | Hook 事件回调 | ⏳ 未实现 |
| GET | `/api/ui` | 当前 UI 界面状态 | STATE_nState/GAMESTATE | ✅ v0.2.22 |

**静态数据端点（✅ 数据已就绪，模块内嵌 JSON 库）**

| 方法 | 路径 | 说明 | 数据源（static-data/json/） |
|---|---|---|---|
| GET | `/api/data/roles` | 职业 + 属性成长 + 等级上限 | `tables/CHARCLASSBASE`、`ATTRINITBASE`、`MAXLEVELBASE` |
| GET | `/api/data/items` | 道具/装备配置（含名称/稀有度/描述） | `tables/ITEMDATABASE`、`ITEMRARITYGRADEBASE`、`ITEMDESCBASE` |
| GET | `/api/data/skills` | 技能表（含名称/训练） | `tables/SKILLDESCBASE`、`SKILLTRAINBASE` |
| GET | `/api/data/mercenaries` | 佣兵 + 技能 | `tables/MERCENARYINFOBASE`、`MERCENARYSKILLBASE` |
| GET | `/api/data/maps` | 地图 ID/名称 + 传送点 | `tables/MAPINFOBASE`、`PORTALINFOBASE` |
| GET | `/api/data/monsters` | 怪物数值/技能/掉落 | `tables/MONDATABASE`、`MONSKILLBASE`、`MONSTERDROPBASE` |
| GET | `/api/data/quests` | 任务 + 奖励 | `tables/QUESTINFOBASE`、`QUESTREWARDBASE` |
| GET | `/api/data/npcs` | NPC 信息 | `tables/NPCINFOBASE`、`NPCDESCBASE` |
| GET | `/api/data/text?lang=zh-Hans` | 全量文本（7 语言） | `text/<lang>.json` |
| GET | `/api/data/events` | 剧情事件（命令/条件/文本） | `reverse/events.json`、`event_conditions.json` |
| GET | `/api/data/tables/{name}` | 任意原始表（100 张） | `tables/<NAME>.json` |

**静态数据响应示例**（GET `/api/data/items?query=治疗`）：
```json
{
  "items": [
    { "itemId": 1, "name": "治疗药水", "rarity": 0, "description": "恢复少量生命值。" }
  ]
}
```

预留扩展：`/api/player/party/{index}` 单角色详情、`/api/inventory/{itemId}` 单道具。

### 4.1 操作端点（写入/控制，可行性详见 `docs/notes/control-capability.md`）

| 方法 | 路径 | 操作 |
|---|---|---|
| POST | `/api/player/money` | 增减金币（body: delta） |
| POST | `/api/player/{index}/experience` | 增减经验/升级 |
| POST | `/api/player/{index}/equip` | 穿/脱装备（body: itemId + slot） |
| POST | `/api/player/{index}/skill` | 使用/学习技能 |
| POST | `/api/player/switch` | 切换主控角色 |
| POST | `/api/inventory/move` | 背包移动/整理 |
| POST | `/api/inventory/remove` | 删除物品 |
| POST | `/api/teleport` | 传送（地图 + 坐标） |
| POST | `/api/save` | 触发存档 |

> 写操作经 `PushMainThreadEvent` 投递到游戏主线程执行；操作后返回最新状态。函数签名逆向是主要工作量。

### 稀有度（rarity）取值

装备稀有度 5 档：`0=白`、`1=绿`、`2=蓝`、`3=黄（特殊装备）`、`4=紫`（最终映射以 `ITEMRARITYGRADEBASE` 解析结果为准，M3 确认）。

## 5. Hook 点 / 解析点映射（已实测验证）

> 详细分析见 `docs/notes/hook-points.md`。**读取方式 = 模块 native 层 base+VMA 直读全局 / 调用 Getter**
> （不用 dlopen/dlsym：Android linker namespace 隔离会加载独立副本读不到数据）。

| 信息 | 数据源（VMA） | 方式 | 状态 |
|---|---|---|---|
| 金币 | `INVEN_nMoney`（0x7134c0）/ `INVEN_GetMoney` | 函数调用 | ✅ 实时（真机实测） |
| 实时地图 ID | `MAP_nBaseInfo+0`（0x713878, u16） | 直读 | ✅ 实时（切图实测变动） |
| 存档地图 ID | `SAVE_nMapID`（0x729824） | 直读 | ⚠️ 存档字段（保存才同步） |
| 实时玩家坐标 | 角色结构体 `+0x02`(x) / `+0x04`(y) int16 | 结构体读取 | ✅ 实时（CHAR_GetDistance 证实） |
| 相机焦点 | `MAP_nFocusBX/BY`（0x724d08/0x726e68） | 直读 | ⚠️ 非玩家位置（弃用） |
| 等级 | 角色结构体 `+0x0E` int8 | 结构体读取 | ✅ |
| 经验 | 角色结构体 `+0x318`(int64) / `+0x320`(升级所需) | 结构体读取 | ✅ |
| HP/MP | 角色结构体 `+0x1F0` / `+0x1F4`；上限=`CHAR_GetAttr(char,0x1e/0x1f)` | 结构体+函数 | ✅ |
| 属性 | 角色结构体 `+0x24 + attr_id*4` | 结构体读取 | ✅ |
| 装备 | `CHAR_GetEquipItem(char, slot)`（10 槽，物品 `+0x08` 类型位域） | 函数调用 | ✅ |
| 技能 | 角色 +0x2A0 已学技能链表（节点 action_id/level/next）+ +0x2B0 位图 + +0x328 技能点 | 结构体读取 | ✅ v0.2.23 |
| 背包 | `INVEN_pItem`（0x7131c0 槽数组：6袋×0x80，每槽8B 指针）+ `INVEN_GetBagSize` | 直读+函数 | ✅ v0.2.32 |
| 队伍 | `PARTY_pChar`（0x728ec0）/ `PARTY_GetMember(i)` / `PARTY_GetSize` | 直读/函数 | ✅ |
| 任务 | `QUESTSYSTEM_nActiveQuest`（0x728ff8） | 直读 | ✅（接任务后待实测） |
| 单位/敌人坐标 | CHARSYSTEM 角色池 `*(0x307538)`（0x430/对象，类型 +0x09、状态 +0x311） | 池直读 | ✅ v0.2.21 |
| UI 状态 | `STATE_nState`（0x307492：4=主菜单 5=游戏中）+ `GAMESTATE_nState`(0x72b068) + `INITSTATE_nState`(0x72b06d) | 直读 | ✅ v0.2.22 |
| 主属性 | `CHAR_GetStat(char, 0..4)`（0xdf8d0：力量/敏捷/体力/智力/精力）；`CHAR_GetStatusPoint`(0xd9c44)=能力点 | 函数调用 | ✅ v0.2.29 |
| 角色名称 | `CHAR_GetName(char)`（0xd9c54，返回 UTF-8 字符串） | 函数调用 | ✅ v0.2.31 |
| 全部佣兵 | 槽数组 `*(*(0x2f6010))`（20B/槽）+ `CHARSYSTEM_FindAsMercenarySlot`(0xf4254) | 直读+函数 | ✅ v0.2.31 |
| 路径 | `CHAR_SearchPath(char,tx,ty,1)`（0xdb094）+ 角色 +0x2F0 PATHLIST 链表 | 函数+结构体 | ✅ v0.2.34 |
| 物品名称 | category=(typeFlags>>6)&0x3FF = ITEMDATABASE itemId（静态表 text_0 联查） | Kotlin 静态联查 | ✅ v0.2.25 |
| 地图通行矩阵 | `MAP_nBaseTile`（0x7148a8）+ `MapBlockingcheck` | 直读 + 函数 | ⏳ 瓦片编码待逆向 |
| game_res 静态数据 | assets/common/game_res/*.dat.jpg | M3 解析 | ✅ `static-data/json/` |

## 6. 部署形态差异

| 项 | 手机版（LSPosed 模块） | 服务器版（LSPatch 集成） |
|---|---|---|
| 数据获取 | 同进程 Hook | 同进程 Hook（LSPatch 注入） |
| API 访问 | 监听 0.0.0.0，局域网 Wi-Fi IP | Waydroid NAT，需端口转发 |
| minSdk | Android 11+（Zygisk-LSPosed） | 随游戏 targetSdk 29 |
| 静态数据 | JSON 库随模块打包 | JSON 库随集成 APK 打包 |

## 7. 待确认/待验证项

**已解决**：
- [x] game_res 静态数据提取（✅ M3：100 表 + 7 语言文本 + 事件/SNASYS，见 `docs/notes/static-data.md`）
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
- [x] /api/units 单位坐标（CHARSYSTEM 池枚举，v0.2.19-21）
- [x] /api/ui UI 界面状态（STATE_nState，v0.2.22）
- [x] /api/path 寻路（CHAR_SearchPath + PATHLIST，v0.2.33-34）
- [x] 召唤物识别（units 含 status=2 怪物/召唤物，类型字段已逆向）

**待实现/待确认**：
- [ ] `/api/events` 事件流（战斗/拾取/升级，Hook 事件回调）
- [ ] 操作端点（POST 写操作，见 §4.1，需 PushMainThreadEvent 逆向）
- [ ] 动态背包袋真机验证（装备/卸下背包袋对比 capacity）
- [ ] /api/path 真机验证（v0.2.34 待真机确认）
- [ ] activeQuest 接任务后实测
- [ ] 静态表数值字段语义全逆向（当前 48/100 表已验证核心字段）
- [ ] 地图通行矩阵（MAP_nBaseTile 瓦片编码）
