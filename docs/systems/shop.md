# 商店系统逆向笔记（Shop）

> 目录：docs/systems/ ｜ 主题：商店商品/价格表/购买链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/shop/buy` | ⏳ 未实现（商店数据结构 P1 依赖） | — | — |

## 2. 已知线索

- **UIStore 商品列表/价格表（DEALSYSTEM）未逆向**——shop/buy 的前置依赖
- `UIStore_BuyItem(price)` @0xd242c：`ControlObject_GetCursor` 取 UI 选中商品（**依赖商店界面打开状态**）
- `UIStore_SellItem` @0xd25f0：依赖 cursor 选中态
- api-technical-spec：商店买卖依赖 P1「商店物品/价格数据结构」完成后，探索底层购买/出售函数（绕过 cursor）

## 3. 待探索方向

1. 反汇编 `DEALSYSTEM_FindSaleByID` + UIStore 初始化链 → 商店商品表结构（商品 ID/价格/库存）
2. 商店买卖的底层函数（绕过 cursor 选中态）——`DEALSYSTEM_*` 系列
3. 购买消耗金币校验（INVEN_MinusMoney 链）

## 4. 相关符号（待查）

> 符号表检索词：`DEALSYSTEM`、`UIStore`、`Sale`、`Shop`
