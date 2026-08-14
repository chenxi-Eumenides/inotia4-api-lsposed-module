# 功能研究：堆叠上限 999（stack-limit-999）

> 状态：研究设计稿（反汇编已确认，未实现）
> 工作区：`wt-stack-limit`（分支 `feature/stack-limit-999`）
> 配置开关：`ModuleConfig.stackLimitIncrease`（config.json，默认 false）
> 日期：2026-08-14

## 1. 功能目标

把背包可堆叠物品的数量上限从 99 提升到 999，全链路正常：

- 背包显示（UI 数字）
- 移动/堆叠合并（`INVEN_MoveItem`）
- 使用/消耗递减（`INVEN_ConsumeItem`）
- 拆堆（`ITEMSYSTEM_Divide`）
- 商店买卖（`UIStore_*`）
- 存档/读档（`SAVE_SaveItem`/`SAVE_LoadItem`）
- 其他可堆叠物品通路（创建/任务/地图掉落/网络商店）

**不改变**：装备耐久、宝石属性、混沌系统、不可堆叠物品语义。

## 2. 现状与阻碍（反汇编证据）

### 2.1 数量位段仅 7bit

物品对象 `+0x10`（u32 位域）中数量占 **bit25-31（7bit，最大 127）**。
999 需要 10bit；若强行把 999 塞进 (31,25)，`999<<25` 溢出 32 位截断后 `&0x7F = 103`，数据被吃。

位段由调用点显式传参决定（`UTIL_GetBitValue/SetBitValue @0x140528/0x140564`）：

```arm64
mov  w1, #0x1f     ; bitEnd=31  （不动）
mov  w2, #0x19     ; bitStart=25 → 改为 #0x16 (22)
```

### 2.2 99 上限 clamp

5 个函数 9 条指令写死 99（`cmp #0x63`/`mov #0x63`），见 §4 清单。

### 2.3 +0x10 完整位图（已全量枚举确认）

```
bit31     25 24      22 18  17  16  15       8  7       0
│ 数量/耐久 │        │宝石│ 混沌值率 │ 混沌等级 │
│ (31,25)  │   子物品数量位段 (0,24)         │
```

| 位段 | 语义 | 使用者 |
|---|---|---|
| bit0-7 | 混沌等级（有符号） | `ITEM_GetChaosLevel 0x105c84` 读；`ITEMSYSTEM_RestoreChaos 0x10d654` 写；`+0x1A` bit0 混沌标志门控 |
| bit8-15 | 混沌值率 | `ITEMSYSTEM_RestoreChaos 0x10d63c` 写 |
| bit0-10 | 宝石数值 | `ITEMSYSTEM_MakeJewel 0x10bc0c` 写 |
| bit11-17 | 宝石词条扩展 | `ITEMSYSTEM_MakeJewel 0x10bbec` 写 |
| bit18-23 | 宝石属性 id | `ITEMSYSTEM_PutJewel 0x10bdd0` 读 |
| **bit0-24** | **子物品数量位段** | `ITEMSYSTEM_CreateItem 0x10bf50` 写静态值；`SAVE_SaveInventory 0x127e0c` / `SAVE_LoadInventory 0x127f50` 读子物品检查 |
| bit25-31 | 数量/耐久 | 42 处读写（可堆叠=数量，装备=耐久 100） |

### 2.4 冲突点（关键发现）

把数量位段扩到 `(31,22)` 后，bit22-24 与 **子物品数量位段 (0,24)** 重叠：

- 堆叠 999 时 `999<<22` 的 bit22-24 = `999&7 = 7 ≠ 0`
- `SAVE_SaveInventory 0x127e0c` 读 `(24,0)` 判断子物品数量 → 误判为有 7 个子物品 → **触发子物品保存循环 → 存档异常**

**必须**同步缩窄子物品检查位段 `(24,0)→(21,0)`（存档 2 处）。子物品数量位段变 22bit（最大 419 万），实际子物品数远小于此，语义无损；混沌位段（bit0-15）仍在 bit0-21 内，混沌物品行为不变。

### 2.5 按物品类型隔离核对

| 类型 | bit22-24 占用 | 冲突 | 说明 |
|---|---|---|---|
| 装备 | 无（耐久在 bit25-31，bit22-24 恒 0） | ✅ | 耐久路径 `(31,25)` 点全部保留不动 |
| 宝石 | 属性 `(23,18)` 与扩展位段 bit22-23 重叠 | ✅ | 宝石不可堆叠（ITEMCLASSBASE 28-32 `+6` bit0=0），**永不进数量位段分支** |
| 药水/卷轴/材料等可堆叠 | bit16-24 恒 0 | ⚠️ 有 | 数量 999 时 bit22-24=7 污染存档子物品检查 → 由 §4③ 修复 |
| 混沌物品 | 混沌等级/值率在 bit0-15 | ✅ | 与 `(31,22)` 无重叠 |

## 3. 方案总览

```
启用（stackLimitIncrease=true）：
  1. native 内存 patch 游戏指令（可逆，保存原字节）
  2. 数据迁移（旧档 ×8 副作用纠正）
关闭（false）：
  1. 还原 patch 指令
  2. 数据回迁（新位段 → 旧位段）
```

## 4. Patch 点完整清单

### ① 位段扩展 `(31,25)→(31,22)`：`mov w2,#0x19`(0x52800322) → `mov w2,#0x16`(0x528002C2)

| 函数 | 地址 | 用途 |
|---|---|---|
| ITEM_GetCumulateCount | 0x10610c | 读数量 |
| INVEN_MoveItem | 0x104aac / 0x104ac8 / 0x104b58 / 0x104b70 | 堆叠合并写 |
| INVEN_ConsumeItem | 0x104814 / 0x10483c / 0x104854 | 消耗读/读/写 |
| ITEMSYSTEM_CreateItem | 0x10c108 | 可堆叠创建数量=1 |
| ITEMSYSTEM_Divide | 0x108460 / 0x108488 / 0x10849c / 0x1084b8 | 拆分 |
| INVEN_SaveItemDirect | 0x103cd8 / 0x103d00 / 0x103d58 | 存入堆叠 |
| INVEN_SaveItemData | 0x10469c | 存数据 |
| INVEN_CheckSaveInNotEmptySlot | 0x103ec4 | 槽检查 |
| INVEN_RemoveItemData | 0x104234 | 删除数据 |
| INVEN_GetCumulateSaveSlotEx | 0x10530c / 0x105324 | 找堆叠槽 |
| UIStore_ButtonSellExe | 0xd18d8 / 0xd18f0 | 商店卖出读数量 |
| UIStore_SellItem | 0xd26c0 | 商店卖出 |
| GAME_StartNewGame | 0x100228 | 初始数量 |
| ITEMSYSTEM_MakeItem | 0x10ca38 | 合成产物 |
| ITEMSYSTEM_ProcessUnpack | 0x10d1a4 | 拆包 |
| MAPITEMSYSTEM_CreateItem | 0x116ec4 | 地图掉落 |
| NetworkStore_AddItem | 0x15d750 | 网络商店 |
| SAVE_ReviseCharacterLocation | 0x126204 | 读 |
| UIMix_StartMix | 0xc09f0 | 合成产物数量 |

⚠️ 待确认（暂不 patch，实机验证后决定）：
- DEALSYSTEM_MakeSale 0xf67a0（商店补货数量 126，SetBitValue(31,25,126)）
- EVTSYSTEM_Process 0xfd19c（剧情数量检查 cmp #0x7e）
- 0x15c138（未知函数）

### ② 99→999 clamp：`#0x63`→`#0x3E7`（cmp 与 mov 同改）

| 函数 | 地址 | 指令 |
|---|---|---|
| INVEN_MoveItem | 0x104a84 | cmp w0,#0x63 → #0x3E7 |
| INVEN_MoveItem | 0x104a8c | mov w23,#0x63 → #0x3E7 |
| INVEN_SaveItemDirect | 0x103ccc / 0x103ce0 | cmp w3,#0x63 / cmp w20,#0x63 → #0x3E7 |
| INVEN_SaveItemData | 0x104690 / 0x1046a0 | cmp w20,#0x62 → #0x3E6 / mov w3,#0x63 → #0x3E7 |
| INVEN_FindSaveSlot | 0x103b98 | cmp w0,#0x63 → #0x3E7 |
| INVEN_GetCumulateSaveSlotEx | 0x105314 / 0x10532c | cmp w0,#0x62 → #0x3E6 / mov w2,#0x63 → #0x3E7 |

### ③ 存档子物品检查缩窄 `(24,0)→(21,0)`：`mov w1,#0x18`(0x52800301) → `mov w1,#0x15`(0x528002A1)

| 函数 | 地址 | 说明 |
|---|---|---|
| SAVE_SaveInventory | 0x127e0c | 子物品数量检查 |
| SAVE_LoadInventory | 0x127f50 | 子物品数量恢复 |

### ④ 不动的点（明确排除）

| 函数 | 地址 | 原因 |
|---|---|---|
| ITEM_IsRealEquip | 0x105b58 | 装备耐久判定（>99） |
| ITEM_IsRealBroken | 0x105c18 | 耐久损坏判定 |
| ITEMSYSTEM_IsEnchantable | 0x10b26c | 可附魔判定 |
| ITEMSYSTEM_CreateItem | 0x10c034 | 装备耐久=100 初始化 |
| UIStore_BuyItem | 0xd2590 / 0xd25d4 | 耐久初始化/动态值 |
| ITEMSYSTEM_CreateItem | 0x10bf50 | 静态值写 `(24,0)`（创建时数量=1，两段不重叠） |

### ⑤ 误报点（mov w2,#0x19 是函数参数非位域，不 patch）

CHAR_SuccessOnMagicalAttack 0xed1cc、ITEMSYSTEM_GetEquipMinLevel 0x108550、ITEM_GetDamage 0x109c2c、ITEM_GetDefense 0x109efc、WORLDMAPBUILDER_LinkByMapID 系列 ~20 处。

## 5. 模块侧同步改动

| 文件 | 位置 | 改动 |
|---|---|---|
| game_ops_value.cpp | data_op_add_item 行 51-56 | 99 判定改为 999；掩码 `0x7F800000`→`0x7FC00000` |
| game_ops_action.cpp | inventory_gained_json 行 43/47/53 | 数量位段读取 (31,25)→(31,22) |

## 6. 数据迁移（启用/关闭双向）

### 6.1 旧格式检测

物品 +0x10：
- 旧格式（未迁移）：`(值>>25)&0x7F ≠ 0` 且 `(值>>22)&0x7 == 0`（bit22-24 空）
- 新格式：`(值>>22)&0x3FF` 直接是数量

### 6.2 启用迁移（旧 → 新）

对背包每个可堆叠物品（`ITEMCLASSBASE +6 bit0=1` 判定，复用 `game_read.cpp:210` 的 `item_is_equip()` 同源逻辑）：

```
N_old = (值>>25)&0x7F
若 N_old ≠ 0：
    值 = (值 & ~0x3FF00000) | (N_old << 22)   # 数值保持 N_old，不 ×8
    写回 +0x10
```

效果：旧档 50 个药水迁移后仍是 50 个（而非 8 倍）。

### 6.3 关闭回迁（新 → 旧）

```
X = (值>>22)&0x3FF
若 X ≠ 0：
    若 X ≤ 127：值 = (值 & ~0x3FF00000) | (X << 25)   # 完整回迁
    若 X > 127：数量超出 7bit 表达范围 → 决策点（见 6.5）
    写回 +0x10
```

### 6.4 迁移时机与幂等

- 时机：游戏进入 world 状态（`STATE_nState==5`）后、任何数量读写发生前执行一次；由 native 层函数 `data_op_migrate_stack()` 触发（对应新增 JNI/内部调用）
- 幂等：迁移后再迁移是无操作（旧格式检测不命中）；用模块内存标志位防止重复执行
- 迁移完成后调用 `F_SAVE_VMA`（0x129600 无参静默保存）固化

### 6.5 关闭时 X>127 的处理（决策点）

选项：
1. 截断到 127（数据丢失，警告日志）
2. 保持数据不迁移（旧位段读到 `X>>3`，数量错误但可逆——下次启用时无损恢复）
3. 提示玩家先消耗（游戏内无法弹窗，可行性低）

推荐 2（可逆优先），实机验证后定。

## 7. 时序与配置传递（设计要点）

1. `HookMain` 启动 → `bridge_init()`（native 初始化）
2. `ModuleConfig.load(context)` 读取 `stackLimitIncrease`（Kotlin，最早可行处）
3. Kotlin 通过 JNI 通知 native 启用/关闭：`nativeSetStackLimitEnabled(bool)`
4. native 执行 patch（保存原字节）→ 迁移

⚠️ **风险点**：patch 必须在**游戏读档之前**生效，否则读档已用旧位段载入数据，迁移时机错乱。
实现时需验证：读档（`SAVE_LoadData`）发生在 bridge_init 之后还是 ApiServer.start 之后？
若读档先于配置读取，需把 ModuleConfig 读取提前到 HookMain 最早处。

## 8. 验证清单

1. 移动物品堆叠 >99（100+）显示与合并正确
2. 使用/消耗递减正确（999→998）
3. 拆堆 999 → N + (999-N) 正确
4. 存档 → 读档数量保持
5. 商店卖出/买入 999 数量正确
6. 装备耐久不受影响（磨损正常）
7. 宝石属性显示正常（bit22-23 未被污染）
8. 混沌物品存档正常（子物品检查位段缩窄后）
9. 启用迁移：旧档数量保持不变（不 ×8）
10. 关闭回迁：新档数量回迁（≤127 完整）
11. 配置 false 时完全无行为变化（patch 不生效）

## 9. 参考

- 反汇编：`archive/tmp-exploration/libgame-arm64.dis`（VMA 与文件偏移一致）
- 符号表：`apk/decompiled/libgame-symbols.txt`
- 存档机制：`docs/systems/save.md`（SAVE_SaveItem/LoadItem 全宽无损序列化）
- 物品系统：`docs/systems/inventory.md`
- 位段枚举脚本：`.tmp/enumerate_count_bits.py`
