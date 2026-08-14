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

## 5. UI 按钮注入方案（已探索定型，2026-08-14）

### 5.1 UIMix 控件系统结构（反汇编已确认）

UIMix **无扁平控件数组**，是「控件树 + 固定指针槽」：

- 每个控件是堆上独立 `ControlObject`（0xf8 字节），父子链表成树
- UIMix 全局状态 `0x305550`（=0x305000+0x550）存一组**固定偏移控件指针槽**

**固定指针槽表**：

| 偏移 | 地址 | 内容 |
|---|---|---|
| +0x00 | 0x305550 | 根 ControlObject* |
| +0x40 | 0x305590 | 材料槽 ControlItem*（`ControlItem_GetItem` 遍历它） |
| +0x60..+0x80 | 0x3055b0..0x3055d0 | 5 个菜单按钮（类型选择，步长 8） |
| +0x90 | 0x3055e0 | 帮助/说明按钮 |
| +0x98 | 0x3055e8 | 合成执行按钮（ExecuteProc=UIMix_ButtonMixingExe） |
| +0xa0 | 0x3055f0 | **宝石合成按钮**（DrawProc=UIMix_ButtonDrawMixingGem） |
| +0xa8 | 0x3055f8 | 滚动条 |
| +0xb8/+0xc0 | 0x305608/0x305610 | 配方上/下按钮 |
| +0xc8 | 0x305618 | 背包物品选择 group（物品槽父节点） |
| +0xf0 | 0x305640 | 背包物品绘制计数 |
| +0xf8 | 0x305648 | 费用（UIMix_ButtonMixingExe 读它判钱） |
| +0x128 | 0x305678 | 当前选中背包槽 index |

**ControlObject 结构（0xf8 字节，`ControlObject_Create @0x9e4ec`）**：

| 偏移 | 大小 | 含义 |
|---|---|---|
| +0x08 | u32 | Type（button=3） |
| +0x0c | u32 | Active（0x20 激活/0x21 非激活） |
| +0x18/+0x20/+0x28/+0x30 | i64×4 | rect x/y/w/h |
| +0x40 | u64 | UserType（0 通用/1 按钮/2 物品） |
| +0x50 | ptr | Data（类型私有数据） |
| +0x78 | u32 | Count（子控件数） |
| +0x88 | u32 | ControlEventCallType（0x100 按下/0x200 点击触发） |
| +0x90 | ptr | Proc（统一绘制/事件分发） |
| +0x98 | ptr | ControlProc（类型事件处理器） |
| +0xa0 | ptr | Parent |
| +0xa8 | 0x10 | ChildList（内嵌 LINKEDLIST） |
| +0xd0 | 0x20 | Sibling（内嵌 LINKEDLISTITEM） |

**按钮私有数据（0x78 字节，`ControlButton_Create @0xaa710`，指针存于 ControlObject+0x50）**：

| 偏移 | 含义 |
|---|---|
| **+0x20** | **ExecuteProc（点击回调函数指针）← 最关键** |
| +0x28 | DrawType（u32） |
| +0x30 | DrawID（贴图 id，-1 默认） |
| +0x38 | DrawSubID |
| +0x60 | DrawProc（绘制函数指针） |
| +0x68 | State（u8：0 正常/1 选中高亮） |

**回调注册表（rela.dyn 全局函数指针表）**：

| 地址 | 值 | 用途 |
|---|---|---|
| 0x2f6d60 | UIMix_ButtonMixingExe(0xc21ec) | 合成按钮 ExecuteProc |
| 0x2f3a40 | UIMix_ButtonDrawMixing(0xbf0f0) | 合成按钮 DrawProc |
| 0x2f4038 | UIMix_ButtonDrawMixingGem(0xbf218) | 宝石按钮 DrawProc |
| 0x2f58e8 | UIMix_ButtonMixingGemExe(0xbf488) | 宝石执行 ExecuteProc |

### 5.2 关键约束：绘制与点击不对称

- **绘制**（`UIMix_Draw @0xc1654`，由 `Scene_Draw_POPUP_SC_MIX @0x14b3e0` 调）：**硬编码枚举固定指针槽**，对每个调 `ControlButton_Draw`。**不遍历树** → 插进 ChildList 的新控件**不会显示**。
- **点击**（`TouchHandle_Event @0xa380c` → `ControlObject_EventProc @0x9e244`）：**递归树遍历**，命中检测 + ExecuteProc 正常触发。

**结论**：新增「可见」按钮必须复用 UIMix_Draw 已绘制的槽；仅插 ChildList 只能点击不可见。

### 5.3 三个方案（已定型）

| 方案 | 做法 | 可见 | 代价 |
|---|---|---|---|
| **1. 改 ExecuteProc（最简）** | 写 `[[0x3055f0]+0x50]+0x20 = 批量合成函数`（改宝石按钮回调） | ✅（复用原按钮显示） | 覆盖原宝石合成功能 |
| **2. 复用槽 + 完整新按钮（推荐）** | 在模块映射内存新建 ControlObject(0xf8)+按钮数据(0x78)，填全字段（Active=0x20、UserType=1、Proc=0xa3590、ControlProc=0xaa818、ControlEventCallType=0x200、rect、Data、ExecuteProc=批量函数、DrawProc=0xbf218 复用贴图），指针写入 UIMix_Draw 已绘制的槽（`[0x3055f0]` 宝石按钮或 `[0x305608]/[0x305610]` 配方按钮） | ✅ | 替换某槽原按钮 |
| 3. 完全新增 | 新控件插入 group ChildList | ❌ 不可见 | 不推荐 |

### 5.4 推荐方案与语义映射

**方案 2，复用 `[0x3055f0]` 宝石按钮槽**——该槽本就是「宝石合成」按钮，替换为「批量宝石合成」按钮语义最契合，且天然可见+可点。

- 原「单次宝石合成」逻辑（`UIMix_ButtonMixingGemExe @0xbf488`）保留在批量函数内部：材料 <3 时回退单次合成，材料 ≥3 走批量
- 复用原宝石按钮 DrawProc（0xbf218）避免处理贴图资源；State 字段（+0x68）控制选中高亮
- 批量合成函数签名：`void cb(ControlObject* ctrl)`（x0=控件对象），native 内部调用链：`data_op_mix_gem_batch()` → 直接读 `g_inven` 第一袋宝石，不走 UIMix 材料槽选中态（规避 P4 卡点）

**实现待定项**：
1. 模块映射内存分配：需 `mmap` 一段 RWX 存新 ControlObject + 按钮数据（项目内存分配机制待查）
2. 新控件字段精确值：Active/Show 位（0x20/0x31）、ControlEventCallType=0x200、Proc/ControlProc 用 rela.dyn 现有地址（0xa3590/0xaa818）
3. 关闭配置时：还原 `[0x3055f0]` 原宝石按钮指针 + 释放 mmap

## 6. 配置接入（jewelBatchMix）

- Kotlin：`ModuleConfig.jewelBatchMix`（已预留，默认 false）
- 启用时：native 注入按钮 + 批量合成逻辑可用；关闭时：不注入、不响应
- 配置变更时机：与 stack-limit 相同，见 wt-stack-limit 文档 §7 时序设计

## 7. 决策点（✅ 已决策 2026-08-14）

1. **批量完成后自动存档**：调 `F_SAVE_VMA` 静默保存（批量是玩家显式操作，存档固化产物）
2. **产物位置**：第一背包（与材料同袋，宝石材料本就在第一袋）
3. **混沌宝石**（cat32）不参与批量（已是最高级）
4. **背包满/材料不足**：安全停止并日志记录（不崩溃、不部分合成）
5. **UI 按钮贴图**：复用原宝石按钮 DrawProc（0xbf218），不引入模块资源

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
