# 合成系统逆向笔记（Craft/Mix）

> 目录：docs/systems/ ｜ 主题：配方表/合成执行链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/action/craft/mix` | ⛔ 卡点（需合成器交互验证材料消耗链，P3） | — | — |
| `POST /api/op/craft/mix-direct` | ⛔ OP（architecture §9.1 约束暂缓） | — | — |

## 2. 合成执行链（✅ 逆向）

### MIXSYSTEM_MakeItem（产物生成核心）
`int(int mixType, void** outItem)` @0x11af58：
- `MIXSYSTEM_IsNeedEuip`(0x11a9f0) 判断是否需要装备（无则返回 0）
- mixType 分支：
  - **0**：`ITEMSYSTEM_MakeChaos(item, 0)`(0x10a5e0) 混沌合成
  - **1**：`ITEMSYSTEM_ApplySocketForMixure(item)`(0x10da64) 开孔合成
  - **其他**：查配方表读产物 itemId → `ITEMSYSTEM_CreatePerfectItem`(0x10c600) 生成完美物品 → 写 `[outItem]`
- 返回 1 成功 / 0 失败

### MIXSYSTEM_CheckMixture（配方检查）
`int(int mixType, void* item)` @0x11ac34：
- `MIXSYSTEM_IsNeedEuip` + item 判空
- mixType 0（混沌）：读 item 类别 + 配方表校验
- 返回：0=通过 / 1=需材料 / 2=无 item

### MIXSYSTEM_MakeRecipeList（配方列表过滤）
`int(int filter, int level, void** outList, int maxCount)` @0x11b5ac：
- 按 filter 位掩码 + level 过滤配方表 `[0x2f3000+0x158]`（记录 +0xa level u8 / +0xb 类型位掩码 u8）
- 匹配配方索引写入 outList

## 3. UI 合成入口（UIMix_ButtonMixingExe 0xc21ec）

```
UIMix_GetType() → 2/3/4 特判（宝石合成等）
通用路径：[0x305000+0x550] 控件 → [x19+0xf8] 合成费用
  → INVEN_GetMoney() < 费用 → 弹"金币不足"（text 10）
  → UIPopupMsg_CreateYesNoFromTextData（确认合成）
```

## 4. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| MIXSYSTEM_MakeItem | 0x11af58 | int(int, void**) |
| MIXSYSTEM_CheckMixture | 0x11ac34 | int(int, void*) |
| MIXSYSTEM_MakeRecipeList | 0x11b5ac | int(int, int, void**, int) |
| MIXSYSTEM_CreateRecipeList | 0x11b7e0 | — |
| MIXSYSTEM_UseStuff | 0x11b300 | —（材料消耗） |
| MIXSYSTEM_GetCost | 0x11ab64 | —（费用） |
| MIXSYSTEM_GetStuffItem | 0x11b05c | — |
| MIXSYSTEM_MakeStuffSlot | 0x11b20c | —（材料槽组装） |
| MIXSYSTEM_GetStuffCount | 0x11b130 | — |
| MIXSYSTEM_IsNeedEuip | 0x11a9f0 | int(int) |
| MIXSYSTEM_pRecipeList | 0x307768 | 8B（配方列表） |
| MIXSYSTEM_pRecipeBook | 0x307760 | 8B（配方书） |
| UIMix_ButtonMixingExe | 0xc21ec | —（UI 合成入口） |
| ITEMSYSTEM_CreatePerfectItem | 0x10c600 | — |
| ITEMSYSTEM_MakeChaos | 0x10a5e0 | — |
| ITEMSYSTEM_ApplySocketForMixure | 0x10da64 | — |

## 5. 卡点原因

- 合法 craft/mix 完整链需：**配方数据结构**（pRecipeList/pRecipeBook）+ **材料槽组装**（MakeStuffSlot）+ **材料消耗**（UseStuff）+ **费用**（GetCost）+ **产物入库**（INVEN_SaveItem）——多函数组装
- **验证前提**：需玩家在合成器（融合器/调合箱）交互点旁触发合成流程（hook MakeItem 验证材料消耗+费用扣款+产物入库）
- 2026-08-09 用户确认：标记卡点收尾（无合成器交互验证条件）
