# 功能研究：合成器批量宝石合成 + 自定义 UI（craft-batch-ui）

> 状态：研究设计稿（合成逻辑已反汇编确认，UI 注入方案待探索）
> 工作区：`wt-craft-ui`（分支 `feature/craft-ui`）
> 配置开关：`ModuleConfig.jewelBatchMix`（config.json，默认 false）
> 日期：2026-08-14

## 1. 功能目标

在游戏**合成器界面**上增加「批量合成」交互：把第一个背包中所有宝石（低级/中级/高级/顶级）按规则批量合成为更高一级宝石。最终形态是**游戏内 UI 按钮**（非 HTTP 端点），由 `ModuleConfig.jewelBatchMix` 开关控制。

## 2. 合成规则（已反汇编确认）

### 2.1 宝石配方（RECIPEBASE，mixType = 配方索引）

| mixType | 材料 | 数量 | 产物 | 费用文本ID |
|---|---|---|---|---|
| 12 | 低级宝石 cat28 | ×3 | 中级宝石 cat29 | 188 |
| 13 | 中级宝石 cat29 | ×3 | 高级宝石 cat30 | 189 |
| 14 | 高级宝石 cat30 | ×3 | 顶级宝石 cat31 | 190 |
| 15 | 顶级宝石 cat31 | ×3 | 混沌宝石 cat32 | 191 |

**比例固定 3:1**，每配方只消耗一种材料。宝石物品 id：低级=58、中级=59、高级=60、顶级=61、混沌=62（ITEMDATABASE）。

### 2.2 执行函数链（UIMix_StartMix @0xc0870）

```
UIMix_StartMix:
  ├─ UIMix_GetType()（读 [0x305000+0x588] u8）分支
  │   ├─ type 2/3（宝石合成）：ControlItem_GetItem 取材料
  │   │     → MIXSYSTEM_MakeItem(mixType, &outItem)
  │   │     → 成功后 MIXSYSTEM_UseStuff(mixType, stuffList) 消耗
  │   │     → INVEN_MinusMoney(费用)
  │   └─ type 1（宝石升级）：MakeItem → INVEN_SaveItem(产物)
  │       → 遍历控件 INVEN_RemoveItem 删材料 → INVEN_MinusMoney
  └─ 费用校验（UIMix_ButtonMixingExe @0xc21ec）：
        INVEN_GetMoney() < [0x305000+0x550+0xf8] → 弹「金币不足」(text 0xa)
```

### 2.3 关键函数

| 函数 | 地址 | 说明 |
|---|---|---|
| MIXSYSTEM_MakeItem | 0x11af58 | 产物生成。非装备类：读 RECIPEBASE[mixType*12+2] 产物 id → ITEMSYSTEM_CreatePerfectItem → *outItem；mixType 0/1→MakeChaos、0x10→ApplySocketForMixure |
| MIXSYSTEM_GetCost | 0x11ab64 | 费用：读配方表 +8 费用文本 id → MEMORYTEXT_GetText_E(0x1186cc) → CAL_Calculate 按公式计算 |
| MIXSYSTEM_UseStuff | 0x11b300 | 遍历材料槽，逐条 INVEN_RemoveItemData(0x1040a8) 删材料 |
| INVEN_SaveItem | 0x104528 | 产物入库（已绑定 F_INVEN_SAVE_ITEM_VMA） |
| INVEN_MinusMoney | 0x104780 | 扣费（已绑定 F_MINUS_MONEY_VMA） |
| ITEMSYSTEM_CreatePerfectItem | 0x10c600 | 完美物品创建（含词条继承逻辑） |

**改版定向合成**（v1.3.2 大修）：材料宝石词条类型 ≥2 个相同时，新宝石词条类型取自材料（3 力量必出力量、2力量1敏捷随机）。该逻辑在游戏合成函数内，**走游戏函数自动生效，无需复刻**。

## 3. UI 约束（决定性）

- **hook 不可用**：项目架构 §2.2 确认 ShadowHook 与手写 arm64 inline hook 在 LSPosed 环境均失败（trampoline lr 污染、adrp 重定位）。**不能 hook 绘制函数叠加按钮**。
- 可行路径：**写内存改游戏状态 + 单指令 patch**（不涉及跳转/trampoline）或**直接调游戏函数**。
- UI 相关函数（符号已确认）：UIMix_ButtonDrawMixingGem 0xbf218（宝石按钮绘制）、UIMix_ButtonMixingGemExe 0xbf488（宝石按钮点击）、UIMix_MixGemNextProcess 0xbf2f8、UIMix_FindJewelUpgradeStuff 0xc2164、UIMix_SetType 0xbfe44、UIMix_GetType 0xbf47c。

## 4. 批量合成核心逻辑（方案 A：调游戏合成链，已定）

```
data_op_mix_gem_batch()  // native，第一个背包全部宝石批量合成
1. 遍历 INVEN_pItem(0x7131c0) 第一袋 0x80 槽（16 槽 × 8B 指针）
2. 用 ITEMSYSTEM_IsJewel(0x10b964) 过滤 cat 28-31 宝石（排除混沌宝石 32）
3. 按类别分组计数（cat28/29/30/31 各多少）
4. 每组按 3 个一组分批：
   a. MIXSYSTEM_MakeItem(mixType=12+cat-28, &outItem)   # 走游戏函数，词条继承自动生效
   b. 失败 → 中断并回滚/提示
   c. 消耗 3 个材料（INVEN_RemoveItemDirect，已绑定）
   d. 产物 INVEN_SaveItem(outItem) 入库
5. 费用：MIXSYSTEM_GetCost(mixType, item) 逐批累加 → 一次性 INVEN_MinusMoney
6. 调 SAVE_Save(0x129600) 静默存档？→ 决策点（见 §7）
```

新增符号常量（game_symbols.h）：
- `F_MAKE_MIX_VMA=0x11af58`（MIXSYSTEM_MakeItem）
- `F_GET_COST_VMA=0x11ab64`（MIXSYSTEM_GetCost）
- `F_USE_STUFF_VMA=0x11b300`（MIXSYSTEM_UseStuff，或直接用 RemoveItemDirect 简化）
- `F_IS_JEWEL_VMA=0x10b964`（已存在）

已有先例：`data_op_jewel`（game_ops_action.cpp:594）——「读背包宝石→调 native→消耗材料防刷」完整模式。

## 5. UI 按钮注入方案（待探索，二选一）

### 路径 1：写内存注入自定义控件（首选方向）

- 前提：摸清合成器界面（UIMix）的**控件对象数组**——绘制遍历与点击分发都扫它。
- 目标：在控件数组尾部追加一个自定义按钮控件（贴图引用 + 点击区域 + 回调），点击走游戏自己的控件分发，无需 hook。
- 需探索：
  - UIMix 界面控件数组在内存何处、控件对象结构（贴图 id/坐标/回调指针）
  - 控件点击分发函数（点击坐标 → 控件回调的派发链）
  - 按钮回调目标：直接指向 `data_op_mix_gem_batch()`（native 内部调用链）或现有 UIMix 合成确认弹窗

### 路径 2：单指令 patch 点击分支（备选）

- 找宝石合成按钮（ButtonMixingGemExe 0xbf488）的点击处理，patch 分支条件：
  点击宝石按钮且满足批量条件 → 跳批量逻辑；否则原逻辑。
- 单条分支指令修改（非 inline hook，无 trampoline 问题），复用现有按钮不新增绘制。
- 缺点：无法新增独立按钮，交互与现有按钮耦合。

### 探索路线图

1. 定位 UIMix 控件数组与控件结构（反汇编 UIMix_ButtonDrawMixingGem 的绘制循环）
2. 定位点击分发链（点击事件 → 控件回调）
3. 评估路径 1 可行性（写内存注入控件的完整要素：贴图资源引用/坐标/回调）
4. 不可行则回退路径 2

## 6. 配置接入（jewelBatchMix）

- Kotlin：`ModuleConfig.jewelBatchMix`（已预留，默认 false）
- 启用时：native 注入按钮 + 批量合成逻辑可用；关闭时：不注入、不响应
- 配置变更时机：与 stack-limit 相同，见 wt-stack-limit 文档 §7 时序设计

## 7. 决策点

1. 批量完成后是否自动存档（`F_SAVE_VMA`）？游戏改版已移除强制存档，但批量是玩家显式操作，建议合成后存档。
2. 产物位置：宝石产物进第一个背包（与材料同袋）还是当前背包？
3. 混沌宝石（cat32）不参与批量（已是最高级）。
4. 第一背包装不下产物时的处理（背包满 → 停止并提示）。
5. UI 按钮文案/贴图资源来源（复用游戏现有按钮贴图 vs 模块资源）。

## 8. 验证清单

1. 启用配置后合成器界面出现批量按钮
2. 点击后第一背包低级宝石全部合成中级（3:1 整除部分，余数保留）
3. 产物词条类型遵循改版定向继承规则
4. 费用正确扣减（一次性总额）
5. 材料正确消耗（3 的倍数）
6. 背包满/材料不足时安全停止
7. 关闭配置后按钮消失、功能不响应
8. 合成后存档正确，读档一致

## 9. 参考

- 反汇编：`archive/tmp-exploration/libgame-arm64.dis`
- 合成系统笔记：`archive/tmp-exploration/mixsystem-exploration.md`
- 静态表：`apk/static-data/json/tables/RECIPEBASE.json` / `MIXTUREBASE.json` / `ITEMDATABASE.json`
- 合成文档：`docs/systems/craft.md`
- 物品系统：`docs/systems/inventory.md`
- 项目约束：`docs/architecture.md` §2.2（hook 不可用）、§9.1（OP 端点策略）
