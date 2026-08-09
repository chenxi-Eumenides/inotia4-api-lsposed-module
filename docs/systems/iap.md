# 支付/每日奖励系统逆向笔记（Iap）

> 目录：docs/systems/ ｜ 主题：Hive 支付弹窗 + 每日奖励（daily_reward）+ in-app 商店触发链逆向结论（唯一归属）
> 关联：docs/systems/save.md §8/§9（付费弹窗阻断实现 + 进档无 UI 修复）、docs/systems/ui.md（popup 面板系统）、docs/reference/ui-click-coordinates.md（world HUD 坐标）

## 1. 系统概览

- **Hive 支付弹窗** = Android 原生 Dialog（`SelectTarget.iapSelectTarget` → `android.app.Dialog`），**不走游戏 popup 栈**（WindowManager/UI 线程）
- **每日奖励（daily_reward）** = 游戏 popup 面板（类型 0x1a），进档后自动弹出，是支付弹窗的触发前置
- **进档触发链**：进档 → world → UIPlayPorting_Draw 检测 → UIPlay_CallInAppShopProc → daily_reward + HUD 开关置 0 + 支付请求
- 联网已删 → 支付请求必然失败 → 走失败恢复链（resultPostTarget → HubLogin_CheckOK）

## 2. 触发链（UIPlayPorting_Draw 检测）

```
UIPlayPorting_Draw(0xc87a0)（每帧，world HUD 状态绘制）：
  c8d7c: [0x2f5000+0xff8] → 读 u32（每日奖励触发标志）
  c8d88: == 0 → b 0xc9900 → UIPlay_CallInAppShopProc(0xc7b64)
```

- **每日奖励触发标志** `[0x2f5000+0xff8]`（GOT → 指向 u32）：**0=未处理（每帧触发商店流程）**，非 0=已处理
- 标志被置 1 的路径：支付流程完成后（模块修复也直接置 1）
- ⚠️ 该 GOT 槽在 **UIDesc（技能描述）** 结构里也有同名偏移（0xb4000 区域）——是不同变量，勿混淆

## 3. UIPlay_CallInAppShopProc(0xc7b64) 完整逻辑

```
SOUNDSYSTEM_Play(0)
[0x2f5000+0x228] 指向字节 bit0 == 0 → 置 bit0=1 + APPINFO_Save（首次标记）
SAVE_IsOK() == 0 → UIPopupMsg_CreateOKFromTextData（弹「无法保存」）
CS_netGetActiveNetwork() == 1 → 网络商店分支（c7c24，联网才走）
NetworkStore_SetType(0)(0x15b0dc) + NetworkStore_Enter(0)(0x15b330)
[0x2f6000+0xc48] 指向字节 = 0        # ★ HUD 绘制总开关置 0（隐藏 HUD）
UI_SetPopupProcessInfo(1, 0x1a)     # 注册流程1 → 弹 daily_reward 面板（类型 0x1a）
[0x2f5000+0xff8] → u32 != 0 → C2S_HubBeginWithFlow(0x934ac)
InApp_SelectTarget(0x1608d8)        # 支付请求（触发 Java 弹窗）
```

- **触发时机**：进档后每日奖励可领时（标志==0 且主循环绘制 HUD 路径运行）
- 调用者：仅 `UIPlayPorting_Draw`（每帧检测）——即**只在 world 状态且 HUD 绘制开关开启时**才会触发

## 4. 支付链（native → Java）

```
InApp_SelectTarget(0x1608d8) → CS_IapSelectTarget(0x206fa8)
  CS_IapSelectTarget = C++ 对象虚函数分派（[0x70f000+0x848] 对象 vtable → blr）
  → JNI → Java SelectTarget.iapSelectTarget(Activity, SurfaceViewWrapper, SelectTargetCallback|null, long)
    - InApp.iapSelectTarget(J)：回调参数为 null（游戏不等待回调）
    - SelectTarget.iapSelectTarget：activity==null 或 selectThread 存活 → 直接 return；
      否则启动 SelectTarget$1 线程创建 android.app.Dialog
  Java 弹窗完成 → 回调 → native resultPostTarget
```

**支付结果处理（native）**：

```
resultPostTarget(0x160940)（w0 = 结果码）：
  w0 == 0xc8（200 成功）→ [0x2f5000+0xff8] 计数 +1 + C2S_HubBeginWithFlow(0x934ac)
  其他（失败/取消）→ HubLogin_CheckOK(0x1608f0)

HubLogin_CheckOK(0x1608f0)（失败恢复）：
  UI_SetPopupProcessInfo(3, 0)          # 流程3 → POPUPSTATE_Pop（关闭面板）
  NetworkStore_GetType()(0x15b0e8) == 0 → [0x2f6000+0xc48] = 1（恢复 HUD）+ NetworkStore_SetState(0)
  NetworkStore_GetType() == 1 → UI_SetPopupProcessInfo(1, 0x12)（另一分支）
```

## 5. 关键状态变量

| 变量 | 地址（GOT） | 语义 |
|---|---|---|
| HUD 绘制总开关 | `[0x2f6000+0xc48]` → 字节 | 0=GAMESTATE_DrawPlay 跳过全部 HUD 绘制；1=正常 |
| 每日奖励触发标志 | `[0x2f5000+0xff8]` → u32 | 0=UIPlayPorting_Draw 每帧触发商店流程；非 0=已处理 |
| NetworkStore type | `[0x30d588]`（GetType 0x15b0e8/SetType 0x15b0dc） | 商店流程类型（0/1） |
| NetworkStore state | `[0x30d580]`（GetState 0x15b0d0） | ==0xf 时 Scene_Init_INAPP_HOT 走网络商店初始化 |
| 首次标记 | `[0x2f5000+0x228]` → 字节 bit0 | 每日奖励首次标记（APPINFO_Save 持久化） |
| LCD 刷新 flag | `[0x3024c0]`（UI_SetRefreshLCDFlag 0xaea60 / Get 0xaea6c） | popup 场景刷屏标志（与 world HUD 无关） |

## 6. HUD 绘制机制（为什么开关=0 就无 HUD）

```
主循环 MainProcess(0xd4984) 每帧：
  UI_PopupProcess(0xaebfc) → STATE_NextStartProcess(0xd46b8)
  POPUPSTATE_Exist() != 0 → POPUPSTATE_Process(0x122608)（只处理面板，GAMESTATE 绘制不运行）
  popup 空 → STATE_ProcessGame(0x151540)：
    [0x2f3000+0x938]→* → GAMESTATE_Process（ProcessPlay 等）
    [0x2f4000+0x930]→* → GAMESTATE_Draw(0x1512b8) → fpDraw

GAMESTATE_DrawPlay(0x9d6cc)：
  ... MAP_DrawBase/LAYER/特效 等世界绘制 ...
  9d720: [0x2f6000+0xc48] → 字节 == 0 → return   # ★ HUD 总开关
  UIPlay_Draw(0xc716c)（掉落物）+ UIPlayPorting_Draw(0xc87a0)（HUD 控件树）
```

- **GAMESTATE_DrawPlay 的两段结构**：世界场景绘制无条件执行；**HUD（UIPlay 控件树 16 控件）被 `[0x2f6000+0xc48]` gate 住**，==0 直接 return
- popup 面板存在期间主循环只跑 POPUPSTATE_Process，GAMESTATE 绘制完全停摆（画面冻结）
- 状态表：fpDraw @`[0x2f4000+0x930]`→*（=0x309990 变量）、fpProcess @`[0x2f3000+0x938]`→*（=0x3099a0）、Enter @`[0x2f4000+0x890]`→*（=0x309988）；state 语义 0=MapChange/1=Play/4=Exit→STATE_Set(4)

## 7. 观察模式 vs 阻断模式（v0.4.18 实测对照）

| 阶段 | 观察模式（无 hook） | 阻断模式（hook 跳过 iapSelectTarget） |
|---|---|---|
| daily_reward 弹出 | ✓（类型 0x1a，Scene_Draw 每帧绘制） | ✓ 相同 |
| HUD 开关 [0x2f6000+0xc48] | 0（商店流程置 0） | 0（相同） |
| 支付弹窗 | Android 原生 Dialog 弹出 | **被 hook 跳过（不弹）** |
| 玩家处理 | 点确认 → 离线失败 → 「连接出错」dialog → ok → 「保存成功」提示 | 无任何可交互（弹窗不弹） |
| HUD 恢复 | 支付失败链 → HubLogin_CheckOK → 开关恢复 1 → world 完整 HUD | **开关永停 0 → GAMESTATE_DrawPlay 跳过 HUD → 无 HUD 卡死** |

- 附：支付失败后游戏自动执行「保存成功」提示（SAVE_ProcessSave 链，无按钮自动消失）；NPC 对话文本区显示纯白矩形（文本渲染 bug）——两者为已知待修项（2026-08-09 暂缓）

## 8. 模块修复（v0.4.19 → v0.4.20）
- 见 docs/systems/save.md §9（完整根因 + 修复 + 真机验证）
- P0-1 (v0.4.20)：**data_recover_after_hive_block() 新增 NetworkStore_SetState(0)**，匹配官方 HubLogin_CheckOK 失败恢复链。
  恢复链现在完整覆盖 4 步：
  1. `[0x2f5000+0xff8]` → u32 = 1（阻止 UIPlayPorting_Draw 再次触发商店流程）
  2. `[0x2f6000+0xc48]` → 字节 = 1（恢复 HUD 绘制开关）
  3. `NetworkStore_SetState(0)`（复位商店状态，修复面板触摸失效/地图切换失败/NPC 对话失效）
  4. `UI_SetPopupProcessInfo(4, 0)`（关闭 daily_reward 面板）
  - 新增符号：`F_NETWORKSTORE_SET_STATE_VMA=0x15b0d0`（game_symbols.h），解析 `fn_networkstore_set_state`（game_access.cpp）
- P0-2 (v0.4.20)：**支付拦截 hook 模块化为 `patch/IapBlocker`**。`HookMain.onPackageLoaded` 委托给 `IapBlocker.install(param)`，hook 逻辑、恢复逻辑、观察模式标志检查全部收敛于 patch 模块内。
- 观察模式开关：游戏私有目录 `skip_hive_block.flag` 存在时跳过 hook（对比原始流程用）
