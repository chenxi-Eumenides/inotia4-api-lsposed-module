# 合成系统逆向笔记（Craft/Mix）

> 目录：docs/systems/ ｜ 主题：配方表/合成执行链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/craft/mix` | ⏳ 未实现（配方表 P3） | — | — |

## 2. 已知线索

- **MIXSYSTEM 系列**（合成系统）：
  - `MIXSYSTEM_Initialize`(0x11b8a0)、`MIXSYSTEM_GetRecipeCount`(0x11b394)、`MIXSYSTEM_CalcRecipeListCount`(0x11b470)、`MIXSYSTEM_GetResultItemCount`(0x11aa5c)
  - `MIXSYSTEM_CheckMixture`：仅检查非执行
  - `UIMix_ButtonMixingExe`(0xc21ec)：依赖材料槽选中态（UI）
  - `MIXSYSTEM_MakeItem`(0x11af58)：免配方机直合成（OP 路径）
- 融合器/调合箱配方表结构未逆向（Class D-S 五级/材料合成链，见 game-systems §6.4）
- `UIMix_FindJewelUpgradeStuff`(0xc2164)：宝石升级材料查找

## 3. 待探索方向

1. MIXSYSTEM 配方表结构（配方 ID/材料/产物/消耗）
2. 合成执行链（免 UI 的底层执行函数，绕过材料槽选中态）
3. mix-direct（OP）vs mix（合法）的分级边界

## 4. 相关符号（部分）

| 函数 | VMA | 签名 |
|---|---|---|
| MIXSYSTEM_Initialize | 0x11b8a0 | — |
| MIXSYSTEM_GetRecipeCount | 0x11b394 | — |
| MIXSYSTEM_CalcRecipeListCount | 0x11b470 | — |
| MIXSYSTEM_MakeItem | 0x11af58 | —（OP） |
| UIMix_ButtonMixingExe | 0xc21ec | —（依赖 UI） |
| UIMix_FindJewelUpgradeStuff | 0xc2164 | — |
