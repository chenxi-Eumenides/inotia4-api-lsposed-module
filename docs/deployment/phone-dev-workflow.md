# 手机端开发调试工作流（M5）

> 日期：2026-08-05 ｜ 用途：root 手机（Android 11+ / Zygisk-LSPosed）上的模块开发、frida 动态分析、API 联调
> 背景：模拟器路线已全部否定（见 emulator-research.md §6-7），**真机是 frida 分析与 LSPatch native 验证的唯一可靠路径**
> **✅ 当前状态：实体手机已就绪**——oneplus-13（root + Zygisk-LSPosed，Android 11+）已配置并完成真机联调
> 关键包名：游戏 `com.com2us.inotia4.normal.freefull.google.global.android.common` ｜ 模块：`output/inotia4-export-module-*.apk`

## 1. 自动化程度概览

| 环节 | 自动化 | 说明 |
|---|---|---|
| adb 连接 | ✅ 一次性配对后全自动 | USB 或 Android 11+ 无线调试（`adb pair`） |
| 安装模块 | ✅ `adb install -r` | 覆盖安装，LSPosed 启用状态保留 |
| 启动游戏 | ✅ `adb shell am start` | |
| frida 注入 | ✅ 全自动 | frida-server 常驻 + `adb forward` + frida CLI/脚本 |
| 日志 | ✅ `adb logcat` | 按模块 tag 过滤 |
| 数据采集 | ✅ 局域网 curl | 模块 HTTP API 直连手机 Wi-Fi IP |
| **手动（仅一次性）** | ❌ 3 项 | USB 调试开关、RSA 授权、LSPosed 启用模块 |

## 2. 前置条件

- 手机：root + Zygisk-LSPosed（Android 11+）
- 开发机：adb（`android-tools`）、项目 `.venv/`（frida 17.16.4 + frida-tools 14.10.4）
- **frida-server 版本必须与 `.venv/` frida 匹配**：17.16.4，架构 **android-arm64**（手机是 ARM64）
  - 下载：https://github.com/frida/frida/releases/tag/17.16.4 → `frida-server-17.16.4-android-arm64.xz`

## 3. 一次性初始配置（手动，约 5 分钟）

### 3.1 开启 USB 调试（手机 UI）

`设置 → 关于手机 → 连点版本号 7 次` → `开发者选项 → USB 调试` 开启。

### 3.2 连接 + RSA 授权（一次）

```bash
adb devices                 # 手机插 USB，确认设备出现
# 手机弹「允许 USB 调试」→ 勾选「始终允许」→ 允许
```

### 3.3 （推荐）Wi-Fi 无线调试配对（免 USB 线）

```bash
# 手机：开发者选项 → 无线调试 → 开启 → 「使用配对码配对设备」
adb pair 手机IP:配对端口     # 输入配对码（一次性）
adb connect 手机IP:连接端口  # 之后每次 adb connect 即可，USB 线可拔
```

### 3.4 部署 frida-server（常驻）

```bash
adb push frida-server-17.16.4-android-arm64 /data/local/tmp/frida-server
adb shell "chmod 755 /data/local/tmp/frida-server && /data/local/tmp/frida-server &"
adb forward tcp:27042 tcp:27042    # frida 默认端口
# 验证：uv run frida-ps -U | head     # 应列出进程
```

> 手机重启后 frida-server 需重新启动（或装 Magisk 模块 frida-server 自动启动，可选）。

### 3.5 LSPosed 启用模块（一次，约 10 秒）

1. 安装模块 APK：`adb install output/inotia4-export-module-*.apk`
2. 手机 LSPosed 界面 → 模块 → 找到导出模块 → 启用开关
3. 勾选作用域：**游戏包名**（`com.com2us.inotia4.*`）
4. 按 LSPosed 提示重启（或强制停止游戏进程）

## 4. 日常开发循环（全命令化）

> 固定流程（构建→部署→重启→等待就绪→进入世界）与脚本速查以 **`docs/environment.md` §3 为权威**，此处为流程概览。

```bash
# 0. 连接
adb connect 手机IP                 # Wi-Fi；USB 则 adb devices 确认

# 1. 构建模块（workdir: module/；完整命令见 environment.md §3.1①）
GRADLE_BIN=$PWD/../.gradle/wrapper/dists/gradle-8.11.1-bin/*/gradle-8.11.1/bin/gradle
GRADLE_USER_HOME=$PWD/../.gradle $GRADLE_BIN :app:assembleDebug --no-daemon
# 产物 → output/inotia4-export-module-*.apk（复制 + 版本递增，见 README 规则 6）

# 2. 部署（覆盖安装，LSPosed 启用状态保留）
adb install -r output/inotia4-export-module-*.apk

# 3. 重启游戏进程（让 Xposed 重新注入，模块更新生效的必需步骤）
# 按包名 force-stop，无需 pid
adb shell am force-stop com.com2us.inotia4.normal.freefull.google.global.android.common
adb shell monkey -p com.com2us.inotia4.normal.freefull.google.global.android.common -c android.intent.category.LAUNCHER 1
until curl -s -m 2 http://192.168.3.11:8088/api/info/ui | grep -q '"state"'; do sleep 2; done   # 等 8088 就绪

# 4. frida 动态分析（验证 hook 点/读结构体）
# 进程名用 adb shell ps 的 NAME 列（如 "Inotia4"），非包名、非 pid
uv run frida -U -n <进程显示名> -l scripts/frida/xxx.js

# 5. 抓模块日志
adb logcat -s Inotia4Export:V        # 模块日志 tag；文件版在手机 sdcard/Android/data/<包名>/files/

# 6. 数据采集（无需 adb，局域网直连手机 Wi-Fi IP）
curl http://手机IP:8088/api/info/player

# 7. 自动进入游戏世界（完整序列见 environment.md §3.1⑤）
uv run python scripts/touch_automation.py click 1700,1200 0.1 click 2000,800 0.3 click 1700,350 1.5 click 1680,1030 0.3 click 1715,750 0.1 click 1715,750 0.1
# 流程：开始游戏 → 登录弹窗否 → 存档槽1 → 进入游戏 → 确认×2；curl 验证 state=5
# 无参数运行 = 检测模式（实时打印触摸坐标，用于调试定位按钮）
```

## 5. 模块更新注意事项

- **更新 APK（`adb install -r` 同包名升级）后不需要重新在 LSPosed 启用**——启用状态按包名记录，自动保留
- **必须重启游戏进程**才生效：Xposed 在目标进程启动时注入（第 4 节第 3 步）
- 仅当新版模块修改了 `scope.list` 且新增未勾选的作用域时才需回 LSPosed 界面补充勾选（本项目 scope 固定游戏包名，不受影响）

## 6. 断连独立运行（采集模式）

- **运行 API 服务不需要 adb 连接**：模块在游戏进程内监听 `0.0.0.0`，局域网程序直接访问手机 Wi-Fi IP
- 手机独立运行需供电：USB 充电线即可（不通数据也供电）
- 若要局域网可发现，可固定手机 Wi-Fi IP（路由器 DHCP 绑定）或配 mDNS

## 7. 常见问题

| 问题 | 原因 / 修复 |
|---|---|
| `adb devices` 无设备 | 未授权 RSA / 换 USB 口 / 驱动（Windows）；`adb kill-server` 重启 |
| frida 连接失败 | frida-server 版本与 `.venv/` 不匹配（必须 17.16.4）；`adb forward` 未执行；手机重启后 frida-server 未拉起 |
| 模块不生效 | 未在 LSPosed 启用 / 作用域未勾游戏 / 更新后未重启游戏进程 |
| API 不通 | 模块 HTTP 监听地址是否 `0.0.0.0`（非 `127.0.0.1`）；手机与消费端是否同一局域网；`usesCleartextTraffic`（Android 9+ 明文 HTTP） |
| 游戏闪退 | 模块 native 层崩溃 → `adb logcat -s AndroidRuntime ExportModule` 定位 |

## 8. 关联文档

- 数据访问方案（native base+VMA 直读）：`docs/reference/hook-points.md`
- API 规格：`docs/api-spec.md`
- 模拟器选型结论（为何用真机）：`docs/deployment/emulator-research.md`
