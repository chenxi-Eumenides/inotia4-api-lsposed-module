# 移动系统逆向笔记（Movement）

> 目录：docs/systems/ ｜ 主题：移动/寻路/方向键模拟全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/movement/move` | `CHAR_SearchPath` + `CHAR_MoveAsPath` 循环 + **`MAP_SetFocus` 摄像机同步 + 每步 `GAMEPLAY_GoMapLinkByChar` 切图检测** | v0.4.24-25 | ✅ 真机（含切图 3080↔2056） |
| `/api/action/movement/move/cancel` | `CHAR_RemovePath`(0xdb064) | v0.4.1 | ✅ 真机 |
| `/api/action/movement/walk` | `CHAR_Move`(0xe9808) flag=**0**（自动 `MAP_SetFocus` 跟随）+ **每帧** `GAMEPLAY_GoMapLinkByChar` 切图检测 | v0.4.24-25 | ✅ 真机（摄像机跟随+切图） |
| `/api/action/movement/walk/stop` | `CHAR_RemovePath` | v0.4.1 | ✅ 真机 |

## 1.5 摄像机（MAP Focus）与切图（v0.4.24）

**摄像机 = MAP Focus 体系**（无 CAMERA 符号）。全局：焦点X `[0x2f3000+0x340]`、焦点Y `[0x2f4000+0x3c0]`（像素）；渲染消费 4 个滚动偏移（`GAMEPLAY_DrawFocus` 0x9d3ec 实证）。

- `MAP_SetFocus(x,y)` @0x11336c：**像素坐标**，写焦点 + `MAP_SetDisplayInformation`(0x11256c) 转滚动偏移，立即跳变
- `MAP_SetFocusMove(x,y,speed)` @0x11374c：平滑插值（帧计数×速度），每帧 `MAP_MoveFocus`(0x113870) 推进
- **根因（v0.4.23 bug）**：`CHAR_Move` 第 4 参 flag≠0 时跳过 `MAP_SetFocus`（e9a40 cbnz）→ 坐标变画面不动。游戏正常路径传 flag=0；模块 walk 曾传 flag=1 → 摄像机不同步。**v0.4.24 改 flag=0 修复**

**切图链（完整，官方路径）**：
```
GAMEPLAY_GoMapLinkByChar(ch, tileX, tileY) @0x9cdc0
  → GAMEPLAY_CheckMapLink(ch, x, y) @0x9cc28
      读 [0x2f3000+0xf48] 解引用瓦片网格（y*64+x 索引，byte bit7=1 表示有出口）
      有出口 → MAP_FindMapLink(0x112aac) / MAP_FindMapLinkNoDir(0x112b2c)
  → GAMEPLAY_GoMapLink(ch) @0x9cc74
      状态≠5 时：MAPCHANGE_Set(mapId,x,y,dir) @0x9c740 + 复活检查 + GAMESTATE_SetState(3)
  → 切图状态机 GAMESTATE_ProcessMapChange @0x9c7ec
      状态0-3 渐隐 alpha+=0x19/帧；状态4 MAPSYSTEM_ChangeMap(0x114fc4)；状态11 GAMESTATE_SetState(0)
```

**模块实现（v0.4.25 每步检测）**：move 循环内**每步** MoveAsPath 后、walk 循环内**每帧** CHAR_Move 后调 `fn_go_map_link_by_char(ch, px>>4, py>>4)`（像素>>4=tile）触发出口检测，命中（返回 1）立即 break 提前终止；move 末尾再兜底检测一次 + 显式 `fn_map_set_focus(px,py)`。真机验证：move 到出口 tile → 3080→2056 切图成功；walk 出口处也切图 2056→3080。切图由 GoMapLink 内部 `GAMESTATE_SetState(3)` 驱动状态机（渐隐→加载新图→恢复），期间游戏自动暂停。

**出口 tile 探测法**（frida）：扫描瓦片网格 `[0x2f3000+0xf48]` 解引用，`(byte & 0x80)!=0` 即出口（map 3080 出口在 tile (24,19)-(24,22)）。

## 2. 移动机制（✅ 完整逆向）

**玩家真实移动机制** = 方向键长按 → 游戏主循环每帧 `CHAR_Move`。API move 实现 = `CHAR_SearchPath` 计算路径 + **临时清零 +0x2e2 控制态 + 循环调用 `CHAR_MoveAsPath` 走完 PATHLIST（上限 512 步）+ 还原控制态**（仍走游戏合法寻路链路，非 OP 传送）。

```
CHAR_SearchPath(ch, tx, ty, flag) @0x13ba14：寻路计算 → 结果存 [ch+0x2F0] PATHLIST 链表
CHAR_MoveAsPath(ch) @0xe9db8：沿 PATHLIST 走一步（玩家控制态 +0x2e2≠0 需 +0x278 目标指针非空；只走一步不续走）
CHAR_Move(ch, mode, delta, flag) @0xe9808：方向键移动，mode 0-3 = 上/下/右/左，delta=8 像素/帧；flag=0 时内部自动 MAP_SetFocus
CHAR_RemovePath(ch) @0xdb064：清路径（cancel/walk_stop 共用）
```

**walk 方向映射**（v0.4.1 真机验证）：
- mode 0 = 下（y+）、1 = 左（x-）、2 = 上（y-）、3 = 右（x+）
- delta **值传递**（非指针！）——v0.4.1 踩坑：误 typedef 为 int* 传 &delta 导致不动（反汇编 sub w21,w20,w2 直接值减）

**角色移动相关字段**：
- +0x2E2：控制状态（0=AI / 7=玩家 / 135=战斗）
- +0x2F0：PATHLIST 寻路结果链表（节点 +0x00 u16 网格x、+0x02 u16 网格y、+0x08 next）
- +0x278：移动目标指针（MoveAsPath 前置条件）
- +0x02/+0x04：像素坐标 int16（`C_POS_X`/`C_POS_Y`）

## 3. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| CHAR_SearchPath | 0x13ba14 | int(void*, int32_t, int32_t, int32_t) |
| CHAR_MoveAsPath | 0xe9db8 | int(void*) |
| CHAR_Move | 0xe9808 | int(void*, int32_t, int32_t, uint8_t) |
| CHAR_RemovePath | 0xdb064 | void(void*) |
| MAP_SetFocus | 0x11336c | void(int32_t, int32_t)（像素坐标） |
| GAMEPLAY_GoMapLinkByChar | 0x9cdc0 | int(void*, int32_t, int32_t)（tile 坐标） |
| MAPCHANGE_Set | 0x9c740 | void(int32_t, int32_t, int32_t, int32_t) |
| GAMESTATE_ProcessMapChange | 0x9c7ec | void() |
| MAP_FindMapLink | 0x112aac | int(void*, int32_t, int32_t) |
