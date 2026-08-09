# 游戏操作能力分析（写入/控制可行性）

> 日期：2026-08-05 ｜ 结论：**可以操作**。libgame.so 未 strip，游戏内部逻辑函数全部导出，可通过 dlsym 调用实现控制；引擎提供主线程事件队列保证调用安全。
> 更新 2026-08-05（无实机开发阶段）：**全部写操作函数签名已通过 objdump 逆向确认**，见 §5 签名表。

## 0. 调用机制修正（2026-08-05 逆向确认）

- **PushMainThreadEvent(0xd4d58)**：`void (void* fn, void* data)`。投递后游戏主循环 `MainProcess`(0xd4984) 消费：
  `blr x1` 调用 `fn(0x3074c8)`（x0 = AllocCBData 存储的全局 data 地址）。**单槽设计**：`[*(0x2f3cd8)]` 存 fn，
  `[0x3074c8]` 存 data，调用后清空。游戏自身（hubCallback*）也用此通道投递**无参回调**。
- **结论**：写操作**直接调用**（游戏函数内部连续执行无阻塞点，读操作已真机验证跨线程安全）；
  复合操作（equip/move/remove）一次调用内完成，无 yield 不会与逻辑线程交错。**调用前检查 `STATE_nState==5`（游戏中）**。
- **关于 `STATE_nState==5` 检查（2026-08-08 用户指出，修正表述）**：这是**为减少开发难度与测试广度的简化假设**，非逐操作实证的硬性要求——
  统一要求调用时游戏处于 world 状态（与正常玩法状态一致），避免在主菜单/存档界面调用游戏函数时因数据结构未就绪导致崩溃。
  **未经逐操作实证**：未对每个操作在多种界面状态下实测"是否真的会崩"；若后续需要支持非 world 状态操作，可**去除该检查并在多种情况下实测验证**。
- 存档：`SAVE_Save(0x129600)` 依赖存档上下文参数（内部取 `[x0+0x8c0]`），签名未完全确认，暂不直接调用。

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

> ⚠️ 本节原为 M4 初期的 API 设计草案，路径已过时，表格已删除。
> v0.3.1 起实施四层结构：合法操作 = `POST /api/action/*`，OP 操作 = 未来 `POST /api/op/*`。
> **现行端点以 `docs/api-reference.md` 为准**，分级依据见 `docs/api-technical-spec.md`。

## 5. 写操作函数签名表（2026-08-05 objdump 逆向确认，arm64-v8a libgame.so）

> 方法：NDK llvm-objdump 反汇编 + 参数寄存器使用分析（x0-x7 / w0-w7）。VMA 对应符号表。

| 函数 | VMA | 签名 | 说明 |
|---|---|---|---|
| `PushMainThreadEvent` | 0xd4d58 | `void(void* fn, void* data)` | 主线程事件投递（单槽，见 §0） |
| `INVEN_GetMoney` | 0x10445c | `int64_t ()` | 读金币 |
| `INVEN_SetMoney` | 0x10449c | `void (int64_t money)` | 设金币（内部 SV_GoldSet） |
| `INVEN_AddMoney` | 0x1044e4 | `int (int64_t delta)` | 加金币，返回 1/0（溢出则 0） |
| `INVEN_MinusMoney` | 0x104780 | `int (int64_t delta)` | 减金币，返回 1/0（不足则 0） |
| `INVEN_RemoveItem` | 0x104044 | `int (int32_t category)` | 按类别删第一个物品（→RemoveItemDirect） |
| `INVEN_ConsumeItem` | 0x1047bc | `void (void* item)` | 消耗 1 个（数量>1 减 1，否则删除） |
| `INVEN_MoveItem` | 0x104934 | `int (void* item, int32_t, int32_t, int32_t)` | 4 参（item+3），复杂，v0.3 暂缓 |
| `CHAR_SetExperience` | 0xd9b5c | `void (void* ch, int32_t exp)` | 直接写 +0x318 |
| `CHAR_AddExperience` | 0xe7028 | `int (void* ch, int32_t exp, uint8_t flag)` | 加经验（走升级判定链） |
| `CHAR_SetStatusPoint` | 0xd9c4c | `void (void* ch, int32_t points)` | 写 +0x32a（u16） |
| `CHAR_SetAutoAttack` | 0xe4cf4 | `void (void* ch, int32_t onoff)` | 写 +0x3a0 bit7..10 |
| `CHAR_EquipItem` | 0xe51c0 | `int (void* ch, void* item)` | **自动找槽**（CHAR_FindEquipSlot） |
| `CHAR_UnequipItemToInven` | 0xe2f68 | `int (void* ch, int32_t slot)` | 脱下装备槽→背包 |
| `CHAR_CanEquipItem` | 0xe4eb4 | `int (void* ch, void* item)` | 检查可否装备（职业掩码） |
| `CHAR_LearnAction` | 0xe2390 | `void* (void* ch, int32_t actionId, int32_t level)` | 学习/升级技能 |
| `PARTY_SetActivePlayer` | 0x11f584 | `int (int32_t slot)` | 切换主控（→PLAYER_SetActivePlayer） |
| `PARTY_Swap` | 0x11ff5c | `void (int32_t a, int32_t b)` | 交换队伍槽 |
| `CharSetPosition` | 0x12aa14 | `void (int32_t x, int32_t y)` | **全队传送**（对每佣兵槽写 +0x2/+0x4） |
| `MAPSYSTEM_ChangeMap` | 0x114fc4 | `void (int32_t mapId, int32_t x, int32_t y, int32_t dir)` | 切图（内部 MAP_Load(mapId,1) + FindBestLoc） |

### 已确认不可直接调用
| 函数 | VMA | 原因 |
|---|---|---|
| `SAVE_SaveData` | 0x1290c0 | 签名 `(w0, x1, x2)→SAVE_SaveDataAsKey`，上下文复杂 |
| `SAVE_ProcessSave` | 0x129830 | UI 流程（弹窗+KEY 状态），依赖游戏状态机 |
| `SAVE_Save` | 0x129600 | 依赖存档上下文参数（`[x0+0x8c0]`），需进一步逆向 |
| `POPUPSTATE_Pop` | 0x122600 | **关闭面板直接调用崩溃**（v0.4.5 真机实测）：pop 后触发新栈顶 resume 回调 → `STATE_ResumeGame → GAMESTATE_DrawPlay → MAP_DrawLayer` SIGSEGV（settings 面板关闭场景；character_info 关闭偶发成功）。popup 栈状态机对顺序敏感，需走 UI 触摸路径（Back 键事件），**panel/close 端点已撤销** |
| `CHAR_SetActionID` | 0xe79ec | **释放技能动作直接调用崩溃**（v0.4.5 真机实测）：`CHAR_SetActionID(ch, actionId, level)` 设置技能动作后，渲染帧 `GAMEPLAY_DrawFocus+368` 读目标结构 +0x4 空指针 SIGSEGV（fault addr 0x4，GLThread）。技能动作的"焦点/目标指示器"绘制需要**合法敌人目标**——传交互物/非敌人目标即崩溃；且需验证无目标场景。**cast 端点已撤销**，待探索敌人判定（区分敌人 vs 交互物）+ 合法目标后重做 |

### 5.1 合法操作函数签名（v0.3.1 初版，v0.3.2-0.3.6 真机验证修正）

| 函数 | VMA | 签名 | 说明 |
|---|---|---|---|
| `CHAR_MoveAsPath` | 0xe9db8 | `int (void* ch)` | **沿已存路径移动**（读角色 +0x2f0 PATHLIST，配合 CHAR_SearchPath 计算后调用 = 合法移动）。⚠️ 玩家控制态（+0x2e2≠0）下 MoveAsPath 需目标指针 +0x278 非空，且只走一步不续走——API 层循环调用（v0.3.2） |
| `ITEMDATABASE_IsUse` | 0x1058ac | `int (int32_t itemId)` | **物品可否使用**（读 ITEMDATABASE 记录 +2 字节 ∈ {0x16,0x17}=药水类）。API use-item 前置校验（v0.3.2），非消耗品拒绝 |
| `INVEN_ConsumeItem` | 0x1047bc | `void (void* item)` | 消耗 1 个（使用药水/卷轴） |
| `INVEN_RemoveItemDirect` | 0x103fd8 | `int (int32_t bag, int32_t slot)` | **按槽删物品**（x0=bag 左移 4 位，x1=slot → bag*16+slot 索引，ITEMPOOL_Free 释放）。⚠️ **返回值非成功标志**（成功路径 tail-call PLAYER_UpdateShortcut）——API 按调用后槽位清空判定（v0.3.2） |
| `MERCENARYSYSTEM_IncludeParty` | 0x118e04 | `int (void* ch)` | 佣兵入队（内部 PARTY_GetSize<3 校验 + PARTY_Include + 位置设置），返回 1/0。API 前置校验已在队/满员（v0.3.6） |
| `MERCENARYSYSTEM_ExcludeParty` | 0x118d0c | `int (void* ch)` | 佣兵离队（PARTY_Exclude + 状态设置）。⚠️ 主控/任务NPC 走 UIPopupMsg 弹窗路径返回 -1——API 前置校验（v0.3.5，CHAR_IsSpecialNPC 0xe4d90 识别任务NPC） |
| `CHAR_EquipItem` | 0xe51c0 | `int (void* ch, void* item)` | 穿装备。⚠️ **目标槽被占用时返回 0**——API 自动替换（先卸后穿，v0.3.3，配合 CHAR_FindEquipSlot 0xe4fd0 + CHAR_GetEquipItem 0xda20c） |
| `CHAR_SetTarget` | 0xdc754 | `void (void* ch, void* target)` | **设置攻击目标**（写 [ch+0x278]=target，目标变化时写全局状态 0x11）。API attack 前置（v0.4.2） |
| `CHAR_MakeDefaultAttack` | 0xe2730 | `int (void* ch)` | **置普攻动作**（写 [ch+0x2a8]，内部 CHAR_FindAction(0xdd3ac)+CHAR_LearnAction(0xe2390) 算 actionId=5，成功后 CHAR_UpdateActionInfo(0xe2138)）。API attack 后置（v0.4.2） |
| `CHAR_StopCombat` | 0xe7c24 | `void (void* ch)` | **停止战斗官方函数**（清 [ch+0x358] 战斗标志 → 清 [ch+0xc] bit2 → HATESYSTEM_RemoveWho(0x1024e4) → tail-call CHAR_SetActionID(ch,0,0)）。API stop（v0.4.2） |
| `INVEN_MoveItem` | 0x104934 | `int (void* item, int count, int targetBag, int targetSlot)` | **物品移动/堆叠合并**：源 item 指针 + count + 目标 bag/slot。空目标→ITEMSYSTEM_CopyAsNewUID 复制+INVEN_SaveItemDirect 存入；同类→堆叠合并（上限 99）；源数量减 count。返回 w0=1 成功/0 失败（v0.4.4） |
| `ITEM_GetPrice` | 0x109f50 | `int (void* item)` | **读静态表价格**（读 item+8 字段 + ITEM_GetAbilityLevel 计算）。API sell 价格来源（v0.4.3，防任意定价刷钱） |

### 5.2 依赖 UI 状态不可直接调用（合法但需 UI 流程）
| 函数 | VMA | 依赖 |
|---|---|---|
| `UIStore_BuyItem(price)` | 0xd242c | ControlObject_GetCursor 取 UI 选中商品（商店界面打开状态） |
| `UIStore_SellItem(price)` | 0xd25f0 | 同上（UI 选中物品） |
| `UISkill_SkillMainExe` / `UIPlay_ButtonSKill` | — | 技能 UI/快捷键状态（释放技能） |
| `CHAR_ProcessSkillBook(ch,item)` | 0xe2488 | 技能书物品（学习技能，非释放） |
| `QUESTSYSTEM_AcceptReivew` | 0x125c70 | **硬编码剧情任务 quest 489**，非通用接任务 |
| `QUESTSYSTEM_RefuseReview` | 0x125cd0 | 同上（quest 489 拒绝） |
| `UIMix_ButtonMixingExe` | 0xc21ec | 合成 UI 状态（材料槽选中） |
| `MIXSYSTEM_CheckMixture(type,item)` | 0x11ac34 | 仅检查（0/1/0x10 分支），非执行 |
| `ITEMSYSTEM_EnchantItem`/`PutJewel` | — | 强化/镶嵌需物品+材料上下文 |
