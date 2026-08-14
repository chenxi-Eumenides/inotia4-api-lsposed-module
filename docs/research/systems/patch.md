# 游戏补丁（Patch）笔记

> 目录：docs/systems/ ｜ 主题：**对游戏本身的 hook 补丁**（Java 层 libxposed hook）——与软件开发主线（API 导出）无关，仅修正游戏体验/行为
> 代码位置：`module/app/src/main/java/com/inotia4/export/patch/`（每个补丁一个 object，`install(param, hooker)` 模式，由 `HookMain.onPackageLoaded` 委托）
> 关联：docs/systems/iap.md（支付系统逆向）、docs/systems/ui.md（UI 状态逆向）

## 1. 补丁总览

| 补丁 | 文件 | hook 目标 | 目的 | 版本 |
|---|---|---|---|---|
| 支付框阻断 | `patch/IapBlocker.kt` | `SelectTarget.iapSelectTarget` | 跳过 Hive 付费弹窗，进档直接进 world | v0.4.18→v0.4.20 |
| 沉浸模式 | `patch/ImmersiveMode.kt` | `MainActivity.onWindowFocusChanged` | 隐藏导航栏/手势条（游戏无原生沉浸模式） | v0.4.36 |

hook 模式统一：`param.getDefaultClassLoader()` 加载目标类 → `getDeclaredMethod` → `hook(method).setExceptionMode(PROTECTIVE).intercept { chain -> ... }`。
`PROTECTIVE` 模式：hook 逻辑抛异常不影响宿主（异常被框架捕获并记录，不传播到游戏）。

---

## 2. 支付框阻断（IapBlocker）

### 背景
进档后触发链（见 iap.md §3）：`UIPlay_CallInAppShopProc` → daily_reward 面板 + HUD 开关置 0 + `InApp_SelectTarget` → Java `SelectTarget.iapSelectTarget` 弹 Hive 支付 Dialog。联网已删 → 支付必失败，且 hook 阻断后 HUD 开关不恢复 → world 无 HUD 卡死。

### hook 目标
`com.com2us.module.inapp.SelectTarget.iapSelectTarget(Activity, SurfaceViewWrapper, SelectTargetCallback, long)`：
- intercept 返回 `null` **阻断原方法**（不弹支付框）
- 随后调 `IapBlocker.recover()` → `NativeBridge.nativeRecoverAfterHiveBlock()` → `data_recover_after_hive_block()`（game_data.cpp）模拟支付流程结束：
  1. `[0x2f5000+0xff8]` → u32 = 1（阻止 UIPlayPorting_Draw 再次触发商店流程）
  2. `[0x2f6000+0xc48]` → 字节 = 1（恢复 HUD 绘制开关）
  3. `NetworkStore_SetState(0)`（复位商店状态）
  4. `UI_SetPopupProcessInfo(4, 0)`（关闭 daily_reward 面板）

### 观察模式
游戏私有目录 `skip_hive_block.flag` 存在时跳过 hook（对比原始流程用）。

### 验证
✅ 真机（v0.4.18-0.4.20）：进档直接进 world 有完整 HUD，无支付弹窗。

---

## 3. 沉浸模式（ImmersiveMode）

### 背景
游戏无原生沉浸模式：主题虽为 `NoTitleBar.Fullscreen`（状态栏已隐藏），但**导航栏/手势条始终显示**。

### hook 目标
`MainActivity.onWindowFocusChanged(boolean)`：intercept 先 `chain.proceed()` 走原逻辑（不阻断！），获焦时调 `applyImmersive(activity)` 隐藏系统栏：
- API 30+：`window.insetsController.hide(WindowInsets.Type.systemBars())` + `BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`（边缘滑动临时显示）
- API 30 以下：`SYSTEM_UI_FLAG_IMMERSIVE_STICKY | FULLSCREEN | HIDE_NAVIGATION | LAYOUT_*`

选获焦回调而非 onResume：焦点变化每次重新获焦（含弹窗/对话框关闭后）都会回调，可反复隐藏系统栏；onResume 只在首次进入触发。

### 验证
✅ 真机（v0.4.36）：用户确认沉浸模式生效（导航栏/手势条隐藏）。日志 `ImmersiveMode hook installed on MainActivity.onWindowFocusChanged`。

---

## 4. 补丁与主线分离说明

- 补丁 = 修改游戏行为本身（hook Java 层），**不导出任何 API 数据**，与 API 主线（native 导出 + AndServer）无耦合
- 主线开发只依赖 native 层（game_data.cpp/game_access.cpp + JNI 桥），补丁是独立旁路
- 新增补丁遵循：`patch/` 包下新建 object → `HookMain.onPackageLoaded` 委托 → 本文件登记
