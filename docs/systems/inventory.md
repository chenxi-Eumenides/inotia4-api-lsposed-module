# 背包物品系统逆向笔记（Inventory）

> 目录：docs/systems/ ｜ 主题：背包结构/物品操作（使用/丢弃/装备/出售/移动/镶嵌）全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/inventory/use-item` | `ITEMDATABASE_IsUse`(0x1058ac) 校验 + `INVEN_ConsumeItem`(0x1047bc) | 早前 | ✅ 真机 |
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
- 物品类别：`UTIL_GetBitValue(item+0x08, 15, 6)`
- 物品数量：`UTIL_GetBitValue(item+0x10, 31, 25)`
- 物品稀有度：`ITEMSYSTEM_GetRarity`(0x10d700)
- 名称联查：category = (typeFlags >> 6) & 0x3FF = ITEMDATABASE itemId → 静态表 text_0

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
