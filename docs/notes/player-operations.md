# 玩家操作能力清单（合法操作 vs OP 操作）

> 日期：2026-08-05（无实机开发阶段）｜ 来源：libgame-symbols.txt 函数枚举 + game-systems.md 游戏机制 + jadx UI 代码
> 用途：操作端点分级依据。**合法操作 = 玩家在游戏 UI 中能做到的事**（先实现）；**OP 操作 = 越权改数据/强行操作**（需先获取 OP 权限，未来实现）。

## 1. 分级定义

| 级别 | 定义 | 判定标准 | 示例 |
|---|---|---|---|
| **合法操作** | 玩家在游戏内通过 UI/正常玩法能做到的操作 | 存在对应 UI 入口（UI*_Button*Exe）或游戏机制路径；有游戏内校验（如 CHAR_CanEquipItem） | 穿/脱装备、买卖、合成、加点 |
| **OP 操作** | 玩家在游戏内做不到、需修改数据/绕过校验的操作 | 无 UI 入口，直接改全局/结构体；绕过游戏校验；生成不存在物品 | 设任意金币、生成物品、强制强化、穿透移动 |

> 边界判定：有 `*_Exe`（UI 按钮执行）函数 = 合法；仅有 `ITEMSYSTEM_Make*`/`*_Set*` 直写 = OP。
> 例外：`INVEN_AddMoney` 既是游戏捡钱/卖货路径也是 API 改钱路径——**按调用语义区分**（带校验走合法、直接调走 OP）。

## 2. 合法操作全清单（按游戏系统）

### 2.1 移动与场景（移动=点击/触摸注入）

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 点击移动 | 点击地面，角色走路径到达 | `TouchHandle_Event`/`TouchHandle_ControlEventProc`（触摸注入）、`CHAR_MoveAsPath`/`CHAR_Move`、`CHAR_SearchPath`（已实现寻路） | P0 |
| 角色跟随/归队 | 佣兵跟随主角色 | `PARTY_Follow` | P1 |
| 队伍撤退 | 战斗撤退 | `PARTY_MoveBack` | P1 |
| 跨图传送（传送门） | 走到传送门/世界地图点选 | `MAPSYSTEM_ChangeMap(mapId,x,y,dir)`、`UIPlay_CallOpenWorldmap`、`UIPlay_NextMap` | P0（已实现 teleport） |
| 同图传送（卷轴类） | 使用移动卷轴/点对点物品 | `CharSetPosition(x,y)`（全队，写坐标） | P0（已实现 teleport） |

### 2.2 战斗

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 普通攻击 | 点击攻击按钮 | `UIPlay_bPressedAction`、触摸注入 | P1 |
| 释放技能 | 技能快捷键 | `UIPlay_ButtonSKill`、`UISkill_SkillMainExe`、`UIMercenary_SkillExe`、`CHAR_ProcessSkillBook` | P0（skill 已实现学习） |
| 使用物品（药水） | 背包/快捷键使用 | `UIEquip_ButtonUseExe`、`UIEquip_ConfirmUseItem`、`INVEN_ConsumeItem` | P0 |
| 自动攻击开关 | 技能菜单开关 | `UISkill_ButtonAutoExe`、`CHAR_SetAutoAttack` | ✅ 已实现 |
| 战斗 AI 模式 | 技能菜单 AI 设置 | `UISkill_ButtonAIExe`、`UISkill_MakeAIInfo` | P1 |
| 复活 | 复活卷轴/费用复活 | `CHAR_ProcessReviveScroll`、`PARTY_GetReviveCost`、`PARTY_AddHPMP` | P1 |
| 休息恢复 | 营地休息 | `PARTY_ApplyRest`、`PARTY_GetRestCost` | P1 |

### 2.3 背包与物品

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 使用物品 | 背包双击/确认使用 | `UIEquip_ButtonUseExe`/`UIEquip_OKConfrimUseItem`/`INVEN_ConsumeItem` | P0 |
| 丢弃物品 | 背包销毁确认 | `UIEquip_ButtonDestroyExe`/`UIEquip_OKDestroyItem`/`INVEN_RemoveItemDirect` | P0 |
| 移动/整理背包 | 拖拽/整理按钮 | `INVEN_MoveItem`（4 参签名待逆向） | P1 |
| 开箱 | 使用钥匙开箱 | `UIEquip_ButtonOpenBoxExe`、`ITEMSYSTEM_OpenItemBox` | P1 |
| 掷骰鉴定 | 装备鉴定骰子 | `UIEquip_ButtonRollDiceExe` | P2 |
| 解封装备/技能书 | 解封卷轴 | `UIEquip_ButtonReleaseSealedExe`、`ITEMSYSTEM_ReleaseSealed`/`ReleaseSealedSkillBook` | P2 |

### 2.4 装备

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 穿装备 | 背包→装备槽 | `UIEquip_ButtonEquipExe` → `CHAR_EquipItem`（有 `CHAR_CanEquipItem` 职业/等级校验） | ✅ 已实现 |
| 脱装备 | 装备槽→背包 | `UIEquip_ButtonUnequipExe` → `CHAR_UnequipItemToInven` | ✅ 已实现 |
| 装备强化/附魔 | 铁匠强化（消耗材料+金币） | `ITEMSYSTEM_EnchantItem`/`ApplyEnchantValue`、`UIEquip_ApplyStuff` | P1（走正规校验） |
| 宝石镶嵌 | 镶嵌宝石到插槽 | `ITEMSYSTEM_PutJewel`/`ApplySocket`、`UIMix_FindJewelUpgradeStuff` | P1 |
| 快捷键绑定 | 物品/技能拖到快捷键栏 | `UIEquip_ButtonShortCutExe`/`UISkill_ButtonShortcutExe`/`CHAR_AddShortcut` | P2 |

### 2.5 角色成长

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 分配属性点 | 属性面板加点 | `CHAR_SetStatusPoint`（游戏限制：≤剩余能力点） | ⚠️ 已实现（需改语义：只允许扣剩余点） |
| 学习技能 | 技能树学习 | `UISkill_ButtonLearnOK`/`ButtonLearnExe` → `CHAR_LearnAction`（消耗技能点） | ✅ 已实现 |
| 升级技能 | 技能树升级 | `UISkill_ButtonUpExe`、`CHAR_ProcessSkillBook` | P1 |
| 技能点重置 | 技能面板重置（内购） | `UISkill_ButtonSkillPointResetExe`（含 UIInAppProcess=内购） | P2（依赖内购） |
| 切换主控角色 | 游戏内切换按钮 | `UIPlay_ButtonSwap`/`UIPlay_SelectPlayer` → `PARTY_SetActivePlayer` | ✅ 已实现 |

### 2.6 队伍/佣兵

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 佣兵入队 | 佣兵菜单「加入队伍」 | `UIMercenary_ButtonIncludeExe` → `MERCENARYSYSTEM_IncludeParty`/`PARTY_Include` | P1 |
| 佣兵离队 | 佣兵菜单「退出队伍」 | `UIMercenary_ButtonExcludeExe` → `MERCENARYSYSTEM_ExcludeParty`/`PARTY_Exclude` | P1 |
| 佣兵遣散 | 佣兵菜单「解散」 | `UIMercenary_ButtonDischargeExe` → `MERCENARYSYSTEM_Release` | P1 |
| 佣兵取出/放置 | 佣兵列表管理 | `UIMercenary_ButtonWithdrawExe`/`ButtonListExe` | P2 |
| 队伍换位 | 队伍界面调整顺序 | `PARTY_Swap(a,b)` | P1 |

### 2.7 商店/交易

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 购买 | 商店买物品（金币） | `UIStore_ButtonBuyExe`/`UIStore_BuyItem`/`UIStore_BuyOKInputItemCount`、`DEALSYSTEM_FindSaleByID` | P0 |
| 出售 | 商店卖物品 | `UIStore_ButtonSellExe`/`UIStore_SellItem`/`UIStore_SellOKInputItemCount`、`DEALSYSTEM_AddSale*` | P0 |
| 稀有度商店抽奖 | 金币抽奖 | `RarityShop`、`DEALINFOBASE` | P2 |

### 2.8 合成/炼金

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 配方合成 | 合成菜单选配方→执行 | `UIMix_ButtonMixingExe`、`MIXSYSTEM_CheckMixture`/`GetCost`/`GetResultItemCount` | P1 |
| 宝石合成/升级 | 宝石合成 | `UIMix_ButtonMixingGemExe`/`UIMix_FindJewelUpgradeStuff` | P1 |
| 学习配方 | 获得配方书 | `MIXSYSTEM_AddRecipeBook` | P2 |

### 2.9 任务

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 接受任务 | NPC 任务菜单「接受」 | `QUESTSYSTEM_AcceptReivew`/`QUESTSYSTEM_Add`、`UINpcQuest` | P1 |
| 交付任务 | 任务菜单「完成」 | `UIQuestMenu_ButtonClearExe`、`QUESTSYSTEM_ApplyReward`/`CanUseClear`/`ApplyPrepare` | P1 |
| 放弃任务 | 任务菜单「放弃」 | `UIQuestMenu_ButtonQuitExe`、`QUESTSYSTEM_RefuseReview` | P1 |

### 2.10 存档

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 手动存档 | 菜单存档 | `UIPlay_CallSave`、`SAVE_ProcessSave`/`SAVE_Save`（签名待逆向） | P1 |
| 读档 | 主菜单读档 | `SAVE_Load*`/`GAMELOADER`（游戏内通常主菜单操作） | P2（风险高） |

### 2.11 经济

| 操作 | 游戏内方式 | 函数证据 | 优先级 |
|---|---|---|---|
| 金币增减（合法路径） | 捡钱/卖货/花费 | `INVEN_AddMoney`/`INVEN_MinusMoney`（游戏内部调用） | ⚠️ 已实现（add/minus 保留，set 归 OP） |

## 3. OP 操作清单（需 OP 权限，未来实现）

| 操作 | 说明 | 函数证据 |
|---|---|---|
| 设置任意金币 | 直接写 `INVEN_nMoney`/`INVEN_SetMoney` | `INVEN_SetMoney` |
| 获取任意物品 | 生成指定 itemId 物品进背包 | `ITEMSYSTEM_MakeItem`/`MakeEquip`/`MakeFixedItem`/`MakeMoney`/`MakeSkillBook` |
| 设置经验/等级 | 直接改角色经验/等级 | `CHAR_SetExperience`/`CHAR_SetLevel`/`CHAR_AddExperience`（绕过升级链） |
| 设置能力点/技能点 | 直接写剩余点数 | `CHAR_SetStatusPoint`（任意值）/`CHAR_SetSkillPoint` |
| 设置物品属性 | 强制强化/镶嵌/附魔到任意值 | `ITEMSYSTEM_ApplyEnchantValue`/`ApplySocket`/`ApplyGrade`/`ApplyChaosValue` |
| 强行装备 | 绕过 `CHAR_CanEquipItem` 职业/等级校验 | `CHAR_EquipItemFromInvenToSlot`（直写槽位） |
| 无视碰撞移动 | 穿透墙壁/传送任意坐标 | `CharSetPosition` 直写、`CHAR_MoveAsBlock` |
| 消耗不减少数量 | 调用效果函数但保留物品 | 绕过 `INVEN_ConsumeItem` 计数 |
| 物品复制/作弊生成 | 调用 `CHEATCHAR*` 函数 | `CHEATCHARBASE` 相关（盗版大修自带的作弊角色表） |

## 4. 实现规划

### 4.1 已实现（v0.3.1 API 重构后）→ 分级

> v0.3.1 起：**信息获取（GET）与操作（POST）分离**。合法操作统一 `/api/action/*`；OP 操作不暴露 HTTP 端点（native 就绪，未来 `/api/op/*`）。

**合法操作端点（POST /api/action/*，✅ v0.3.1）**：money（add/minus）、move、use-item、equip、unequip、auto-attack、skill（学习）、switch、inventory/discard、inventory/sell、party/include、party/exclude、teleport。

**已从 HTTP 移除的 OP 类端点（native 保留，未来 /api/op/*）**：
- money **set**（设任意值）
- experience（set/add——玩家不直接改经验）
- status-point（set 任意点数——合法路径只允许 ≤ 剩余点）
- inventory/remove（按类别删除——丢弃应走 discard 指定槽）

### 4.2 合法操作实现状态与优先级（v0.3.1 更新）

| 优先级 | 操作 | 状态 |
|---|---|---|
| P0 | 移动（move：SearchPath+MoveAsPath） | ✅ v0.3.1 实现 |
| P0 | 使用物品（use-item） | ✅ v0.3.1 实现 |
| P0 | 商店买/卖 | ⏸️ **依赖 UI 状态**（UIStore_BuyItem/SellItem 需商店界面选中），暂缓 |
| P1 | 丢弃物品（discard） | ✅ v0.3.1 实现（RemoveItemDirect 按槽） |
| P1 | 出售物品（sell：discard+加钱） | ✅ v0.3.1 实现（价格由调用方提供） |
| P1 | 佣兵入队/离队（include/exclude） | ✅ v0.3.1 实现 |
| P1 | 任务接/交 | ⏸️ 依赖 UI（AcceptReivew 硬编码 quest 489），暂缓 |
| P1 | 释放技能 | ⏸️ 依赖 UI（UIPlay_ButtonSKill/技能快捷键状态），暂缓 |
| P1 | 合成 | ⏸️ 依赖 UI（UIMix_ButtonMixingExe 材料槽状态），暂缓 |
| P2 | 升级技能/强化/镶嵌/开箱/鉴定/AI 模式/休息/复活/换位 | 待逆向 |

### 4.3 OP 权限机制（未来）

- 设计独立的 OP 端点组（`/api/op/*`）+ 权限开关（模块配置/首次启动随机 token）
- 文档与合法端点分离，避免误用
- 实现顺序：金币 set → 物品生成 → 属性直改 → 强行操作
