# 移动系统逆向笔记（Movement）

> 目录：docs/systems/ ｜ 主题：移动/寻路/方向键模拟全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/movement/move` | `CHAR_SearchPath` + `CHAR_MoveAsPath` 循环 | 早前 | ✅ 真机 |
| `/api/action/movement/move/cancel` | `CHAR_RemovePath`(0xdb064) | v0.4.1 | ✅ 真机 |
| `/api/action/movement/walk` | `CHAR_Move`(0xe9808) 每帧方向移动 | v0.4.1 | ✅ 真机 |
| `/api/action/movement/walk/stop` | `CHAR_RemovePath` | v0.4.1 | ✅ 真机 |

## 2. 移动机制（✅ 完整逆向）

**玩家真实移动机制** = 方向键长按 → 游戏主循环每帧 `CHAR_Move`。API move 实现 = `CHAR_SearchPath` 计算路径 + **临时清零 +0x2e2 控制态 + 循环调用 `CHAR_MoveAsPath` 走完 PATHLIST（上限 512 步）+ 还原控制态**（仍走游戏合法寻路链路，非 OP 传送）。

```
CHAR_SearchPath(ch, tx, ty, flag) @0x13ba14：寻路计算 → 结果存 [ch+0x2F0] PATHLIST 链表
CHAR_MoveAsPath(ch) @0xe9db8：沿 PATHLIST 走一步（玩家控制态 +0x2e2≠0 需 +0x278 目标指针非空；只走一步不续走）
CHAR_Move(ch, mode, delta, flag) @0xe9808：方向键移动，mode 0-3 = 上/下/右/左，delta=8 像素/帧
CHAR_RemovePath(ch) @0xdb064：清路径（cancel 用）
```

**walk 方向映射**（v0.4.1 真机验证）：
- mode 0 = 下（y+）、1 = 左（x-）、2 = 上（y-）、3 = 右（x+）
- delta **值传递**（非指针！）——v0.4.1 踩坑：误 typedef 为 int* 传 &delta 导致不动（反汇编 sub w21,w20,w2 直接值减）

**角色移动相关字段**：
- +0x2E2：控制状态（0=AI / 7=玩家 / 135=战斗）
- +0x2F0：PATHLIST 寻路结果链表（节点 +0x00 u16 网格x、+0x02 u16 网格y、+0x08 next）
- +0x278：移动目标指针（MoveAsPath 前置条件）

## 3. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| CHAR_SearchPath | 0x13ba14 | int(void*, int32_t, int32_t, int32_t) |
| CHAR_MoveAsPath | 0xe9db8 | int(void*) |
| CHAR_Move | 0xe9808 | int(void*, int32_t, int32_t, uint8_t) |
| CHAR_RemovePath | 0xdb064 | void(void*) |
