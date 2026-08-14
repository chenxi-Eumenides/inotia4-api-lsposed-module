# 商店系统逆向笔记（Shop）

> 目录：docs/systems/ ｜ 主题：商店商品/价格表/购买链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/item/shop/buy_item` | DEALSYSTEM 表定位 + ITEM_GetBuyPrice + INVEN_SaveItem + MinusMoney | v0.4.14 | ✅ 真机 |
| `GET /api/item/shop/items` | 遍历 DEALSYSTEM_pSaleList | v0.4.14 | ✅ 真机 |

## 2. 商店商品表（✅ 逆向 + frida 真机采样）

**`DEALSYSTEM_pSaleList` @0x712a60**（GOT 槽 0x2f3000+0x490，**表基址 = GOT 值，不二次解引用**）：

```
表 = [0x2f3000+0x490] 值（48 槽 × 16B）
槽 i（步长 0x10）：
  +0x00 位域（bit0=空/已售，flags=1 空）
  +0x08 商品对象指针（item 结构）
    item 类别 = (item+0x08 u16 >> 6) & 0x3FF
    item 数量 = (item+0x10 u32 >> 25) & 0x7F
    item 买入价 = ITEM_GetBuyPrice(item)
```

- `DEALSYSTEM_FindSaleByID(item)` @0xf636c：遍历表（基址 → +0x300）按 item 类别匹配返回槽指针
- `DEALSYSTEM_AddSale`(0xf6444)：玩家卖货给商店（FindEmptySaleSlot 0xf6338 + AddSaleDirect 0xf62fc）
  - **签名（v0.5.16 objdump 逆向确认）**：
    - `DEALSYSTEM_AddSale(void* item) → int`：FindEmptySaleSlot 找空槽 → AddSaleDirect 存入 → 返回槽号 0-47 或 -1（表满）
    - `DEALSYSTEM_AddSaleDirect(void* item, int slot) → int`：`slot>47` 或该槽已占用返回 0；否则 `*(slot_ptr+8)=item`、清 bit0（标记占用）返回 1
    - `DEALSYSTEM_FindEmptySaleSlot() → int`：遍历 48 槽（bit0=空）返回首个空槽号，无空槽 -1
  - ⚠️ **出售端点未实现**：AddSale 将 item 指针移入商店表（同一指针），但背包槽仍需「去引用而不 ITEMPOOL_Free」（否则悬空/双释放），此所有权转移未真机验证；当前「卖物品换钱」由全局 `POST /api/item/inventory/sell_item`（v0.4.3，ITEM_GetPrice÷5）覆盖，商店 buyback 出售留待后续。
- 当前商店实测 11 商品：cat 5/6/7/8/26/27/1/2/62/19/24（slot0-10），slot11+ 空
- ⚠️ **非商店界面时表为空**（DEALSYSTEM 仅在商店加载时填充）——shop/items 返回 `{"items":[]}`

## 3. 买入链（UIStore_BuyItem 0xd242c 反汇编 + API 绕过）

```
UIStore_BuyItem(price)（UI 依赖，API 不调用）：
  INVEN_GetMoney() → price>money 弹"金币不足"返回
  ControlObject_GetCursor(0x307000+0x3e0+0x8 控件) + ControlItem_GetItem 取选中商品（依赖 cursor）
  DEALSYSTEM_FindStatic(0xf6c84) 静态校验 → INVEN_FindSaveSlot(0x103960) 找空槽
  → INVEN_SaveItem(0x104528) 存入 → 扣款
```

**API 实现（绕过 cursor，纯函数链）**：商品定位（按 slot 遍历 saleList）→ 金币校验（INVEN_GetMoney ≥ price，不足 `not enough money`）→ INVEN_FindSaveSlot 找空槽 → INVEN_SaveItem 存入 → INVEN_MinusMoney(price) 扣款

**价格函数**：
- `ITEM_GetBuyPrice`(0x10a200)：买入价 = ITEM_GetPrice(0x109f50) + MERCENARYGROUPSKILLSYSTEM 折扣系数
- `ITEM_GetPrice`(0x109f50)：静态表价格（v0.4.3 sell 已用）

## 4. 真机验证（v0.4.14）

- 寻路到商人（movement/move 到相邻格）→ interact → npc/select {index:0} → `screen=shop`
- GET shop/items：11 商品（cat5 恢复药水 price15 / cat6 price60 / cat7 price210 / cat8 price720 / cat26 price65 / cat27 price850 / cat1 price90 / cat2 price540...）
- POST shop/buy {slot:0}：金币 81→66（-15 扣款正确）+ 背包 cat5 恢复药水（小）count15 入库 ✅

## 5. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| ITEM_GetBuyPrice | 0x10a200 | int(void*) |
| INVEN_FindSaveSlot | 0x103960 | int(void*, int32_t) |
| INVEN_SaveItem | 0x104528 | int(void*, void*) |
| DEALSYSTEM_FindSaleByID | 0xf636c | void*(void*) |
| DEALSYSTEM_AddSale | 0xf6444 | int(void*)（卖货：FindEmptySaleSlot+AddSaleDirect） |
| DEALSYSTEM_AddSaleDirect | 0xf62fc | int(void*, int32_t)（item+slot，存槽清 bit0） |
| DEALSYSTEM_FindEmptySaleSlot | 0xf6338 | int(void)（首个空槽 0-47/-1） |
| UIStore_BuyItem | 0xd242c | —（UI 依赖 cursor） |
| UIStore_SellItem | 0xd25f0 | —（UI 依赖 cursor） |
| UIStore_ButtonBuyExe | 0xd1710 | —（cursor→BuyPrice→确认弹窗） |
| ITEM_GetPrice | 0x109f50 | int(void*)（已有） |
| INVEN_GetMoney | 0x10445c | int64(void)（已有） |
| INVEN_MinusMoney | 0x104780 | int(int64)（已有） |
