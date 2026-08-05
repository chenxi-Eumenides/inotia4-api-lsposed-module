# API 逐端点审查（功能 / 游戏内存在性 / 合法性）

> 日期：2026-08-05 ｜ 方法：对全部 34 个端点逐一定义功能 → 对照游戏机制判断"玩家游戏内能否做到" → 分级
> 分级标准：**合法** = 玩家在游戏 UI 中能做到（有 UI 入口或游戏机制路径 + 有业务校验）；**OP** = 游戏内做不到（直接改数据/越权/绕过校验）
> 信息类（GET 只读）无副作用，一律归信息组（合法——不改变游戏状态）

## 1. 动态信息获取（GET /api/info/*，11 端点）

| 端点 | 功能 | 游戏内存在 | 分级 | 说明 |
|---|---|---|---|---|
| `/api/info/player` | 金币/地图/坐标/队伍/控制角色总览 | ✅ HUD 与状态栏 | ✅ 信息 | 只读，实时采样 |
| `/api/info/player/party` | 3 出战角色完整状态（HP/MP/属性/装备/名称） | ✅ 角色/队伍面板 | ✅ 信息 | 只读 |
| `/api/info/player/skills` | 技能链表/位图/技能点 | ✅ 技能面板 | ✅ 信息 | 只读 |
| `/api/info/player/mercenaries` | 全部佣兵（含未上场） | ✅ 佣兵菜单 | ✅ 信息 | 只读（未上场信息为菜单可见） |
| `/api/info/inventory` | 背包物品+属性+名称+容量 | ✅ 背包界面 | ✅ 信息 | 只读 |
| `/api/info/map` | 地图 ID + 玩家坐标 | ✅ 小地图/界面 | ✅ 信息 | 只读 |
| `/api/info/quest` | 当前激活任务 | ✅ 任务日志 | ✅ 信息 | 只读 |
| `/api/info/units` | 场景单位坐标（敌人/NPC/召唤物） | ⚠️ 屏幕可见敌人，完整坐标属"透视" | ✅ 信息 | 只读无副作用，信息扩展（可辅助外部程序寻路/避敌） |
| `/api/info/ui` | UI 状态（主菜单/游戏中） | ✅ 界面状态 | ✅ 信息 | 只读 |
| `/api/info/path?tx=&ty=` | 寻路计算（引擎 A*） | ✅ 玩家点击移动时游戏内部计算 | ✅ 信息 | **只计算不移动**（无副作用）；辅助移动决策 |
| `/api/info/events` | 事件流（轮询差异） | ✅ 游戏事件（战斗/拾取/升级） | ✅ 信息 | 轮询快照对比，零 hook 无副作用 |

> 结论：信息组 11 端点全部合理。`units`（敌人坐标）和 `path`（寻路计算）是信息扩展，但只读无副作用，归信息组正确。

## 2. 静态数据获取（GET /api/data/*，11 端点）

| 端点 | 功能 | 游戏内存在 | 分级 | 说明 |
|---|---|---|---|---|
| `/api/data/roles` | 职业/属性成长/等级上限 | ✅ 游戏数据表 | ✅ 信息 | 静态表 |
| `/api/data/items` | 物品配置 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/skills` | 技能表 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/mercenaries` | 佣兵+技能 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/maps` | 地图+传送点 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/monsters` | 怪物数值/技能/掉落 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/quests` | 任务+奖励 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/npcs` | NPC | ✅ | ✅ 信息 | 静态表 |
| `/api/data/events` | 剧情事件 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/text?lang=` | 7 语言文本 | ✅ | ✅ 信息 | 静态表 |
| `/api/data/tables/{name}` | 任意原始表 | ✅ | ✅ 信息 | 静态表（全量导出，只读） |

> 结论：静态数据组全部合理（M3 解析产物，只读）。

## 3. 玩家操作（POST /api/action/*，12 端点）

| 端点 | 功能 | 游戏内方式 | 分级 | 结论 |
|---|---|---|---|---|
| `player/move` | 寻路移动（SearchPath+MoveAsPath） | ✅ 点击地面移动（渐进寻路） | ✅ **合法** | 等同玩家点击移动，无越权 |
| `player/use-item` | 使用药水/卷轴（ConsumeItem） | ✅ 背包/快捷键使用 | ✅ **合法** | 消耗数量走游戏内部计数 |
| `player/{role}/equip` | 穿装备（自动找槽） | ✅ 装备界面 | ✅ **合法** | 走 `CHAR_CanEquipItem` 职业/等级校验 |
| `player/{role}/unequip` | 脱装备 | ✅ 装备界面 | ✅ **合法** | 走 `CHAR_UnequipItemToInven` |
| `player/{role}/auto-attack` | 自动攻击开关 | ✅ 技能菜单/设置 | ✅ **合法** | 游戏内开关（UI 有按钮） |
| `player/{role}/skill` | 学习/升级技能（LearnAction） | ✅ 技能树学习 | ✅ **合法** | 走 `CHAR_LearnAction`（游戏内校验技能点/前置） |
| `player/switch` | 切换主控角色 | ✅ 切换按钮 | ✅ **合法** | 走 `PARTY_SetActivePlayer`（校验 CAN_BeActive） |
| `inventory/discard` | 丢弃指定槽物品 | ✅ 背包丢弃确认 | ✅ **合法** | 走 `RemoveItemDirect`（删真实物品） |
| `inventory/sell` | **"卖"物品（删物品+加钱）** | ⚠️ 游戏卖物品走商店（价格由商店/物品决定） | ❌ **OP** | **价格由调用方任意传 = 刷钱漏洞**（可任意定价卖物品）。游戏内价格来自 `ITEMDATABASE` 定价+商店折扣 |
| `party/include` | 佣兵入队 | ✅ 佣兵菜单「加入队伍」 | ✅ **合法** | 走 `MERCENARYSYSTEM_IncludeParty`（内部队伍上限校验） |
| `party/exclude` | 佣兵离队 | ✅ 佣兵菜单「退出队伍」 | ✅ **合法** | 走 `MERCENARYSYSTEM_ExcludeParty` |
| `teleport` | **任意切图/瞬移** | ⚠️ 玩家只能通过传送门/世界地图/传送卷轴到**特定已解锁点** | ❌ **OP** | **任意 mapId 切图 + 任意坐标瞬移 = 游戏内做不到**。合法路径 = 传送到已解锁传送点（PORTALINFOBASE），需校验 |

> 结论：12 个操作端点中 **2 个误归合法**（`inventory/sell`、`teleport`），应归 OP；10 个合法。

### 误归原因分析

1. **`inventory/sell`**：实现为"删物品 + 加任意价格"——游戏内卖物品的**价格由商店系统决定**（物品基础价 × 商店折扣），玩家不能自己定价。调用方自传 price = 绕过定价 = 刷钱（OP）。
   - 合法版本：价格从 `ITEMDATABASE` 静态表读取（物品价格字段）→ 但价格字段语义未逆向；或走商店 UI（依赖 UI 状态，暂缓）
   - **修正**：归 OP（`/api/op/inventory/sell`），native 保留
2. **`teleport`**：实现为"任意 mapId 切图 / 任意坐标瞬移"——玩家游戏内只能走传送门/卷轴到已解锁点。任意切图 = 跳过地图解锁流程（OP）。
   - 合法版本：传送到**当前地图可达坐标**（move 已覆盖）或**已解锁传送点**（PORTALINFOBASE 过滤，未实现）
   - **修正**：归 OP（`/api/op/move/teleport`），native 保留

## 4. 修正后的合法操作（10 端点）

move、use-item、equip、unequip、auto-attack、skill（学习）、switch、inventory/discard、party/include、party/exclude

## 5. OP 端点规划（未来 /api/op/*）

| 端点 | 功能 | 依据 |
|---|---|---|
| `/api/op/player/money` | add/minus/set 任意金币 | 玩家游戏内无法凭空加钱（2026-08-05 修正） |
| `/api/op/player/experience` | set/add 任意经验 | 玩家不直接改经验 |
| `/api/op/player/status-point` | set 任意能力点 | 合法路径只允许 ≤ 剩余点 |
| `/api/op/player/skill-point` | set 任意技能点 | 同上 |
| `/api/op/inventory/sell` | 任意定价出售 | 价格绕过商店系统（本次修正） |
| `/api/op/move/teleport` | 任意切图/瞬移 | 跳过地图解锁（本次修正） |
| `/api/op/item/give` | 生成任意物品 | ITEMSYSTEM_MakeItem |
| `/api/op/item/attributes` | 强制强化/镶嵌 | ApplyEnchantValue/ApplySocket |
| `/api/op/equip/force` | 强行装备 | 绕过 CanEquipItem |
| `/api/op/move/through` | 无视碰撞 | 直接写坐标 |
| `/api/op/consume` | 消耗不减少数量 | 绕过计数 |

## 6. 审查方法说明

- **信息组**（GET）：只读无副作用，不改变游戏状态 → 一律合法（含透视类信息如敌人坐标——属信息扩展，外部程序可用）
- **操作组**（POST）：判定"玩家游戏内能否做到 + 是否有业务校验"
  - 合法 = 有 UI 入口 + 游戏内部校验（CanEquip/队伍上限/技能点/真实物品）
  - OP = 直接改数据（任意金额/经验/点数）、绕过校验（任意定价/任意传送/强行装备）、生成不存在之物
