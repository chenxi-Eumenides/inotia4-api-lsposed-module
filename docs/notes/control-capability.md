# 游戏操作能力分析（写入/控制可行性）

> 日期：2026-08-05 ｜ 结论：**可以操作**。libgame.so 未 strip，游戏内部逻辑函数全部导出，可通过 dlsym 调用实现控制；引擎提供主线程事件队列保证调用安全。

## 1. 结论

| 操作 | 可行性 | 关键函数 |
|---|---|---|
| 金币增删 | ✅ 高 | `INVEN_AddMoney` / `INVEN_MinusMoney` / `INVEN_SetMoney` |
| 背包管理 | ✅ 高 | `INVEN_MoveItem` / `INVEN_RemoveItem` / `INVEN_ConsumeItem` / `INVEN_RemoveItemDirect` |
| 等级/经验 | ✅ 高 | `CHAR_AddExperience` / `CHAR_SetLevel` / `CHAR_SetExperience` |
| 属性点分配 | ✅ 高 | `CHAR_SetStatusPoint` / `CHAR_SetStatMain` / `CHAR_SetStatBase` / `CHAR_SetStatSub` |
| 装备穿脱 | ✅ 高 | `CHAR_EquipItem` / `CHAR_EquipItemFromInvenToSlot` / `CHAR_UnequipItemToInven` / `CHAR_CanEquipItem` / `CHAR_ResetEquipItem` |
| 角色切换 | ✅ 高 | `PARTY_SetActivePlayer` / `PARTY_SetNextActivePlayer` / `PARTY_Swap` |
| 队伍调整 | ✅ 高 | `PARTY_AddMember` / `PARTY_Exclude` / `MERCENARYSYSTEM_AddCharacter` / `IncludeParty` |
| 位置/传送 | ✅ 高 | `CharSetPosition`（0x12aa14）/ `PARTY_Follow` / `PARTY_MoveBack` |
| 自动攻击开关 | ✅ 高 | `CHAR_SetAutoAttack` |
| 主动使用技能 | ⚠️ 中 | `UIMercenary_SkillExe`、`CHAR_SuccessOnPhysicalAttack` / `CHAR_SuccessOnMagicalAttack`（攻击处理链），需理解技能触发流程 |
| 学习技能 | ⚠️ 中 | `UISkill_ButtonLearnOK` / `UISkill_ButtonLearnExe`（走 UI 流程，依赖游戏状态） |
| 物品生成/强化 | ⚠️ 中 | `ITEMSYSTEM_GenerateGrade` / `ITEMSYSTEM_ApplyGrade`（品级生成，签名需逆向） |

## 2. 调用机制（关键设计）

### 2.1 函数调用（模块内实际用 base+VMA，非 dlsym）

> ⚠️ 下文 dlsym 为 M2 分析期写法；**模块实际实现 = `/proc/self/maps` 基址 + 符号 VMA 直算地址**
> （dlopen/dlsym 在 LSPosed namespace 隔离下会加载独立副本，实测读不到游戏数据；见 architecture.md）。

```cpp
uintptr_t base = /* /proc/self/maps 定位 libgame.so */;
auto addMoney = (void(*)(int64_t))(base + 0x10445c /* INVEN_AddMoney VMA */);
addMoney(1000);
```

**主要工作量 = 函数签名逆向**（参数类型/顺序/返回值）。方法：
- 反汇编目标函数（objdump/Ghidra）推断调用约定
- frida 运行时 `NativeFunction` 探测 + 与游戏自身行为对照验证

### 2.2 线程安全（PushMainThreadEvent）

游戏逻辑运行在特定线程（渲染/逻辑线程），外部线程直接调用有竞态风险。引擎提供主线程事件队列：

- `PushMainThreadEvent`（0xd4d58）：投递函数到主线程执行 ✅ 已验证存在
- 模块所有写操作应投递到主线程执行，保证与游戏逻辑同步

### 2.3 UI 状态机（STATE）

- `STATE_nState`（0x307492）/ `STATE_fpEnter` / `STATE_fpProcess`：当前 UI 状态（Title/MainMenu/Game/...）
- 部分操作（学习技能、进商店）依赖游戏处于特定状态，调用前需检查/切换状态

### 2.4 输入模拟（备选路径）

- `TouchHandle_Event` / `TouchHandle_ControlEventProc`：可直接模拟触摸事件（等价于人工点击），可作为"走 UI 流程"操作的底层手段

## 3. 风险与约束

| 风险 | 说明 | 对策 |
|---|---|---|
| 函数签名错误 | 参数不匹配 → 崩溃/脏数据 | 签名逆向 + frida 验证 + 模块内 try 隔离（native 无法 try，用独立线程 + crash 保护） |
| 线程竞态 | 非主线程调用游戏逻辑 | 一律经 `PushMainThreadEvent` |
| UI 不刷新 | 直接改数据后游戏画面不更新 | 对 API 消费者无影响；如需同步可触发对应 UI 刷新函数 |
| 状态机冲突 | 错误状态下调用操作函数 | 调用前检查 `STATE_nState`，必要时先切状态 |
| 非法状态数据 | 绕过校验（如等级超限） | 尽量走正规 API（有校验的如 `CHAR_CanEquipItem`），少用 Test 函数 |
| 存档持久化 | 修改不自动保存 | 修改后触发 SAVE 系统（`SAVE_*` 函数） |
| 版本耦合 | 符号地址/结构随版本变化 | 按 dlsym 符号名（非地址）解析，仅依赖符号名稳定性 |

## 4. API 设计方向（POST 操作端点）

| 方法 | 路径 | 操作 |
|---|---|---|
| POST | `/api/player/money` | 增/减金币 |
| POST | `/api/player/{index}/experience` | 增减经验/升级 |
| POST | `/api/player/{index}/equip` | 穿/脱装备（body: itemId + slot） |
| POST | `/api/player/{index}/skill` | 使用/学习技能 |
| POST | `/api/player/switch` | 切换主控角色 |
| POST | `/api/inventory/move` | 背包内移动/整理 |
| POST | `/api/inventory/remove` | 删除物品 |
| POST | `/api/teleport` | 传送（地图 + 坐标） |
| POST | `/api/save` | 触发存档 |

> 操作类端点统一返回最新状态（操作后重读），幂等性由游戏 API 自身保证。
