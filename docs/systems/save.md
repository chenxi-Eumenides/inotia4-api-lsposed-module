# 存档系统逆向笔记（Save）

> 目录：docs/systems/ ｜ 主题：存档/读档链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/action/save/save` | `SAVE_Save`(0x129600) 无参静默保存 | v0.4.16 | ✅ 真机 |
| `POST /api/action/save/load` | ⛔ 卡点（仅主菜单/选档界面，GAMELOADER 状态限制，P3 暂缓） | 占位 | not implemented |

## 2. SAVE_Save 完整签名（✅ v0.4.16 逆向修正）

**`SAVE_Save` 是无参函数 `int(void)`**——之前的"[x0+0x8c0] 上下文参数"是**误判**（0x2f3000+0x8c0 是全局存档上下文指针，非参数）。

```
SAVE_Save():
  SV_GoldGet() + SV_TStatPointGet(0x16c960) + SV_TSkillPointGet(0x16caf8) 校验（任一不过 → CS_knlExit 返回 0）
  KEY_ResetActive(0x10f354)
  写 [0x2f3000+0x878] 保存槽标志位（UTIL_SetBitValue）
  APPINFO_Save(0xd8084)（保存设置）
  SAVE_SaveInformation(0x1270ec) → SAVE_SetBlockInfo
  SAVE_SavePlayer(0x128e38) → SAVE_SetBlockInfo
  SAVE_SaveCharacterAll(0x129480) → SAVE_SetBlockInfo
  SAVE_SaveInventory(0x127d8c) → SAVE_SetBlockInfo
  SAVE_SaveQuest(0x128000) → SAVE_SetBlockInfo
  SAVE_SaveEvent(0x1282d0) → SAVE_SetBlockInfo
  SAVE_SaveETC(0x1286f8) → ...
  SAVE_SaveData(0x1290c0)（最终写盘）
```

## 3. 保存链（UI 流程 vs 直接调用）

```
UI 流程（P0-1 动态验证）：SystemMenu_ButtonSaveExe(0x14f7c4) → SAVE_ProcessSave(0x129830)
  → SAVE_IsOK(0x128c14) → KEY_ResetActive → SAVE_Save → 弹 UIPopupMsg_CreateOK("保存成功")

API 直接调用（v0.4.16）：SAVE_Save() 无参——静默保存无弹窗，内部校验通过即全量序列化
```

## 4. 存档数据结构

- `SAVE_pSaveSlot` @0x729858：存档槽结构（87 字节，含角色/物品数据，**离线兜底数据源**）
- 存档上下文：`[0x2f3000+0x8c0]`（全局指针，SAVE_Save 内部读取）
- 存档槽 UI：SC_SAVESLOT @0x14c720，3 槽位面板

## 5. 真机验证（v0.4.16）

- **hook 确认**：SAVE_Save → SAVE_SaveInformation → SAVE_SaveCharacterAll → SAVE_SaveInventory → SAVE_SaveData 全链命中（真实序列化）
- **存档生效验证**：丢弃再生药水（消耗背包物品）→ save/save → 回主菜单重进 → **再生药水保持丢弃**（存档写入生效）、金币 81 不变
- 用户规则（m1488）：save 测试可消耗资源并保存，但**消耗背包物品**（可再获得）而非金币/能力点

## 6. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| SAVE_Save | 0x129600 | int(void)（无参，完整保存） |
| SAVE_ProcessSave | 0x129830 | —（UI 流程，弹窗） |
| SAVE_SaveData | 0x1290c0 | —（写盘） |
| SAVE_SaveDataAsKey | 0x129050 | — |
| SAVE_SaveInformation | 0x1270ec | — |
| SAVE_SaveCharacterAll | 0x129480 | — |
| SAVE_SaveInventory | 0x127d8c | — |
| SAVE_SaveQuest | 0x128000 | — |
| SAVE_SaveEvent | 0x1282d0 | — |
| SAVE_SaveETC | 0x1286f8 | — |
| UIPlay_CallSave | 0xc604c | —（36B UI 入口） |
| SAVE_IsOK | 0x128c14 | — |
| APPINFO_Save | 0xd8084 | —（保存设置） |
| SAVE_GetSaveSlot | 0x1289e4 | void*(int32_t)（槽结构，[0x2f5000+0xe40]+slot×0x1d） |
| SAVE_LoadSaveSlot | 0x1298dc | int(int32_t, void*)（读档，依赖 popup 流程上下文） |
| SAVE_LoadData | 0x129260 | int(int32_t, void*, void*)（读档数据核心） |
| UI_SetPopupProcessInfo | 0xaecc8 | int(int32_t, int32_t)（注册 popup 流程，4=读档） |
| GAME_StartResumeGame | 0x1002e8 | int(int32_t slot)（启动游戏读档） |
| GAMESTATE_SetState | 0x151590 | void(int32_t)（状态机切换，4=主菜单） |
| UI_SetPopupProcessInfo | 0xaecc8 | int(int32_t id, int32_t data)（注册 popup 流程） |
| UI_PopupProcess | 0xaebfc | int(void)（主循环处理 popup 数组，跳转表 @0x24a190） |
| DailyReward_ButtonOKExe | 0x16f024 | void(void)（每日奖励确认键 = SetPopupProcessInfo(4,0)+(1,0x14)） |

## 8. 付费弹窗阻断（✅ v0.4.18）

> 支付/每日奖励系统完整逆向见 docs/systems/iap.md（触发链/UIPlay_CallInAppShopProc/支付结果恢复/关键状态变量）

### 弹窗根源（Java 层）
- **支付方式选择弹窗 = `SelectTarget.iapSelectTarget(Activity, SurfaceViewWrapper, SelectTargetCallback, long)`**（静态 void，弹窗入口）→ `showSelectTargetTypePopup` 创建 **`android.app.Dialog`**（selectTargetTypePopup 字段）
- **确认**：原生弹窗**不走游戏 popup 栈**——是 Android 原生 Dialog（WindowManager/UI 线程）
- 调用链：`InApp.iapSelectTarget(J)`（公开）→ `SelectTarget.iapSelectTarget`（静态）；`InApp.selectBillingTarget` → 同上
- 触发场景：daily_reward 面板确认键（确认后领取每日奖励 → Hive 支付流程弹窗）

### 阻断实现（HookMain.kt onPackageLoaded）
- hook `SelectTarget.iapSelectTarget`：`hook(method).setExceptionMode(PROTECTIVE).intercept(chain -> null)`（跳过原方法，弹窗不弹出）
- libxposed 101 API：onPackageLoaded + getDefaultClassLoader + XposedInterface.ExceptionMode
- **真机验证**：enter-slot 进存档1 → daily_reward 瞬时 → **无支付弹窗直接进 world**（金币72847/LV27忍者 存档1 数据一致）

### 用户三任务（m1627）完成
①enter-slot ✅ ②save/slots ✅ ③付费弹窗阻断 ✅（v0.4.18）

## 9. enter-slot 无 UI 问题（✅ 已修复 v0.4.19，2026-08-09）

### 现象（用户报告 + 截图确认）
- **enter-slot 直接进 world 后无 UI/HUD**：无按钮/摇杆/界面元素、不可控制、**亮度比正常暗非常多**
- 截图：画面是**无 UI 的昏暗世界场景**（角色/场景都在，无 HUD）

### 根因（v0.4.19 完整逆向确认）
- 进档链：enter-slot → GAME_StartResumeGame → 读档 → GAMESTATE_SetState(0) 进 world → **游戏自动弹 daily_reward（类型 0x1a）**
- daily_reward 弹出由 **UIPlay_CallInAppShopProc(0xc7b64)** 触发（UIPlayPorting_Draw 每帧检测 `[0x2f5000+0xff8]`==0 时调用），该函数会：
  1. **置 `[0x2f6000+0xc48]` 指向字节 = 0**（world HUD 绘制总开关，GAMESTATE_DrawPlay 0x9d6cc 开头检查，=0 直接 return 不画 HUD）
  2. 注册 daily_reward 面板（UI_SetPopupProcessInfo(1, 0x1a)）
  3. 调 InApp_SelectTarget → CS_IapSelectTarget → **Java SelectTarget.iapSelectTarget（Hive 支付弹窗）**
- 支付被模块 hook 阻断（跳过）后：**支付流程无回调 → 上述开关永不恢复为 1 → GAMESTATE_DrawPlay 永远跳过 HUD 绘制** → world 无 HUD 卡死
- popupCnt=1（daily_reward）期间主循环走 POPUPSTATE_Process 分支，GAMESTATE 绘制不运行，画面冻结

### 修复（v0.4.19）
- **hook 阻断 iapSelectTarget 后调用 native 恢复函数 `data_recover_after_hive_block()`**：
  1. `[0x2f5000+0xff8]` 指向 u32 = 1（每日奖励触发标志，阻止 UIPlayPorting_Draw 再次触发商店流程）
  2. `[0x2f6000+0xc48]` 指向字节 = 1（恢复 HUD 绘制开关）
  3. UI_SetPopupProcessInfo(4, 0)（关闭 daily_reward 面板）
- 效果：进档后 daily_reward 面板立即关闭、HUD 完整显示、无支付弹窗、游戏可正常操作
- 观察模式开关保留（游戏私有目录 `skip_hive_block.flag` 存在时跳过 hook，用于对比原始流程）

### 验证（真机）
- slot 1（存档2）与 slot 0（存档1）均验证：enter-slot → game=world、HUD 完整（小地图/摇杆/技能/角色信息）、无支付弹窗、可移动（坐标变化）、数据正确
- 日志确认：`blocked Hive SelectTarget.iapSelectTarget (payment dialog)` + `hive recovery: {"ok":true}`

### 10 次随机测试（存档1/存档2）
- 循环：main-menu → 随机 slot(0/1) → enter-slot → 验证 snapshot 数据（存档1=金币72847/LV27，存档2=金币81/LV2）
- **前 8 次数据全正确**，第 9 次用户中止（screen 停在 daily_reward）——进档本身稳定，UI 问题是根本缺陷（v0.4.19 已修复）

## 7. 读档/进存档（✅ v0.4.18 逆向 + 纯 API 验证）### 存档槽结构
- **SAVE_pSaveSlot @0x729858 = 槽区 [0x2f5000+0xe40]（同一地址）**，每槽 29B（0x1d，`SAVE_GetSaveSlot`(0x1289e4) 用 slot×0x1d 索引）
- 槽布局：b0=存在标志、b2=槽标志、+0x1c=角色类型；slot 0..2（存档文件 save0.dat=槽1/save1.dat=槽2）
- 运行时槽区 b0 可能为 0（未完整加载）但 b2=1（存档存在）——存在性判定用 b0||b2

### 官方进存档链（frida 捕获确认，触摸进存档）
```
SaveSlot_SlotButtonExe(0x14cd08)（存档槽面板选槽回调）
  → UI_SetPopupProcessInfo(4, 0)(0xaecc8)     # 注册 popup 流程4（读档）
  → [0x2f6000+0x8] = 0                         # 清读档标志
  → GAME_StartResumeGame(slot)(0x1002e8)       # slot=0 存档1 / 1 存档2
      → GAME_Initialize → [0x2f6000+0xd20]=slot → STATE_Set(5) → MAPCHANGE_Set → GAMESTATE_SetState(3)
      → 主循环 UI_PopupProcess(0xaebfc) 处理流程4 → SAVE_LoadData(slot)=1 → SAVE_LoadPlayer=1
        → SAVE_LoadCharacterAll=1 → GAMESTATE_SetState(0) 进 world
```

### 纯 API 进存档（v0.4.18 验证成功）
- **`UI_SetPopupProcessInfo(4,0)` + 清 `[0x2f6000+0x8]` + `GAME_StartResumeGame(slot)`** 在干净主菜单状态可进 world（真机验证存档2：金币81/LV2祭司）
- **前置检查**：仅非 world 状态（STATE==4 主菜单/存档选择）可调用；world 中调用会破坏状态机
- **崩溃教训**：手动调 GAME_StartResumeGame 在**状态不干净**时崩溃——`GAMESTATE_SetState(3)→GAMESTATE_EnterPlay+84→CHAR_GetSkillPoint(角色+0x328)` fault（存档数据未加载到角色对象，popup 流程上下文缺失）。干净主菜单（游戏重启后）才可调用

### 主菜单状态机（v0.4.17-4.18）
- `STATE_nState` GOT 0x2f5000+0xf8（world=5/main_menu=4，切换中=0xFFFF）
- `GAMESTATE_SetState`(0x151590) state==4 分支：GAME_Exit + STATE_Set(4) + Enter 回调（main-menu 端点用）
- popup 栈：`g_arrPopupStack` @0x728fd8（元素 0x40B +0x10 enter；面板区分：0x14c720 save_slot/0x14d670 character_select/0x16f050 daily_reward）

## 10. 创建新存档链（✅ v0.4.64 逆向 + frida 全流程实证 + API 实现）

### 官方链（frida 监听实证，2026-08-12 用户手动创建全程捕获）

```
点主菜单「新的开始」→ save_slot 面板（UI_SetPopupProcessInfo(1,0) + Scene_Init_POPUP_SC_SAVESLOT + SAVE_CreateSaveSlot）
点空白槽(slot) → SaveSlot_GoToNewGame(0x14cc5c, slot):
  SAVE_GetSaveFileName(slot, buf) → CS_fsRemove(buf, 1)      # 删除旧存档文件
  *[0x2f4000+0xd20] = slot                                    # 当前槽位
  *[0x2f6000+0x8] = 1                                         # 新建标志（0=读档 1=新建）
  GAME_ExitSaveSlotSelectCharacter(0x10013c)                  # GAME_Initialize + MAP_Load(6) + MAINMENU_CreateSelectCharList
  UI_SetPopupProcessInfo(1, 1)                                # character_select 面板
  （6 职业预览角色 CHARSYSTEM_Produce(2, idx) 生成）
选职业（写 [0x308080+0x8] = class_idx）→ 点「开始游戏」→ SelectCharacter_ButtonStartExe(0x14dee0):
  SelectCharacter_StartGame(0x14de98):
    *[0x2f5000+0xa00] = [0x308080+0x8]                        # 职业索引（STATE_EnterGame 读作 GAME_StartNewGame 参数）
    STATE_Set(5)                                              # 状态机 → STATE_EnterGame
    UI_SetPopupProcessInfo(4, 0)
    Flurry_EventCharacterClass
  TutorialStart(0x16ceb0)                                     # 新档教学初始化
STATE_EnterGame(0x1511a0) 检测 *[0x2f6000+0x8]==1 → 新建分支:
  GAME_ExitSelectCharacter(0x10015c) + MAINMENU_ReleaseSelectCharList
  GAME_StartNewGame(slot, class_idx, charName)(0x10017c):
    GAMEINFO_Create → *[0x2f4000+0xd20]=slot → CHARSYSTEM_Produce(0, class_idx)
    → 角色名写入 [0x2f3000+0xaf8] → PLAYER_SetMainPlayer/SetActivePlayer
    → 初始物品 ITEMSYSTEM_CreateItem(4)×2 + (5) → 快捷键/槽位标志 → EVTSYSTEM_SetReady
  GAMESTATE_SetState(1) → MAP_Load(0)（初始营地）→ 剧情 NPC 生成 → GAMESTATE_SetState(0) 进 world
```

### 关键状态变量（创建链）

| 变量 | 地址 | 语义 |
|---|---|---|
| G_CURRENT_SLOT_GOT | [0x2f4000+0xd20] 指针 | 当前存档槽 u8 |
| G_GAME_RESUME_FLAG_GOT | [0x2f6000+0x8] 指针 | 进档/新建标志（0=读档 1=新建） |
| G_PRODUCE_CLASS_GOT | [0x2f5000+0xa00] 指针 | 职业索引 u8（STATE_EnterGame→GAME_StartNewGame 参数） |
| G_SELECTED_CLASS | [0x308080+0x8] u32 | 选角 UI 选中职业（SelectCharacter_StartGame 读取源） |
| GAMESTATE_bNewGame | [0x3099a8] u8 | 新档标志 |

### 纯 API 创建（v0.4.64 实现，`POST /api/action/save/create`）
- **`data_op_create_slot(slot, class_idx)` 复刻官方链**：SAVE_CreateSaveSlot 槽区初始化 → 删目标槽旧档 → 写 slot/新建标志/选中职业 → GAME_ExitSaveSlotSelectCharacter → SelectCharacter_StartGame → TutorialStart → 状态机自动驱动到初始营地
- **前置**：非 world（主菜单）状态；slot 0-2；class_idx 0-5（0=战士 1=盗贼 2=弓手 3=法师 4=圣职者 5=...，CHARCLASSBASE 顺序，真机 frida 实测 class_idx=3 建号成功）
- **教学残留处理**：创建后教学状态 obj170 可能非 6，移动类操作前确认 tutorial_state 处理（与 enter-slot 一致）

### 相关符号表（v0.4.64 新增）

| 函数 | VMA | 签名 |
|---|---|---|
| STATE_Set | 0xd46a8 | void(int32_t) 写状态机 state |
| GAME_ExitSaveSlotSelectCharacter | 0x10013c | void() GAME_Initialize+MAP_Load(6)+MAINMENU_CreateSelectCharList |
| SelectCharacter_StartGame | 0x14de98 | void() 选角确认（写职业+STATE_Set(5)+UI_SetPopupProcessInfo(4,0)） |
| SelectCharacter_ButtonStartExe | 0x14dee0 | void() 开始游戏按钮（StartGame + TutorialStart） |
| TutorialStart | 0x16ceb0 | void() 新档教学初始化 |
| SAVE_GetSaveFileName | 0x125d08 | void(int32_t slot, char* out) |
| CS_fsRemove | 0x1b27bc | int(char* path, int32_t) |
| GAME_StartNewGame | 0x10017c | int(slot, classIdx, charName) 新档核心启动 |
