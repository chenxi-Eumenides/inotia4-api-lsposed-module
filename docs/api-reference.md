# API 参考手册

> **本文档 = API 规格（面向调用方）**：每个 API 的路径、用途、请求格式、返回格式与注意事项。
> 技术实现细节（VMA/函数签名/调用链/游戏内机制）见 `docs/api-technical-spec.md`。
> 状态：**v0.5.4**。character（第二章）、world（第三章）、item（第四章）域为设计草案、代码待实现；其余域端点路径已对照 controller 真实路由逐条核对。
>
> 通用约定：
> - 服务地址：`http://<设备IP>:8088`（局域网，模块监听 0.0.0.0）
> - 请求/响应均为 JSON；写操作（POST）的 body 是 JSON 字符串
> - `role` = 出战槽位 0..2；`bag` = 背包袋 0..5；`slot` = 袋内槽位 0..15
> - 写操作成功返回 `{"ok":true,...}`；失败返回 `{"ok":false,"error":"<原因>"}`
> - native 未就绪（模块初始化中）时所有端点返回 `{"error":"not ready"}`
> - 写操作返回会附带 `state` 字段 = 操作后的最新状态快照（类型见各端点说明）

---

## 0. 分组总览

> API 按游戏实体/领域分 7 组，不再按读写性质划分（info/data/action 前缀已废弃）：
> 每组内 GET（读）与 POST（写）混合，读写同域，GET/POST 由 HTTP 方法区分。OP（越权操作）独立保留。
>
> **character 域端点按实体化重设计**（第二章为设计草案，代码待实现；其余域为现状）。

| 分组 | 顶层路径 | 覆盖实体 | 端点数 |
|---|---|---|---|
| **character**（角色与队伍） | `/api/character/*` | 出战角色 party、佣兵 mercenary、战斗 combat、角色成长 grow | 29 |
| **world**（地图与移动） | `/api/world/*` | 当前地图 map、移动操作 movement、静态地图 maps（含瓦片矩阵） | 15 |
| **item**（物品与背包） | `/api/item/*` | 背包 inventory、商店 shop | 16 |
| **quest**（任务） | `/api/quest/*` | 任务 | 6 |
| **ui**（界面与对话） | `/api/ui/*` | 界面状态 ui、对话 dialog | 17 |
| **system**（系统与会话） | `/api/system/*` | 健康 health、游戏整体 game、事件流 events、存档 save、静态数据表 tables、多语言文本 text、剧情事件 story-events | 16 |
| **op**（越权操作） | `/api/op/*` | 改数据/强行操作（需独立权限，默认关闭） | 6 已实现 + 15 定稿 |
| debug（调试） | `/api/debug/*` | 开发期调试 | 1 |

> 全量端点 = 106（character 29 + world 15 + item 16 + quest 6 + ui 17 + system 16 + op 6 + debug 1；其中 GET 60 / POST 46）。

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

> 字段设计：战斗属性以属性名直写字段（`max_hp` 而非 `"30"`）、单值属性聚合为 `status`、装备带位置、技能带名称与最大等级。

```json
{
  "id": 1, "id_name": "忍者",
  "name_id": 2210, "name": "凯恩",
  "level": 27,
  "status": {
    "hp": 10598, "max_hp": 10664,
    "mp": 200, "max_mp": 250,
    "exp": 12000, "exp_next": 15000,
    "skill_points": 6,
    "attribute_points": 78
  },
  "stats": {
    "crit_rate": 60, "crit_damage": 400, "attack": 300,
    "magic_attack": 150, "dexterity": 139, "defense": 200,
    "wdr": 100, "max_hp": 10664, "max_mp": 212
  },
  "main_stats": { "力量": 96, "敏捷": 139, "体力": 101, "智力": 54, "精力": 38 },
  "equipment": [
    { "slot": 0, "position": "head", "type_flags": 21352, "category": 333, "rarity": 3,
      "damage": 0, "defense": 37, "magic_rate": 0, "socket": 69, "enchant": 35072,
      "options": [1, 1, 1, 18, 23, 17, 19, 36], "name": "光荣的火冠" },
    null
  ],
  "skills": [
    { "action_id": 3, "name": "血之复仇", "level": 1, "max_level": 10 },
    { "action_id": 5, "name": "致命一击", "level": 3, "max_level": 10 }
  ],
  "unlock_bitmap": 65535,
  "active_skill_id": 3
}
```

| 字段 | 说明 |
|---|---|
| `id` / `id_name` | 角色类型（职业索引 0-5）/ 职业名（CHARCLASSBASE 联查：黑暗骑士/忍者/黑魔导/祭司/暗影猎手/狂战士） |
| `name_id` / `name` | 角色名字文本 ID / 角色名（CHAR_GetName） |
| `level` | 等级 |
| `status` | 状态聚合：血量/魔力/经验/下一级经验/技能点/能力点 |
| `stats` | 战斗属性聚合（角色 +0x24 数组 32 项，以**属性名**为字段名，不用数字 id）。✅ v0.5.1 已确认 15 项：`crit_rate`/`crit_damage`/`attack`/`magic_attack`/`magic_resist`(11)/`dexterity`/`hit_base`(14)/`hit_rate`(15)/`defense`/`phys_reduce`(18)/`wdr`/`sub_weapon_attack`(20)/`level_attr`(28)/`max_hp`/`max_mp`；**其余 12 项占位 attr_<id>**（详见第二章 stats 端点） |
| `main_stats` | 主属性对象（0-4=力量/敏捷/体力/智力/精力，总属性=基础+已分配+加成），以属性名为键 |
| `equipment` | 10 装备槽数组（每件含 `slot`/`position` 位置名 + 物品属性 + `name` 联查），空槽为 `null`；位置映射见第二章 |
| `skills` | 技能列表（每项含 `name` 技能名、`level` 当前等级、`max_level` 最大等级） |
| `unlock_bitmap` | 已解锁技能位图（+0x2B0） |
| `active_skill_id` | 当前装备技能（+0x280） |

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
- 附加字段：`option_ids`/`options` 词缀 ID 与值、`socket_filled`/`socket_total` 宝石孔、`enchant_id`/`enchant_level`/`chaos` 附魔、`chaos_level`/`chaos_rate` 混沌

### Skills（角色技能）

> 直接提供技能列表，每项技能带名称与最大等级。

```json
{
  "skills": [
    { "action_id": 3, "name": "血之复仇", "level": 1, "max_level": 10 },
    { "action_id": 5, "name": "致命一击", "level": 3, "max_level": 10 }
  ],
  "unlock_bitmap": 65535,
  "active_skill_id": 3
}
```

- 来源：技能链表（`[ch+0x2A0]`，节点 `action_id`/`level`）、+0x2B0 解锁位图、+0x280 当前技能
- `name` 由 SKILLDESCBASE 联查（该表 `[0]`=action_id、`[3]` 起为技能名文本）
- `max_level` **不在链表节点、不在 SKILLDESCBASE**：来源为技能信息表 `*(0x2f4000+0x9e0)` 记录 +0x1D → 角色 +0x2B2 打包 nibble 数组解码（`CHAR_GetActMaxLevel` 0xe9560）。**等级规则**：常规最高 **4 级**，使用技能书（CHAR_ProcessSkillBook 0xe2488）可提升至最高 **8 级**。**API 当前未输出该字段，待实现**

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

> ⚠️ **本节为设计草案（文档先行，代码待实现）**。实现后按本节验收，本节端点与当前代码不一致属预期。

**角色实体全生命周期**：出战角色（party）状态与操控、佣兵（mercenary）管理、战斗行为（combat）、角色成长（grow）。

**命名约定**：
- POST 动作用「动词+宾语」两词组合命名（如 `set_auto_attack`、`cast_skill`）
- 静态数据中有名称的字段一律注入名称（职业名 `id_name`、角色名 `name`、装备名、属性名、技能名）
- 单值端点「请求什么，返回的主字段就是什么」，可附加名称字段（如 `id` + `id_name`）

### 2.1 出战角色 party

#### 出战角色复合

`GET /api/character/party`

**用途**：获取 3 槽出战角色完整状态（含职业名/装备名/技能名注入）。

**返回格式**：`[ <Role>, <Role>, ... ]`（Role 模型数组，空槽为 `null`）

#### 出战人数

`GET /api/character/party/count`

**用途**：获取出战人数。

**返回格式**：`{ "count": 2 }`

#### 主控角色

`GET /api/character/leader`

**用途**：获取当前主控角色（转发到 party 中正在操控的那个角色，`main_mercenary_slot` 对应出战槽）。

**返回格式**：`<Role 模型>` 或 `{"error":"not found"}`


#### 指定出战槽

`GET /api/character/party/{slot}`

**用途**：获取指定出战槽（0..2）完整状态。

**返回格式**：`<Role 模型>` 或 `{"error":"not found"}`

#### 角色类型

`GET /api/character/party/{slot}/id`

**用途**：获取指定出战槽角色类型（职业索引）。

**返回格式**：`{ "id": 1, "id_name": "忍者" }`

**注意**：返回 `id`（职业索引 0-5）+ `id_name`（职业名，CHARCLASSBASE 记录 +0x00 文本联查：0 黑暗骑士 / 1 忍者 / 2 黑魔导 / 3 祭司 / 4 暗影猎手 / 5 狂战士）。

#### 角色名

`GET /api/character/party/{slot}/name`

**用途**：获取指定出战槽角色名。

**返回格式**：`{ "name": "凯恩" }`

#### 等级

`GET /api/character/party/{slot}/level`

**用途**：获取指定出战槽等级。

**返回格式**：`{ "level": 27 }`

#### 状态聚合

`GET /api/character/party/{slot}/status`

**用途**：获取指定出战槽基本状态聚合（血量/魔力/经验/技能点/能力点）。

**返回格式**：

```json
{
  "status": {
    "hp": 10598, "max_hp": 10664,
    "mp": 200, "max_mp": 250,
    "exp": 12000, "exp_next": 15000,
    "skill_points": 6,
    "attribute_points": 78
  }
}
```

#### 战斗属性聚合

`GET /api/character/party/{slot}/stats`

**用途**：获取指定出战槽全部战斗属性聚合（一次取全量）。

**返回格式**：

```json
{
  "stats": {
    "crit_rate": 60, "crit_damage": 400, "attack": 300,
    "magic_attack": 150, "dexterity": 139, "defense": 200,
    "wdr": 100, "max_hp": 10664, "max_mp": 212
  }
}
```

**属性名字段映射**（角色 +0x24 数组 32 项，以属性名为字段名；✅ v0.5.1 实机 frida 实测补全）：

| 字段 | 属性 id | 说明 |
|---|---|---|
| `crit_rate` | 0 | 暴击率 ×10（默认 30） |
| `crit_damage` | 3 | 暴击伤害 ×10（默认 1000） |
| `attack` | 4 | 攻击（力量×0.5-0.7 + 敏捷×0.5 + 主手武器） |
| `magic_attack` | 8 | 魔攻（智力/精力×0.6-1.0 + 主手武器） |
| `magic_resist` | 11 | **魔法抵抗**（面板 M.RES 35193；clamp 750=75.0%） |
| `dexterity` | 13 | 敏捷（总，=主属性敏捷） |
| `hit_base` | 14 | **命中率基数**（CHAR_GetHitRate1000 加数；默认 0，LV1 黑魔导 80） |
| `hit_rate` | 15 | **命中率百分比**（敏捷×4+精力×4） |
| `defense` | 17 | 防御（体力×1 + 装备） |
| `phys_reduce` | 18 | **物理减伤系数**（P.RES=attr17×(attr18+1000)/10000） |
| `wdr` | 19 | 武器伤害减免率 ×10（默认 30） |
| `sub_weapon_attack` | 20 | 副手武器攻击（slot6） |
| `level_attr` | 28 | 等级驱动属性 =(960+36×等级)/10（黑魔导；职业各异） |
| `max_hp` | 30 | HP 上限（CHAR_GetAttr ch,0x1e；=640+72×(等级+10) 黑魔导） |
| `max_mp` | 31 | MP 上限（CHAR_GetAttr ch,0x1f；默认 200） |
| `total_attack` | 113 | 总攻击 = max(物攻,魔攻)（扩展 id，不在 32 项数组） |

**属性公式表**（CHAR_UpdateAttrFromStat 映射，frida 实测）：力量→攻击(a×700/600/500÷1000，职业条件 4/2/3/5/6/18/1)、敏捷→攻击(a×500÷1000)+命中(a×4)+敏捷(a×1)、体力→HP上限(a×80)+防御(a×1)、智力→魔攻(a×600/700/1000÷1000，条件 5/6/16)、精力→魔攻(a×600/600/1000÷1000)+命中(a×4)。

**注意**：其余 id（1,2,5,6,7,9,10,12,16,21,22-27）仍输出 `attr_<id>` 占位（LV1 实测全 0；21 默认 1000、16 默认 8、29 默认 8 来源未定性，待高等级/词缀样本）。⚠️ 不可直接套用 ITEMOPTINFOBASE 词缀属性编码（如 MP增加 编码 31，而 stats[31] 是 MP上限）——该编号与 stats 数组索引不一致。

#### 装备列表

`GET /api/character/party/{slot}/equipment`

**用途**：获取指定出战槽装备列表（每件含位置与名称注入）。

**返回格式**：`{ "equipment": [ <Item 对象>, null, ... ] }`（10 槽，空槽 `null`）

**装备位置映射**（每件装备含 `position` 字段。依据：CHAR_FindEquipSlot(0xe4fd0) 反汇编 + ITEMCLASSBASE 记录 +4 字节槽位表）：

| slot | position | 位置 | 物品类别（ITEMCLASSBASE 类名） |
|---|---|---|---|
| 0 | `head` | 头部 | 帽子/头盔/头环/黄金之冠 |
| 1 | `glove` | 护手 | 护手 |
| 2 | `cloak` | 斗篷 | 斗篷 |
| 3 | `armor` | 铠甲 | 布甲/皮甲/板甲 |
| 4 | `boots` | 鞋 | 鞋 |
| 5 | `weapon1` | 武器·主手 | 短剑/长剑/斧头/钝器/双手武器/水晶球/法杖/弓/弩 |
| 6 | `weapon2` | 武器·副手 | 盾牌；忍者/狂战士（职业 1/5）双持副手武器 |
| 7 | `necklace` | 项链 | 项链 |
| 8 | `ring` | 戒指 | 戒指 |
| 9 | `unused` | 未使用 | 无任何装备类映射（保留槽） |

**注意**：游戏只分配 1 个戒指槽（slot 8），无第二戒指槽；槽位由静态表驱动而非固定约定，slot 9 预留无装备可穿。

#### 指定装备槽

`GET /api/character/party/{slot}/equipment/{equip_slot}`

**用途**：获取指定出战槽指定装备（0..9）。

**返回格式**：`<Item 对象>`（含 position/name）或 `null`

#### 技能列表

`GET /api/character/party/{slot}/skills`

**用途**：获取指定出战槽技能列表。

**返回格式**：

```json
{
  "skills": [
    { "action_id": 3, "name": "血之复仇", "level": 1, "max_level": 10 },
    { "action_id": 5, "name": "致命一击", "level": 3, "max_level": 10 }
  ],
  "unlock_bitmap": 65535,
  "active_skill_id": 3
}
```

**注意**：`skill_points` 已移入 status 聚合；`name` 由 SKILLDESCBASE 联查；`max_level` 来源为技能信息表 `*(0x2f4000+0x9e0)` 记录 +0x1D → 角色 +0x2B2 打包 nibble 数组解码（CHAR_GetActMaxLevel 0xe9560），常规最高 4 级、技能书可提升至 8 级，**API 当前未输出该字段，待实现**。

#### 佣兵入队

`POST /api/character/party/include`

**用途**：把待命佣兵编入出战队伍（MERCENARYSYSTEM_IncludeParty）。

**请求格式**：`{ "mercenary_slot": 1 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：已在队→`already in party`；满员→`party full`。⚠️ 参数索引见 2.2「两套索引」说明。

#### 佣兵离队

`POST /api/character/party/exclude`

**用途**：把出战佣兵移回待命（MERCENARYSYSTEM_ExcludeParty）。

**请求格式**：`{ "mercenary_slot": 1 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：主控→`cannot exclude leader`；任务 NPC→`cannot exclude quest npc`。

#### 佣兵遣散

`POST /api/character/party/discharge`

**用途**：遣散佣兵（MERCENARYSYSTEM_Release）。

**请求格式**：`{ "mercenary_slot": 27 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：无该槽角色→`mercenary not found`；主控/任务 NPC→不可遣散。

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

#### ⚠️ 两套索引说明

`GET /mercenary*` 端点返回的 `slot` 与 `POST /party/include|exclude|discharge|withdraw` 请求中的 `mercenary_slot` **不是同一套索引**：

| 概念 | 值示例 | 来源 |
|---|---|---|
| 读端点 `slot`（槽数组索引） | 27/32/58 | 佣兵槽数组 `MERCENARYSYSTEM_pSlotList` 下标（每槽 20B 记录，G_MERC_SLOTLIST_GOT_VMA） |
| 写参数 `mercenary_slot`（角色槽 ID） | 0/1/255 | 角色对象 +0x352 字段（角色池中标记归属槽） |

**为什么会这样**：游戏内部有两套并行的佣兵管理机制——佣兵**记录**存于槽数组（管理占用/在队状态），佣兵**角色对象**在角色池中用 +0x352 字段标记归属槽。API 读端点直接暴露槽数组下标，写端点则透传角色槽 ID 参数（凯恩 +0x352=0，其余角色 +0x352=255 无效），两套编号自然对不上。

**如何改进（待实现）**：API 层统一为「槽数组索引」一套编号——写操作也接收读端点返回的 `slot`，模块内部完成 槽数组索引 ↔ +0x352 槽 ID 的映射转换后再调用底层；`mercenary_slot` 参数改名 `slot` 与读端点对齐。

### 2.3 战斗 combat

> 战斗动作统一「动词+宾语」两词命名：`set_auto_attack` / `set_skill_usage` / `switch_player` / `cast_skill` / `attack_target` / `stop_combat`。

#### 切换主控

`POST /api/character/combat/switch_player`

**用途**：切换主控角色到指定出战槽（PARTY_SetActivePlayer 0x11f584）。

**请求格式**：`{ "slot": 1 }`（目标出战槽 0/1/2）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：目标槽越界/不可切换→`switch failed`；主控切换后 `/api/character/leader` 返回相应角色。

#### 自动反击开关

`POST /api/character/combat/{role}/set_auto_attack`

**用途**：开关指定出战槽的**自动反击**（CHAR_SetAutoAttack 写 [ch+0x3a0] bit7-10）。

**请求格式**：`{ "on": true }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：⚠️ 此开关只控制「**被怪物攻击后的自动反击**」——角色受击后自动还击；**不是**自动攻击附近怪物的挂机行为，角色不会主动索敌。开启后仍需先通过 `attack_target` 进入战斗。

#### 技能使用开关

`POST /api/character/combat/{role}/set_skill_usage`

**用途**：设置指定出战槽**单个已学技能**是否被战斗 AI 自动使用及其使用频率（每个技能独立设置）。

**请求格式**：`{ "action_id": 3, "mode": "normal" }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**mode 三档**：`off` 不使用 / `normal` 一般频率 / `high` 高频率

**注意**：`action_id` 必须为已学技能（未学→`skill not learned`）；`off` 等价于关闭该技能自动使用。底层为 CHAR_SetSkillUsage 写 [ch+0x3a0] bit0-2 总开关 + 技能链表节点 +0x07 单技能 AI 等级（待需）。

#### 释放技能

`POST /api/character/combat/{role}/cast_skill`

**用途**：指定出战槽立即释放技能（CHAR_GetEnemyTarget + CHAR_SetActionID）。

**请求格式**：`{ "action_id": 5 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：⚠️ `action_id` 必须是**技能**——不能传普攻的 action_id，传入非技能 action_id 返回错误；未学技能→`skill not learned`；无目标→`no target`。

#### 攻击目标

`POST /api/character/combat/{role}/attack_target`

**用途**：指定出战槽**进入战斗状态并对指定目标开始自动普攻**（CHAR_SetTarget + CHAR_MakeDefaultAttack）。

**请求格式**：`{ "target_slot": 5 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：目标无效→`target not found`；缺参→`target_slot required`。此操作使角色进入战斗姿态，随后持续普攻该目标直至目标死亡/离开/`stop_combat`。

#### 停止战斗

`POST /api/character/combat/{role}/stop_combat`

**用途**：停止指定出战槽战斗（CHAR_StopCombat）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：非战斗态调用安全（清标志幂等）。

### 2.4 角色成长 grow

> grow 下直接四个动词动作：`add_skill` / `add_stat` / `reset_skill` / `reset_stat`。

#### 技能加点

`POST /api/character/grow/add_skill`

**用途**：主角学习/升级技能（CHAR_LearnAction，消耗技能点）。

**请求格式**：`{ "action_id": 3, "level": 1 }`

**返回格式**：`{"ok":true,"state":<Skills 模型>}`

**注意**：主角专用，无 role 路径段。

#### 属性加点

`POST /api/character/grow/{role}/add_stat`

**用途**：指定出战槽分配属性点（属性+1/能力点-1，StatDivide 语义）。

**请求格式**：

```json
{ "attrs": { "力量": 1, "敏捷": 2 } }
```

`attrs` 为字典：字段名=主属性名（力量/敏捷/体力/智力/精力），值为分配数量。

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：⚠️ 分配数量只能为正整数；各属性分配数量**总和不能超过剩余能力点**，否则报错（`no status point`）；属性名非法→`bad attr`。

#### 属性重置

`POST /api/character/grow/reset_stat`

**用途**：重置主角分配属性（CHAR_InitializeStatus：分配点归零+能力点按公式还原）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：⚠️ 只能对主角使用，无 role 路径段。

#### 技能重置

`POST /api/character/grow/reset_skill`

**用途**：重置主角技能（CHAR_InitializeSkill：移除非基础技能+技能点还原）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：⚠️ 只能对主角使用，无 role 路径段。

### 2.5 待补齐数据（逆向/探索缺口）

> 本节罗列 character 域设计草案中**尚缺的数据**：实现上述端点前需补齐的逆向结论与代码能力。按「运行时逆向 / 静态数据资产 / 文档修正」三类组织。

#### 运行时逆向缺口

> ✅ **v0.5.1 实机 frida 验证已全部闭环**（2026-08-13 真机2），详细证据见 `docs/research/character-data-gaps.md` 实机验证章节。

| # | 缺口 | 状态与结论 |
|---|---|---|
| R1 | stats 属性名 22 项 | ✅ 已确认 15 项（见第二章属性映射表：新增 11 魔法抵抗/14 命中基数/15 命中率/18 物理减伤/20 副手攻击/28 等级驱动），其余 12 项 LV1 实测全 0 暂占位 |
| R2 | skill max_level 权威来源 | ✅ 技能信息表 `*(0x2f4000+0x9e0)` 记录 +0x1D int16（负=无）→ 表2 `*(0x2f3758)` +9 角色偏移 → `[ch+0x2B2]` **bit1-4**（4 位）= 最终 max_level（常规 4 / 技能书 8，实测切换） |
| R3 | set_skill_usage 单技能档位编码 | ✅ 修正：`CHAR_SetSkillUsage` 写 [ch+0x3a0] **bit0-3**（非 bit0-2）；技能链表节点 +0x07=1 恒为激活标志，**native 无单技能档位**，只有全局位域 |
| R4 | merc 两套索引映射规则 | ✅ 槽数 = **21**（data-sources 88 为 GOT 双层误读）；读=槽数组下标、写=+0x352，经 CHARSYSTEM_FindAsMercenarySlot(0xf4254) 匹配 |
| R5 | 装备槽位表运行时验证 | ✅ 实机确认：基础法杖→主手槽5、漆黑之皮甲→身体槽3，与 equipment 数组一致；槽位 = ITEMCLASSBASE+2 → 槽位表+4 |

#### 静态数据资产缺口

| # | 缺口 | 状态与结论 |
|---|---|---|
| S1 | ITEMOPTINFOBASE.json 未打包 | ✅ **已修复并复验**（v0.5.1）：`package_assets.py` INCLUDE_TABLES 加 ITEMOPTINFOBASE → 重打包 → option_names 非空（实测 `["敏捷","体力","瞬间恢复","武器格挡率"]` 等） |
| S2 | className 联查缺失 | ✅ 实现依据：CHARCLASSBASE u16[0]=职业名 text_id=class_idx×2；StaticData.kt 待实现 `className()` |
| S3 | skillName / skillMaxLevel 联查缺失 | ✅ 权威路径修正（非 SKILLDESCBASE）：技能信息表 recN↔action N，技能名 = rec+0 u16 text_id（=1220+rec），max_level = 角色 +0x2B2 bit1-4；StaticData.kt 待实现 |
| S4 | 佣兵名联查缺失 | ✅ 实现依据：MERCENARYINFOBASE +0x04=佣兵名 text_id（35752+idx）；name=null 槽成因 = 无联查函数 |

#### 文档修正项

| # | 缺口 | 状态与结论 |
|---|---|---|
| D1 | static-data.md §7.2 职业名字段错误 | ✅ 修正：+0x00=职业名（u16[0]，text_id=class_idx×2），已修 static-data.md |
| D2 | backlog L63「修复 buildOptionNames 恒空」与资产包矛盾 | ✅ 由 S1 修复消除（v0.5.1 打包 ITEMOPTINFOBASE） |
| D3 | backlog merc 两套索引条目对齐 | ✅ 槽数 21 实测确认，data-sources 88 误读已修正 |

---

## 三、world（地图与移动）— GET/POST /api/world/*

**空间实体**：当前地图感知（map）、移动操作（movement）、静态地图查询（maps，含静态瓦片矩阵）。位置感知与位置操作一体。

**命名约定**：POST 动作用「动词+宾语」两词命名（`move_to`/`walk_dir`/`stop_move`/`interact_with`）；静态数据中有名称的字段注入名称（地图名）。

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
- `exits` 出口区域（瓦片网格 bit7=1，切图用）；`map_data` 静态信息
- `units` 仅取一次本地复用（惰性缓存下重复调用会多次触发刷新）

#### 地图 ID

`GET /api/world/map/id`

**用途**：获取当前地图 ID 与地图名。

**返回格式**：`{ "id": 30, "id_name": "影子丛林1" }`（`id_name` 由 MAPINFOBASE 联查，与 character `id`+`id_name` 规则一致）

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

**注意**：`distance` 玩家 BFS 可达距离 / `nearest_distance` 不可达时最近距离。

#### 敌人/召唤物

`GET /api/world/map/enemies`

**用途**：units 过滤 status==2（保留为过滤视图）。

**返回格式**：`{ "units": [ ... ] }`

#### 城镇 NPC/佣兵

`GET /api/world/map/interactives`

**用途**：units 过滤 status==1（保留为过滤视图）。

**返回格式**：`{ "units": [ ... ] }`

#### 掉落物

`GET /api/world/map/drops`

**用途**：掉落物列表。

**返回格式**：`{ "drops": [] }`

**注意**：数据源未探索，恒返回空数组（⏳ 占位，见 backlog P2 掉落物条目）。

#### 寻路距离

`GET /api/world/map/distance?tx=600&ty=400`

**用途**：玩家→目标的 BFS 距离（只计算不移动）。

**返回格式**：
```json
{ "target": { "x": 600, "y": 400 }, "start": { "x": 40, "y": 168 },
  "in_map": true, "found": true, "distance": 72, "nearest": null }
```

### 3.2 移动操作 movement

> 动作统一「动词+宾语」两词命名。`move_to`/`walk_dir` 均为**正常走路**（后台线程逐帧移动，非瞬移），到达出口区域自动切图。

#### 寻路移动

`POST /api/world/movement/move_to`

**用途**：寻路移动玩家到目标像素坐标（自研 BFS 导航，后台线程逐帧移动，正常走路非瞬移）。

**请求格式**：
```json
{ "x": 600, "y": 400 }
```

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：
- 目标不可达时走到最近可达点并转身面向目标（face-target）
- 单位/墙阻挡自动绕行；到达出口区域自动切图
- 剧情/切图状态（GAMESTATE_nState!=0）时操作自终止

#### 持续移动

`POST /api/world/movement/walk_dir`

**用途**：方向键持续移动（后台线程每帧 CHAR_Move，正常走路）。

**请求格式**：`{ "direction": 1 }`（0=下 1=左 2=上 3=右）

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：撞墙即停；direction 越界返回 `direction 0-3 required`；持续移动到达出口区域自动切图。

#### 停止移动

`POST /api/world/movement/stop_move`

**用途**：停止所有移动（停后台线程+清路径）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

#### 场景交互

`POST /api/world/movement/interact_with`

**用途**：场景交互/攻击键（复现官方 EVTSYSTEM_DoCheckAllEvent(2) 事件触发链）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：与 dialog/interact 等价（内部同链）。

### 3.3 静态地图 maps

> 静态数据查询域：地图信息与静态瓦片矩阵（assets maps/tiles.json，416 图）。**瓦片数据只从静态数据获取，不再从运行时内存读取**；原运行时端点 `/api/world/map/tile`（单格瓦片）与 `/api/world/map/tiles`（矩阵）已移除。

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

**注意**：0-415 按下标查询（text_id 为对应 text_id）；其他值按 text_id 反向匹配（兼容旧语义）。

#### 地图瓦片矩阵

`GET /api/world/maps/{map_id}/tiles`

**用途**：获取指定地图完整瓦片矩阵（寻路/切图判定用），数据源为静态 assets（maps/tiles.json）。

**返回格式**：
```json
{ "map_id": 30, "src": "static", "size": 64, "encoding": "base64", "tiles": "<base64 编码的 64×64=4096 字节矩阵>" }
```

**注意**：瓦片数据只从静态数据获取，缺失时返回 `{"error":"no tiles"}`；由原 `/api/world/map/tiles` 移入本端点。
---

## 四、item（物品与背包）— GET/POST /api/item/*

**物品实体全生命周期**：背包读写（inventory）、商店交易（shop）。物品从获得到消耗/处置全在一个组；穿脱装备/镶嵌宝石属于物品操作，保留在本域。

**命名约定**：POST 动作用「动词+宾语」两词命名（`use_item`/`discard_item`/`sell_item`/`equip_item`/`put_jewel`/`buy_item` 等）；静态数据中有名称的字段一律注入名称（物品名、词缀名 `option_names`）。

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

**用途**：获取指定袋内指定槽物品（**按 `slot` 字段匹配，不跳过空格**）。

**返回格式**：`<Item 对象>`（含 name）或 `null`（空槽）

**注意**：按实际格号（物品的 `slot` 字段）匹配，不做数组下标偏移——native 输出跳过空格后数组下标 ≠ 实际格号，查询以 `slot` 为准。

### 4.2 背包操作 inventory

> 写操作统一「动词+宾语」两词命名。

#### 使用物品

`POST /api/item/inventory/use_item`

**用途**：使用指定背包物品。按物品类别分派四条路径：骰子（STATUSDICE 掷骰）/ 解封（ReleaseSealed）/ 开箱（OpenItemBox）/ 常规物品（CHAR_UseItemEx 效果链：药水回血、卷轴、技能书、佣兵卡等，内部成功时自动消耗）。

**请求格式**：`{ "bag": 0, "slot": 3 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：骰子→掷骰预览返回 `base/pending/delta`（不应用，由 `accept_dice`/`reject_dice` 处理）；非消耗品→`item not usable`；常规物品冷却中/状态不符→`on cooldown`（不消耗）。

#### 接受掷骰

`POST /api/item/inventory/accept_dice`

**用途**：接受未确认的掷骰结果（应用 pending 到基础属性）。

**请求格式**：无 body

**返回格式**：`{"ok":true,"state":<Player 模型>}`

**注意**：无未确认结果→`no dice result pending`。

#### 拒绝掷骰

`POST /api/item/inventory/reject_dice`

**用途**：拒绝掷骰结果（仅清 flag，骰子不退回）。

**请求格式**：无 body

**返回格式**：`{"ok":true}`

**注意**：无未确认结果→`no dice result pending`。

#### 丢弃物品

`POST /api/item/inventory/discard_item`

**用途**：丢弃指定背包物品。

**请求格式**：`{ "bag": 0, "slot": 5 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：按槽位清空判定成功。

#### 移动/整理物品

`POST /api/item/inventory/move_item`

**用途**：移动物品或堆叠合并（INVEN_MoveItem）。

**请求格式**：`{ "bag": 0, "slot": 3, "count": 1, "to_bag": 0, "to_slot": 4 }`

**返回格式**：`{"ok":true,"state":<Inventory 模型>}`

**注意**：源空槽→`slot empty`；count≤0→参数错；同槽→`same slot`；目标越界→`bad target`。

#### 出售物品

`POST /api/item/inventory/sell_item`

**用途**：出售指定背包物品（价格=ITEM_GetPrice 静态表）。

**请求格式**：`{ "bag": 0, "slot": 5 }`

**返回格式**：`{"ok":true,"price":15,"state":<Inventory 模型>}`

**注意**：空槽→`slot empty`；价格由静态表决定（防刷钱）。

#### 穿装备

`POST /api/item/inventory/{role}/equip_item`

**用途**：指定出战槽穿上装备（背包位置或类别；装备操作为物品操作，保留在本域）。

**请求格式**（二选一）：
```json
{ "bag": 0, "slot": 3 }
```
或
```json
{ "category": 512 }
```

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：目标槽占用自动替换（先卸后穿）；不可装备（等级/职业/类型不符）按 CHAR_CanEquipItem 校验失败。

#### 脱装备

`POST /api/item/inventory/{role}/unequip_item`

**用途**：指定出战槽脱下装备。

**请求格式**：`{ "slot": 2 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：空槽返回 `unequip failed`。

#### 镶嵌宝石

`POST /api/item/inventory/{role}/put_jewel`

**用途**：把背包宝石镶嵌到指定装备槽（ITEMSYSTEM_PutJewel）。

**请求格式**：`{ "bag": 0, "slot": 3, "equip_slot": 3 }`

**返回格式**：`{"ok":true,"state":<Party 模型>}`

**注意**：无孔→`no socket`；非宝石→`not jewel`；空装备槽→`equip slot empty`；**镶嵌后自动消耗背包宝石（防刷）**。

### 4.3 商店 shop

#### 商店商品列表

`GET /api/item/shop/items`

**用途**：获取当前商店商品列表。

**返回格式**：`{ "items": [ { "slot": 0, "category": 5, "count": 1, "price": 15, "name": "恢复药水" }, ... ] }`

**注意**：price = ITEM_GetBuyPrice。

#### 购买商品

`POST /api/item/shop/buy_item`

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

