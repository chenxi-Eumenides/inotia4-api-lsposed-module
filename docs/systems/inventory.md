# 背包物品系统逆向笔记（Inventory）

> 目录：docs/systems/ ｜ 主题：背包结构/物品操作（使用/丢弃/装备/出售/移动/镶嵌）全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/inventory/use-item` | **4 路分派**（v0.4.20）：IsDice→报错 / IsSealed→ReleaseSealed+Consume / IsItemBox→OpenItemBox+Consume / 其余→CHAR_UseItemEx(0xeb670) | v0.4.20 | ✅ 真机（药水 50→1504 满血、CD 反馈） |
| `/api/action/inventory/discard` | `INVEN_RemoveItemDirect`(0x103fd8) | 早前 | ✅ 真机 |
| `/api/action/inventory/{role}/equip` | `CHAR_CanEquipItem`(0xe4eb4) + `CHAR_EquipItem`(0xe51c0) | 早前 | ✅ 真机 |
| `/api/action/inventory/{role}/unequip` | `CHAR_UnequipItemToInven`(0xe2f68) | 早前 | ✅ 真机 |
| `/api/action/inventory/sell` | `ITEM_GetPrice`(0x109f50) + `INVEN_RemoveItemDirect` + `INVEN_AddMoney`(0x1044e4) | v0.4.3 | ✅ 真机 |
| `/api/action/inventory/move` | `INVEN_MoveItem`(0x104934) | v0.4.4 | ✅ 真机 |
| `/api/action/inventory/{role}/jewel` | `ITEMSYSTEM_PutJewel`(0x10bcb4) | v0.4.6 | ✅ 真机 |

## 2. 背包结构（✅ 已破解）

```
INVEN_pItem (0x7131c0) = 6 袋 × 0x80 步长，每袋 16 槽 × 8B 物品指针
物品指针 = INVEN_pItem + bag*0x80 + slot*8（0 = 空槽）
物品结构：+0x08 类型位域(u16) / +0x10 数量位域(u32) / +0x19 宝石位域
```
- 物品类别：`UTIL_GetBitValue(item+0x08, 15, 6)` = `(typeFlags >> 6) & 0x3FF`
- 物品数量：`UTIL_GetBitValue(item+0x10, 31, 25)`
- 物品稀有度：`ITEMSYSTEM_GetRarity`(0x10d700)
- 名称联查：category = (typeFlags >> 6) & 0x3FF = ITEMDATABASE itemId → 静态表 text_0

## 2.5 使用物品分派体系（✅ v0.4.20 反汇编完整梳理）

> 权威分派依据 = `UIEquip_SetDescMenu`(0xb8504) 背包右键菜单按钮判定链（disasm 行 43391-43678）。
> 按钮判定从上到下 if/else，命中即显示对应按钮；按钮执行函数不再重复判定类别。

### 2.5.1 UI 按钮判定链（权威类别归属）

| 按钮 | 判定函数 | 类别范围 |
|---|---|---|
| 装备 | `ITEM_IsRealEquip`(0x105ab8) + `CHAR_CanEquipItem`(0xe4eb4) | 真装备且当前角色可穿 |
| 使用（快捷） | `ITEMSYSTEM_IsShortcutUse`(0x10cd04) | ITEMDATABASE 记录 off7 bit0 或 off6 bit5，或 IsReviveScroll |
| 佣兵卡 | `ITEMSYSTEM_IsMercenarySeal`(0x10be70) | [0x2a,0x32] ∪ [0x3a0,0x3a5] |
| 掷骰 | `ITEMSYSTEM_IsDice`(0x10be60) | **[0x34,0x38]** = 52~56 |
| 使用（确认后） | `ITEMSYSTEM_IsUseAfterConfirm`(0x10cca4) | {0x33, 0x39d, 0x39e} ∪ {0xf 超药水} |
| 使用 | ITEMDATABASE off7 bit1 或 id==0x3e | — |
| 使用（解包） | `ITEMSYSTEM_IsPackItem`(0x10cdb0) | PackItem 表遍历匹配 |
| 解封 | `ITEMSYSTEM_IsSealed`(0x10be50) | **[0x3a6,0x3ab]** = 934~939 |
| **开箱** | `ITEMSYSTEM_IsItemBox`(0x10cda0) | **[0x3ef,0x3f1]** = 1007~1009 |

⚠️ 开箱判定是 `IsItemBox`（0x3ef-0x3f1），**不是 IsPackItem**（那是"打包物"走 ProcessUnpack 解包）。

### 2.5.2 CHAR_UseItemEx(0xeb670) 覆盖的类别（走"使用"按钮路径）

签名 `int (void* ch, void* item, int flag)`，类别提取 = `UTIL_GetBitValue(item+8, 0xf, 0x6)`。返回 1=成功（内部已调 INVEN_ConsumeItem）、0=失败（CD/状态不符，不消耗）。

| 类别 | 物品 | 分派 |
|---|---|---|
| 5-8 | 恢复药水 | ITEMCLASSBASE off6 bit5 → `FindInRecover` + CAL_Calculate 公式 + `PARTY_AddHPMP`(AsRate) |
| 9 | 魔法药丸 | 快速回血路径 0xeb8d0 |
| 26/27 (0x1a/0x1b) | 复活卷轴 | `CHAR_ProcessReviveScroll`(0xde3d8) |
| 51 (0x33) | 技能书 | `CHAR_ProcessSkillBook`(0xe2488) |
| 62 (0x3e) | 佣兵封印卡 | `CHARSYSTEM_Produce`(0xf3880) + `CHAR_AppearFromCharacter` |
| 925 (0x39d) | 技能重置 | `CHAR_InitializeSkill`(0xe67c8) |
| 926 (0x39e) | 属性重置 | `CHAR_InitializeStatus`(0xe68c8) |
| 15 (0xf) | 超级药水 | `ITEMSYSTEM_IsSuperPotion` → `CHAR_ProcessSuperPotion`(0xdcc88) |
| 默认 | 配方书 | ITEMCLASSBASE off7 bit1 → `CHAR_ProcessRecipe`(0xdcde4) |
| 默认 | 增益类 | ITEMCLASSBASE off7 bit0 → `FindInBuff` + `CHAR_CreateBuff` |
| 默认 | 打包物 | `ITEMSYSTEM_IsPackItem` → `ITEMSYSTEM_ProcessUnpack`(0x10ce50) |

### 2.5.3 独立路径（不在 CHAR_UseItemEx 内，API 前置分派）

| 类别 | 判定 | 执行链 | API 状态 |
|---|---|---|---|
| 0x34-0x38 骰子 | IsDice | `UIRollDice_Create`(0xccc74) + `STATUSDICE_Roll`(0x138338) | ⛔ 依赖 UI 面板，返回 `dice requires UI interaction` |
| 0x3a6-0x3ab 解封 | IsSealed | `ITEMSYSTEM_ReleaseSealed`(0x10af4c) 成功→手动 `INVEN_ConsumeItem` | ✅ v0.4.20 |
| 0x3ef-0x3f1 开箱 | IsItemBox | `ITEMSYSTEM_OpenItemBox`(0x10e970) 成功→手动 `INVEN_ConsumeItem` | ✅ v0.4.20 |

### 2.5.4 恢复表结构（ITEMRECOVERBASE，药水回血数据源）

- 运行时表：stride=`[0x2f5000+0x890]` 字节(9)、表数据=`[0x2f6000+0x850]` **双重解引用**；记录按**顺序索引**（cat5→0, cat6→1...cat14→5），非类别值索引
- 记录 9B：byte0-1=类别(u16)、byte2=标志、byte3-4=文本ID(u16)、**byte5-6=回血下限公式ID(u16)**、byte7-8=回血上限公式ID(u16)
- 公式：formula-e[127]='2400' / [128]='3200'（cat5 恢复药水小）；恢复量 = MATH_GetRandom(下限公式值, 上限公式值)
- 回血前置标志：ITEMCLASSBASE off6 bit5（回血类）+ byte2 bit1（可消耗）；`[ch+0x2c8] & 0x10004`（死亡状态）拒绝
- ⚠️ 踩坑：OpenItemBox 对**任何类别**都返回 1（表记录数==0 时也返回随机+1）——分派时必须先用 IsItemBox 判定，否则药水会被误拦（v0.4.20 真机实证）

## 3. 出售链（✅ v0.4.3，防刷钱设计）

```
ITEM_GetPrice(item) @0x109f50：价格 = 静态表 ITEMDATABASE（item+8 类别 → 价格字段）+ ITEM_GetAbilityLevel 加成
INVEN_RemoveItemDirect(bag, slot) @0x103fd8：按槽删除
INVEN_AddMoney(money) @0x1044e4：加金币（内部溢出检查）
```
**价格由静态表决定，非调用方传入**（防任意定价刷钱）——2026-08-08 审查修正归合法类别。返回 `{"ok":true,"price":N}`

## 4. 移动/堆叠链（✅ v0.4.4）

```
INVEN_MoveItem(item, count, targetBag, targetSlot) @0x104934：
  目标空槽 → ITEMSYSTEM_CopyAsNewUID(0x1083c8) 复制 + INVEN_SaveItemDirect(0x103bf0) 存入
  目标同类 → 堆叠合并（上限 99）
  源数量 UTIL_SetBitValue 写回 +0x10
  返回 w0=1 成功/0 失败；前置限制同 bag 类型 + 类别比较
```
⚠️ INVEN_RemoveItem(0x104044) 语义为 `int(void* item)`（**item 指针**非类别）——v0.4.3 修正旧记录

## 5. 镶嵌链（✅ v0.4.6）

```
ITEMSYSTEM_PutJewel(equipItem, jewelItem) @0x10bcb4：
  装备类别可镶嵌校验（0x2f5000+0xb60 表 bit0）
  ITEMSYSTEM_IsJewel(0x10b964) 宝石校验（非宝石→3）
  装备 +0x19 bit4-6 插槽数 ≤0 → 2
  ITEM_AddOptionEx(0x105ec4) 加属性 → +0x19 bit0-2 已镶数 +1
  返回 0=成功 / 2=无孔 / 3=非宝石或空装备
```
**⚠️ 不消耗宝石物品本身**——API 手动 `INVEN_RemoveItemDirect(bag,slot)` 删除防刷（v0.4.6 真机验证宝石槽清空）
- 装备插槽访问：角色 +0x1F8（C_EQUIP，10 槽×8B 指针）
- 装备宝石位域 +0x19：bit0-2=已镶宝石数、bit4-6=插槽等级

## 6. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| ITEMDATABASE_IsUse | 0x1058ac | int(int32_t) |
| INVEN_ConsumeItem | 0x1047bc | void(void*) |
| CHAR_UseItemEx | 0xeb670 | int(void*, void*, int) |
| ITEMSYSTEM_OpenItemBox | 0x10e970 | int(int32_t) |
| ITEMSYSTEM_ReleaseSealed | 0x10af4c | int(int32_t) |
| ITEMSYSTEM_IsDice | 0x10be60 | int(int32_t) |
| ITEMSYSTEM_IsSealed | 0x10be50 | int(int32_t) |
| ITEMSYSTEM_IsItemBox | 0x10cda0 | int(int32_t) |
| ITEMSYSTEM_IsPackItem | 0x10cdb0 | int(int32_t) |
| ITEMSYSTEM_IsShortcutUse | 0x10cd04 | int(int32_t) |
| ITEMSYSTEM_IsUseAfterConfirm | 0x10cca4 | int(int32_t) |
| ITEMSYSTEM_IsMercenarySeal | 0x10be70 | int(int32_t) |
| ITEMSYSTEM_IsReviveScroll | 0x10ccf4 | int(int32_t) |
| ITEMSYSTEM_FindInRecover | 0x10aa8c | int(int32_t) |
| ITEMSYSTEM_ProcessUnpack | 0x10ce50 | int(int32_t) |
| CHAR_ProcessRecipe | 0xdcde4 | int(int32_t) |
| CHAR_ProcessReviveScroll | 0xde3d8 | int(void*, int32_t) |
| CHAR_ProcessSkillBook | 0xe2488 | int(void*, void*) |
| INVEN_RemoveItemDirect | 0x103fd8 | int(int32_t, int32_t) |
| INVEN_RemoveItem | 0x104044 | int(void*) |
| INVEN_MoveItem | 0x104934 | int(void*, int32_t, int32_t, int32_t) |
| INVEN_SaveItemDirect | 0x103bf0 | int(void*, int32_t, int32_t) |
| ITEMSYSTEM_CopyAsNewUID | 0x1083c8 | void*(void*) |
| ITEM_GetPrice | 0x109f50 | int(void*) |
| INVEN_AddMoney | 0x1044e4 | int(int64_t) |
| ITEMSYSTEM_PutJewel | 0x10bcb4 | int(void*, void*) |
| ITEMSYSTEM_IsJewel | 0x10b964 | int(int32_t) |
| ITEM_AddOptionEx | 0x105ec4 | int(void*, ...) |
| CHAR_EquipItem | 0xe51c0 | int(void*, void*) |
| CHAR_CanEquipItem | 0xe4eb4 | int(void*, void*) |
| CHAR_UnequipItemToInven | 0xe2f68 | int(void*, int32_t) |
