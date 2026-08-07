# 游戏系统与数据结构总览（Inotia 4 盗版大修 v5.0）

> 日期：2026-08-05 ｜ 来源：`libgame-symbols.txt`（readelf 导出符号，6626 函数 + 全局变量）+ `apk/decompiled/`（jadx）
> 用途：确定 API 能提供什么数据、M3 需要解析哪些静态表、M4 需要 hook/读取哪些运行时数据。

## 1. 游戏类型

暗黑破坏神风格 ARPG（点击移动 + 即时战斗 + 技能释放）。玩家操控 1 名主角色 + 2 名佣兵（共 3 人出战），地图上遇敌即时战斗，掉落装备/材料，任务驱动推进。

## 2. 玩法系统清单（模块证据）

| # | 系统 | 核心模块/符号 | 说明 |
|---|---|---|---|
| 1 | **角色养成** | CHAR(354)、CHARSYSTEM、CHARCLASSBASE、ATTRINITBASE、MAXLEVELBASE | 职业、等级、经验、属性（力量/敏捷等）、技能点、属性点分配 |
| 2 | **队伍/佣兵** | PARTY(36)、MERCENARYSYSTEM(17)、MERCENARYINFOBASE、MERCENARYSLOT | **3 人出战**（`PARTY_pChar` 3 指针）；佣兵招募/入队/离队（`MERCENARYSYSTEM_pSlotList`） |
| 3 | **战斗** | CHAR_Get*Damage、HATESYSTEM、STATUSDICE、CHECKTIME_*、MONSTERAI、**CHARLOCSYSTEM** | 物理/魔法/暴击/命中/闪避计算、仇恨、状态判定、怪物 AI；**所有单位（玩家+敌人+NPC）位置登记于 CHARLOCSYSTEM，可枚举坐标** |
| 4 | **状态/Buff** | CHARACTERSTATEBASE、BUFFDATABASE、BUFFSYSTEM、CONDITIONBASE | 中毒/眩晕/灼烧等状态与增益效果 |
| 5 | **装备** | ITEMENCHANTBASE、ITEMGRADEBASE、ITEMRARITYGRADEBASE、UIEquip、CHAR_FindEquipSlot | 多槽位装备、附魔/强化、**稀有度 5 档：白/绿/蓝/黄/紫（黄 = 特殊装备）**；颜色映射 `UIDesc_GetColorAsItemGradeByID`、取值 `ITEMSYSTEM_GetRarity` |
| 6 | **背包/物品** | INVEN(33)、ITEMSYSTEM(75)、ITEMDATABASE、ITEMCLASSBASE | 槽位背包、物品堆叠、物品分类（装备/消耗/材料/任务） |
| 7 | **货币** | MONEY_GetGold/GetSilver/GetCooper、INVEN_nMoney | **金/银/铜三币制** |
| 8 | **商店/交易** | UIStore、DEALSYSTEM、DEALINFOBASE、RarityShop | NPC 商店买卖、稀有度商店（金币抽奖） |
| 9 | **内购（已阉割）** | UIInApp(44)、CASHITEMBASE、CHARGEDITEMBASE、InApp* | 现金道具/宝石商店（盗版版功能已断，数据表仍在） |
| 10 | **合成/炼金** | MIXSYSTEM、MIXTUREBASE、RECIPEBASE、ITEMMIXLINKBASE | 配方合成、材料混合 |
| 11 | **任务** | QUESTSYSTEM(35)、QUESTINFOBASE、QUEST*BASE、UINpcQuest | 主线/支线 NPC 任务、任务链/前置/奖励/掉落 |
| 12 | **地图/场景** | MAPSYSTEM、MAPINFOBASE、PORTALINFOBASE、WORLDMAPBUILDER、WEATHERSYSTEM、**MAP_nBaseTile / MAP_nBaseInfo / MapBlockingcheck / ASTAR** | 区域地图、世界地图、传送门、天气；**当前地图瓦片矩阵 + 阻挡检测 + 引擎自带 A* 寻路** |
| 13 | **掉落/拾取** | MONSTERDROPBASE、DROPINFOBASE、DROPDETAILINFOBASE、MAPITEMDROP、OPENITEMBOXBASE | 怪物掉落表、地图物品、开箱 |
| 14 | **事件/剧情脚本** | EVTSYSTEM、EVTCMDBASE、ACTDATABASE、ANIMATION*、Scene(189) | 剧情事件驱动（对话/演出/条件分支） |
| 15 | **存档** | SAVE(68)、SAVESLOT、GAMELOADER、HubSave | 本地存档槽 + Hive 云存档（备份/恢复） |
| 16 | **教程** | Tutorial*(16) | 新手引导流程 |
| 17 | **奖励/每日** | DailyReward | 每日登录奖励 |
| 18 | **社交/平台** | Hub*、onHub*、NET(35)、C2S(31) | Hive 账号、云存档、分享（盗版版已断开） |
| 19 | **辅助** | SOUNDSYSTEM、TEXTINPUTSYSTEM、TEXTDATABASE、TIPBASE、HELPTEXTBASE | 音效、输入、文本/提示/帮助 |

## 3. 静态数据表（100 张，= game_res 需解析的全部内容）

来源：`*BASE_nRecordCount / *BASE_pData` 全局对象对。M3 解析优先级：

### 高优先级（直接对应 API 静态数据）

| 表 | 内容 |
|---|---|
| `CHARCLASSBASE` | 职业定义 |
| `ATTRINITBASE` | 属性初始值/成长 |
| `MAXLEVELBASE` | 等级上限 |
| `ITEMDATABASE` / `ITEMCLASSBASE` / `ITEMSTATICBASE` / `ITEMDESCBASE` | 物品主表/分类/静态属性/描述 |
| `ITEMENCHANTBASE` / `ITEMGRADEBASE` / `ITEMRARITYGRADEBASE` | 附魔/品级/稀有度 |
| `SKILLDESCBASE` / `SKILLTRAINBASE` / `SKILLTRAINPOINTBASE` | 技能描述/训练/技能点消耗 |
| `MERCENARYINFOBASE` / `MERCENARYSKILLBASE` / `MERCENARYGROUPSKILLBASE` | 佣兵信息/技能/群体技能 |
| `MAPINFOBASE` / `MAPFEATUREINFOBASE` / `PORTALINFOBASE` | 地图信息/特征/传送门 |
| `MONDATABASE` / `MONAIINFOBASE` / `MONSKILLBASE` / `MONSTERDROPBASE` | 怪物/怪物 AI/技能/掉落 |
| `QUESTINFOBASE` / `QUESTGROUPBASE` / `QUESTREWARDBASE` | 任务/分组/奖励 |
| `NPCINFOBASE` / `NPCDESCBASE` | NPC 信息/描述 |

### 中优先级

| 表 | 内容 |
|---|---|
| `MIXTUREBASE` / `RECIPEBASE` / `ITEMMIXLINKBASE` | 合成配方 |
| `DROPINFOBASE` / `DROPDETAILINFOBASE` / `OPENITEMBOXBASE` | 掉落明细/开箱 |
| `ITEMBUFFBASE` / `ITEMOPTINFOBASE` / `ITEMRECOVERBASE` / `ITEMPACKBASE` | 物品 Buff/词缀/回复/打包 |
| `STATUSBASE` / `STATUSINFOBASE` / `STATUSASSIGNBASE` / `STATUSDICEBASE` | 状态/状态信息/分配/判定 |
| `CHARACTERSTATEBASE` / `CHARACTERSTATECHANGEBASE` / `CONDITIONBASE` | 角色状态/变化/条件 |
| `BUFFDATABASE` / `BUFFUNITBASE` | Buff 定义 |
| `DEALINFOBASE` / `CASHITEMBASE` / `CHARGEDITEMBASE` / `CHARGEDITEMPRODUCTBASE` | 交易/内购商品 |
| `QUESTPREPAREBASE` / `QUESTCOMPLETEBASE` / `QUESTLINKBASE` / `QUESTDROPBASE` / `QUESTGENBASE` / `QUESTOBJECTCHANGEBASE` | 任务链/掉落/生成 |

### 低优先级（渲染/演出/辅助）

`ACTDATABASE`、`ACTUNITBASE`、`ACTTRANSMIT*`、`ANIMATION*`、`EFFECTINFOBASE`、`EVENTDATABASE`、`EVTCMD/EVTCOND/EVTINFO`、`CONSTBASE`、`CHOICEBASE`、`COLORRATE*`、`EXPRESSBASE`、`HELPTEXTBASE`、`IMAGEFILEBASE`、`INSTALLBASE`、`MANAGEMBASE`、`MAPCOLORBASE`、`NPCABILITY/NPCFUNC*`、`PORTRAIT*`、`SOUND*`、`SYMBOLBASE`、`TEXTDATABASE`、`TIPBASE`、`CHARACTERCOSTUME*`、`MONSTERCOSTUMEBASE`、`NPCCOSTUMEBASE`、`CHEATCHAR*`、`QUESTGENBASE`

## 4. 动态运行时数据（模块可读取）

| 数据 | 来源 | 说明 |
|---|---|---|
| 金币（金/银/铜） | `INVEN_nMoney` + `MONEY_GetGold/GetSilver/GetCooper` | 三币制 |
| 角色经验/技能点/属性点 | `CHAR_GetExperience` / `CHAR_GetSkillPoint` / `CHAR_GetStatusPoint` | |
| 角色属性 | `CHAR_GetStat` / `CHAR_GetAttr` | 力量/敏捷等 |
| 角色 HP/MP | 角色结构体 +0x1F0/+0x1F4（上限 = CHAR_GetAttr 0x1e/0x1f） | 已逆向，见 hook-points.md |
| 装备 | `CHAR_GetEquipItem` / `CHAR_FindEquipSlot` | |
| 技能列表 | 角色 +0x2A0 技能链表（`CHARSYSTEM_GetSkillList` 是 stub，勿用） |
| 队伍（3 人） | `PARTY_pChar` / `PARTY_GetMember(i)` | |
| 佣兵槽位 | `MERCENARYSYSTEM_pSlotList` / `SAVE_nPartyMercenarySlot` | |
| 背包 | `INVEN_pItem` / `INVEN_pBagSlot` / `INVEN_MakeItemList` | |
| 当前地图/坐标 | **MAP_nBaseInfo+0**（地图ID，实时）/ 角色结构体 **+0x02/+0x04**（坐标，实时） | ✅ 2026-08-05 真机实测 |
| 当前任务 | `QUESTSYSTEM_nActiveQuest` | |
| 传送目标 | `ACTTRANSSYSTEM_nCoordX/Y`、`nTargetIndex` | |
| 敌人坐标 | `CHARLOCSYSTEM_pPool` + `CHARLOCSYSTEM_Find` | 所有单位位置（含敌人），字段偏移待逆向 |
| 地图瓦片/阻挡 | `MAP_nBaseTile`（8192B）+ `MapBlockingcheck` | 当前地图通行矩阵 |
| 存档槽 | `SAVE_pSaveSlot`（87 字节） | 离线兜底 |
| 各类计时 | `CHECKTIME_*`（Buff/药水/单位/攻城道具 tick） | |

## 4.5 坐标与地图能力（外部程序可用）

### 敌人坐标：可以获得 ✅

- **CHARLOCSYSTEM**（角色位置系统：`CHARLOCSYSTEM_pPool` + `CHARLOCSYSTEM_nCount`）：游戏中所有单位的**位置登记池**（玩家 + 敌人 + NPC），`CHARLOCSYSTEM_Find` 按索引查找、`CHARLOCSYSTEM_Add` 登记
- 位置字段（x/y）在单位结构体，偏移待逆向（反汇编 `CHARLOCSYSTEM_*` + frida 运行时探测）
- 辅助函数：`CHAR_GetDistance`（角色间距）、`CheckCharLocation`、`MATH_GetDistance`
- 敌人单位枚举：`CHARSYSTEM_pPool`（角色对象池，含怪物）+ `MONSTERAI_RunAIProc`（AI 驱动单位）
- 坐标系：与 `MAP_nFocusX/Y` 相同的地图瓦片坐标系

### 地图数据：可以提供 ✅（可用于计算移动）

| 数据 | 来源 | 用途 |
|---|---|---|
| 瓦片矩阵 | `MAP_nBaseTile`（8192 字节，当前地图） | 地图布局（瓦片编码格式待逆向） |
| 地图基础信息 | `MAP_nBaseInfo`（4096 字节） | 尺寸/偏移 |
| 阻挡检测 | `MapBlockingcheck`（函数 0xf2860） | 判断坐标是否可通行 |
| 最近可达点 | `MAP_GetNearWaitCoord` / `MAP_GetNearestWaitCoord` | 找最近可站立坐标 |
| 寻路 | `ASTAR_GeneratePath` / `ASTAR_Step`（引擎自带 A*，13 函数） | 外部复算路径 |
| 特征区域 | `MAPFEATURE_GetAreaRect` / `MAPFEATURE_ApplyFIlter` | 特殊区域（传送点/事件区/碰撞） |

两种提供方式：
1. **导出通行矩阵**：模块读 `MAP_nBaseTile` + `MapBlockingcheck` 语义 → API 提供地图数据，外部程序自行寻路
2. **调用引擎寻路**：模块调用 `ASTAR_GeneratePath` → API 提供"路径计算"端点（需游戏运行时）

> 注意：`MAP_nBaseTile` 瓦片编码、`MAP_nDisplayBX/BTX`（显示起点）语义需逆向确认；地图随 `SAVE_nMapID` 切换而更新。

## 5. 结论：API 能提供什么（初步）

1. **玩家状态**：金币（三币）、等级/经验、HP/MP、属性、技能点、当前地图、坐标、任务
2. **队伍**：3 名角色完整状态（装备槽 + 稀有度 + 技能列表 + 属性）
3. **背包**：物品列表（静态表联查名称/属性/稀有度）
4. **静态全量**：100 张表可按需导出（优先：职业/物品/技能/佣兵/地图/怪物/任务/NPC）
5. **单位坐标**：玩家 + 敌人 + NPC 实时坐标（CHARLOCSYSTEM）
6. **地图数据**：当前地图通行矩阵 + 阻挡检测 + 可选寻路（供外部计算移动）
7. **事件**（可选）：hook `CHAR_AddExperience`/`INVEN_AddMoney`/`PARTY_AddHPMP`/`INVEN_MoveItem` 做变化推送
