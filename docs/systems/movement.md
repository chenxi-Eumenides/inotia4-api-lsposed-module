# 移动系统逆向笔记（Movement）

> 目录：docs/systems/ ｜ 主题：移动/寻路/方向键模拟全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/world/movement/move` | **自研 BFS 导航（v0.4.29，替代 CHAR_SearchPath）** + `CHAR_Move` 逐帧 + **`MAP_SetFocus` 摄像机同步 + 每步 `GAMEPLAY_GoMapLinkByChar` 切图检测** | v0.4.24-25 / v0.4.29 BFS | ✅ 真机（含切图，地图间出口触发）⚠️ 旧记录 3080↔2056 为 v0.4.28 前旧 mapId 体系（3080=text_id"贝恩的士兵"/2056=瓦片矩阵误读），现为 MAPINFOBASE 下标 |
| `/api/world/movement/move/cancel` | `CHAR_RemovePath`(0xdb064) | v0.4.1 | ✅ 真机 |
| `/api/world/movement/walk` | `CHAR_Move`(0xe9808) flag=**0**（自动 `MAP_SetFocus` 跟随）+ **每帧** `GAMEPLAY_GoMapLinkByChar` 切图检测 | v0.4.24-25 | ✅ 真机（摄像机跟随+切图） |
| `/api/world/movement/walk/stop` | `CHAR_RemovePath` | v0.4.1 | ✅ 真机 |

## 1.5 帧驱动移动（v0.4.27 开发中，inline hook GAMESTATE_Draw）

> ⚠️ **状态**：hook 安装成功、trampoline 与重放区内容经验证正确，但进 world 后仍 SIGBUS/SIGILL 崩溃（fault addr 0x19/0x48）。本节记录已知结论、已修复 bug 与待探索方向。

### 1.5.1 动机

**问题**：v0.4.25 前 move/walk 为同步循环（move 循环 512 步 MoveAsPath、walk 循环 60 帧 CHAR_Move），单次 API 调用瞬时走完全程→画面"闪现"无逐帧动画。

**方案演进（终态）**：

| 方案 | 描述 | 结果 |
|---|---|---|
| **A. 后台线程 59ms** | **✅ 最终采用**：game_data.cpp 通用帧任务管理器（FrameTaskManager），单后台线程 sleep(59ms) 每帧遍历任务回调 | ✅ 已实现（v0.4.26）+ 真机验证逐帧移动 |
| B. inline hook 主循环 | hook 游戏每帧入口（STATE_ProcessGame/GAMESTATE_Draw） | ❌ 已弃用（手写 arm64 inline hook 崩溃：adrp 重定位/trampoline lr 污染/AGP .S 符号冲突，调试 2 天未果） |
| C. ShadowHook 库 | bytedance android-inline-hook | ❌ LSPosed 环境下 dispatch 失败（桥跳野地址） |
| D. 填 PATHLIST 让游戏自驱动 | SearchPath + 设动作=行走，让 CHAR_Process 每帧自动 MoveAsPath | ❌ 玩家控制态下动作被重置，驱动条件复杂（0x2fa/0xc40/0x2e0 耦合） |

**通用帧任务管理器（最终架构）** → 详见 **architecture.md §2.1（FrameTaskManager）**——move/walk 已改为注册回调（move_task_tick/walk_task_tick），非专用线程。

### 1.5.2 游戏主循环结构（反汇编确认）

```
MainProcess@0xd4984（全局每帧入口，UI+STATE+NOTIFIER+SOUND 调度）
  → blr [0x2f4000+0xa90] 指针 → STATE_ProcessGame@0x151540
      → bl GAMESTATE_PressKey@0x151310（按键处理）
      → bl GAMESTATE_Process@0x151264（状态 Process，blr [0x938]）
      → b GAMESTATE_Draw@0x1512b8（tail-call，唯一调用路径）
```

实证（frida）：游戏中 STATE_ProcessGame=GAMESTATE_Draw=GAMESTATE_DrawPlay 完全 1:1:1（每帧调用次数相等）。

### 1.5.3 实现要点（v0.4.26 FrameTaskManager）

**move/walk 任务**（game_data.cpp 匿名 namespace，经 FrameTaskManager 驱动）：
- `move_task_tick(void* ch)`：每帧 MoveAsPath 一步（清零 C_CTRL_STATE 让 AI 路径可走）+ map_link_check 切图检测，返回 false 终止
- `walk_task_tick(void* ctx)`：WalkCtx{ch,dir,remaining} 上下文，每帧 CHAR_Move(flag=0) 一步累计 60 帧 + map_link_check；**返回值语义：CHAR_Move 返回 0=正常走一步/非 0=撞墙**（反汇编 e98dc mov w20,#0x1，v0.4.26 修复）
- walk_stop（POST /api/world/movement/stop）：`stop_all_tasks()` + CHAR_RemovePath

**hook 探索记录（弃用）**：指令重定位器曾处理 adrp/adr/ldr literal 的 PC 相对重定位（adrp→movz/movk 序列），最终因 trampoline 返回链路 lr 污染等崩溃弃用。技术教训见 architecture.md §2.2。
- 非 PC 相对：原样复制

### 1.5.4 已修复 bug 清单

| # | 问题 | 根因 | 修复 |
|---|---|---|---|
| ① | adrp 未被识别（原样复制，新地址 PC 相对错） | 掩码 0xFC000000 判 0xB0000000（adrp bit31=1 变体）失败 | 改用 `(insn & 0x9F000000) == 0x90000000`（掩掉 immlo bit30-29） |
| ② | 重定位后目标偏移算错（示例：0x2F6000→0x200000） | imm21 重组公式错误：`(immlo<<19)|immhi`——AArch64 手册：immhi=bits[23:5]=imm21>>2, immlo=bits[30:29]=imm21&3 → **imm21=(immhi<<2)|immlo** | 修正 adrp/adr 两个分支 |
| ③ | trampoline stp/ldp 编码错误（0xa900 vs 0xa9b0） | `stp x0,x1,[sp,#-0x100]!` 编码 = 0xa9b007e0（bit16=1 标示负偏移 0x100），**非 0xa90007e0** | llvm 汇编验证后修正 |
| ④ | trampoline SIGILL at +0x48（执行到数据槽） | `blr x17` 设 lr=数据槽前地址，thunk ret 后执行数据（0x48 .quad thunk 地址被当指令解码→非法） | 数据槽移 trampoline 末尾（thunk 0x98、replay 0xa0），ldr 用远偏移 literal load（ldr x17,[pc,#0x54] / ldr x16,[pc,#0x10]），nop 对齐 .quad 到 8B |
| ⑤ | C++ 跨 TU 调用 .S 导出函数 parse 到 base.apk | AGP/LSPosed 下 .S 符号链接异常 | 放弃 .S，改纯 C++ 动态 mmap 生成 trampoline（install 时写入全部指令编码） |

### 1.5.5 当前问题与可能原因

**当前崩溃**：hook installed 后无初始崩溃，进 world 后 SIGBUS/SIGILL（pc 野地址 0x19/0x48）。

frida dump 确认：
- trampoline：保存段 16 条 → ldr x17（0x580002f1 → blr → 恢复段 17 条 ldp → ldr x16（0x580000d0）→ br → 0x98 thunk=正确 → 0xa0 replay=正确
- 重放区：movz/movk 加载 g_base+0x2F6000 正确 → stp x29,x30 → mov x29,sp → ldr x0,[x0,#0xaa0] → ldr+br resume（Draw+16）正确
- Draw 入口改写：ldr x17,[pc,#8]; br x17; .quad tramp 正确

**可能原因方向**：

| 方向 | 描述 | 验证方法 |
|---|---|---|
| A. Draw 内部后续指令依赖被破坏的寄存器 | 重放区只重放前 4 条，Draw+16 后原指令继续执行可能依赖 x16/x17 等临时寄存器（trampoline/blr 污染） | Stalker trace 从 Draw+16 起约 30 条看路径 + 寄存器 |
| B. trampoline 恢复段 sp 偏移 | stp 保存至 sp-0x100 但恢复后 add sp,#0x100——复原正确，但 blr 期间若 thunk/游戏函数改 sp，ldp 从错位恢复寄存器 → 数据乱 | hook sp 在关键点打印 |
| C. frame_tick_thunk 内部崩溃 | thunk 调 frame_tick → move_task_fn → fn_move_as_path / fn_char_move 可能因 hook 上下文（lr=thunk）崩溃 | 单独 hook frame_tick_thunk 入口 |
| D. Draw 被多次调用重入 | GAMESTATE_Draw 在 STATE_ProcessGame 内唯一 tail-call，但 GAMESTATE_Draw 内部分支（blr [0x930]）可能再调 Draw？→ 重入 | hook 计数 + 栈深 |
| E. ldr literal 的 pc 相对计算在 mmap 匿名区异常 | arm64 literal load 对 mmap RWX 区不可用（架构限制） | 改用 adr+ldr 替代 ldr literal |
| F. 重放区 stp 存错误 lr | trap 保存原始 lr → blr thunk 设 lr=恢复段 → 恢复段复活原始 lr → 重放区 stp 存恢复后的原始 lr → **Draw ret 应正常**。但 Draw 内部若 `blr [0x930]` 再次调 Draw → lr 改为 Draw 内 → 递归层 stp 存错 | 需验证 Draw 是否重入 |

### 1.5.6 替代实现方向

若 inline hook 持续失败，以下为备选：

1. **hook STATE_ProcessGame 入口**（替代 tail-call 处的 Draw）：hook 时机更早但逻辑一样（需处理 PressKey 前/后的差异）
2. **hook CHAR_Process**（每角色每帧驱动）：f1c04 入口，前 3 条非 PC 相对，需去重（同帧只 tick 一次）
3. **后台线程 59ms**（已实现）：回归方案 A，次优但可行
4. **Xposed Java 层 hook 渲染线程**：GLSurfaceView loop 处插入每帧回调——纯 Java，无 native hook 复杂度
5. **ShadowHook 2.x 更新**：等待 ShadowHook 修复 LSPosed 兼容问题

### 1.5.7 关键技术教训

- arm64 `adrp` 位域：immlo=bit30:29, immhi=bit23:5, imm21=(immhi<<2)|immlo（**非 immlo<<19|immhi**），21 位有符号（bit20=符号位）
- arm64 `adrp` 掩码：第 31 位=1 表示"大地址"变体（0xB），需用 0x9F000000 掩码
- AGP 的 .S 编译：符号引用受限制，C++ 跨 TU 调用 .S 导出函数在 LSPosed 下解析到 base.apk
- AGP 的 ASM flags：`set(CMAKE_ASM_FLAGS "-fPIC")` 被覆盖无效
- arm64 `stp x0,x1,[sp,#-0x100]!` = 0xa9b007e0（bit16=1 标志大偏移），非 0xa90007e0
- arm64 literal load：ldr Xt,[pc,#imm] 的 imm 以 4 字节为单位，ldr Xt,#imm=0x58000000|Rt|(imm19<<5)
- arm64 `blr` 设 lr=下一条地址，必须确保下一条非数据（否则 thunk ret 后执行数据→SIGILL）
- arm64临时寄存器：x16/x17 是 IP0/IP1（调用者不保证保存），trampoline 的 ldr+br 后不应假设它们存活

## 1.6 P0 待办事项

- [ ] **修复 inline hook 崩溃**（进 world 后 SIGBUS/SIGILL）：优先排查方向 A（Draw 后续指令寄存器依赖）、方向 E（ldr literal 在 mmap RWX 区可靠性）、方向 C（frame_tick_thunk 内部崩溃）
- [ ] 验证 walk 逐帧移动（hook 修复后）
- [ ] 验证 move 逐帧移动
- [ ] 验证 walk_stop/move_cancel 停止
- [ ] 验证切图（每帧移动后 GoMapLinkByChar 检测）
- [ ] 验证摄像机跟随（CHAR_Move flag=0 自动 MAP_SetFocus / move 显式 fn_map_set_focus）
- [ ] 文档：api-reference.md 补充 v0.4.27 端点说明

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

**模块实现（v0.4.25 每步检测）**：move 循环内**每步** MoveAsPath 后、walk 循环内**每帧** CHAR_Move 后调 `fn_go_map_link_by_char(ch, px>>4, py>>4)`（像素>>4=tile）触发出口检测，命中（返回 1）立即 break 提前终止；move 末尾再兜底检测一次 + 显式 `fn_map_set_focus(px,py)`。真机验证：move 到出口 tile → 3080（v0.4.28 前旧 ID，现 MAPINFOBASE 下标）→2056（v0.4.28 前旧 ID，现 MAPINFOBASE 下标）切图成功；walk 出口处也切图 2056（v0.4.28 前旧 ID，现 MAPINFOBASE 下标）→3080（v0.4.28 前旧 ID，现 MAPINFOBASE 下标）。切图由 GoMapLink 内部 `GAMESTATE_SetState(3)` 驱动状态机（渐隐→加载新图→恢复），期间游戏自动暂停。

**出口 tile 探测法**（frida）：扫描瓦片网格 `[0x2f3000+0xf48]` 解引用，`(byte & 0x80)!=0` 即出口（map 3080（v0.4.28 前旧 ID）出口在 tile (24,19)-(24,22)）。

## 2. 移动机制（✅ 完整逆向）

**玩家真实移动机制** = 方向键长按 → 游戏主循环每帧 `CHAR_Move`。API move 实现 = ⚠️ v0.4.29 前：CHAR_SearchPath 计算路径 + 临时清零 +0x2e2 控制态 + 循环调用 CHAR_MoveAsPath（同步瞬移）；**v0.4.29 起改用自研 BFS 导航**（瓦片矩阵 bit3 + 单位占用）→ FrameTaskManager 逐帧驱动 CHAR_Move（仍走游戏合法寻路链路，非 OP 传送）。

```
CHAR_SearchPath(ch, tx, ty, flag) @0xdb094：寻路计算 → 结果存 [ch+0x2F0] PATHLIST 链表（⚠️ v0.4.29 起 API move 已改用自研 BFS，此函数不再被 move 端点使用）
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
| CHAR_SearchPath | 0xdb094 | int(void*, int32_t, int32_t, int32_t) |
| CHAR_MoveAsPath | 0xe9db8 | int(void*) |
| CHAR_Move | 0xe9808 | int(void*, int32_t, int32_t, uint8_t) |
| CHAR_RemovePath | 0xdb064 | void(void*) |
| MAP_SetFocus | 0x11336c | void(int32_t, int32_t)（像素坐标） |
| GAMEPLAY_GoMapLinkByChar | 0x9cdc0 | int(void*, int32_t, int32_t)（tile 坐标） |
| MAPCHANGE_Set | 0x9c740 | void(int32_t, int32_t, int32_t, int32_t) |
| GAMESTATE_ProcessMapChange | 0x9c7ec | void() |
| MAP_FindMapLink | 0x112aac | int(void*, int32_t, int32_t) |
