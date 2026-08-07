# 开发环境与工具链

> 日期：2026-08-05 ｜ 主机：Manjaro Linux x86_64，内核 6.1.174
> 本文档承接 README「快速了解」之外的详细环境信息：依赖清单、SDK/NDK 路径、Python 环境、关键命令、环境验证记录。

## 1. 依赖清单总览（2026-08-05 全部就绪）

> 下表为开发机既有系统级安装（yay 方式）。按「目录规范与环境隔离」，核心要求是**工具输出的文件必须落在项目文件夹内**。
> **SDK 路径：`/opt/android-sdk/`（platforms/android-34 + build-tools/37.0.0 + platform-tools）**

### A. 基础运行环境 ✅

| 程序 | 用途 | 版本 |
|---|---|---|
| OpenJDK 17 | 全部 Java 工具依赖（已切换为默认） | 17.0.19 |

### B. 静态分析链 ✅

| 程序 | 用途 | 版本 |
|---|---|---|
| apktool | 解码 APK / 提取静态数据 | 3.0.3 |
| jadx | 反编译定位 hook 点 | 1.5.6 |
| apksigner / zipalign / aapt2 / d8 | 签名/对齐/资源编译/dex 转换 | build-tools 37.0.0 |
| uber-apk-signer | 一键签名+对齐+验证 | 1.3.0 |

### C. 模块开发链 ✅（全部就绪）

| 程序 | 用途 | 状态 |
|---|---|---|
| Gradle | 命令行构建模块 APK | ✅ 9.6.1（系统）；项目内 wrapper 8.11.1（`module/gradlew`，缓存于项目 `.gradle/`） |
| Android SDK platform | 提供 android.jar（`/opt/android-sdk/platforms/android-34/`） | ✅ android-34 |
| Android SDK build-tools | 编译/打包 Android 模块 | ✅ 37.0.0 |
| **Android NDK** | 编译 native 数据访问层 | ✅ **r26d（26.3.11579264）**，**项目内 `tools/ndk/`**（瘦身至 2.0G，仅 ARM ABI） |
| libxposed API（compileOnly） | LSPosed 现代 Xposed API（`io.github.libxposed:api:101.0.1`） | 📦 Gradle 依赖（项目内） |
| AndServer 库 | 进程内 HTTP 服务 | 📦 Gradle 依赖（项目内） |
| LSPatch (lspatch.jar) | 集成免 root 版 APK | ✅ `tools/lspatch/lspatch.jar`（v0.6，10.8MB） |

### D. 部署与验证链 ✅

| 程序 | 用途 | 状态 |
|---|---|---|
| LSPosed 框架 | 模块运行框架（手机端） | ✅ 用户已有 |
| adb | 装 APK/抓日志/端口转发 | ✅ 1.0.41（android-tools 36.0.1） |
| python-frida（系统级） | 快速原型验证 hook 点 | ✅ 17.7.2（系统 pacman 安装，CLI 未装） |
| **项目 venv（uv 管理）** | 项目内 Python 依赖（frida 17.16.4 + frida-tools 14.10.4） | ✅ `.venv/`，见下文「Python 环境」 |

### E. 关于 MCP 工具套件

- MCP 套件（如 zinja-coder 系列）是**操作接口层**，非工具本身，底层程序仍需自行安装
- 本方案核心工作（模块开发、LSPosed 部署）在 MCP 套件覆盖范围之外
- **结论：无需安装 MCP 工具套件**，仅需 jadx CLI 做静态分析

## 2. Python 环境（uv 管理）

- 项目级 venv：`.venv/`（uv 创建，Python 3.13.11）
- 依赖清单：`pyproject.toml` + 锁定文件 `uv.lock`
- 已装依赖：**frida 17.16.4 + frida-tools 14.10.4**（含 CLI：frida-ps/frida-trace 等）
- 用法：`uv run <命令>`（如 `uv run frida-ps -U`、`uv run python script.py`）
- 版本约束：frida 库版本须与目标设备上的 **frida-server 版本匹配**（当前 17.16.4，后续部署设备时确认）
- 说明：系统级 pacman 的 python-frida 17.7.2 保留但**不用于本项目**；项目一律用 `.venv/`
- 镜像：pyproject.toml 已配置阿里云 PyPI 镜像（`mirrors.aliyun.com`）

## 3. 关键命令

### 3.1 真机开发循环（固定流程，每次重复执行）

```bash
# ① 构建模块（workdir: module/，缓存落项目 .gradle/）
# wrapper zip 曾被清理，直接用缓存发行版（或先让 wrapper 补下载）
GRADLE_BIN=$PWD/../.gradle/wrapper/dists/gradle-8.11.1-bin/*/gradle-8.11.1/bin/gradle
GRADLE_USER_HOME=$PWD/../.gradle $GRADLE_BIN :app:assembleDebug --no-daemon
# 产物 → output/inotia4-export-module-<版本>.apk（自行复制 + 版本号递增，见 README 规则 6）

# ② 部署（覆盖安装，LSPosed 启用状态按包名保留）
adb install -r output/inotia4-export-module-v0.3.6.apk

# ③ 重启游戏（让 Xposed 重新注入，模块更新生效的必需步骤）
# 按包名 force-stop 即可，**无需 pid**；monkey 启动与桌面点击等价
# 游戏启动约 10 秒到主菜单（state=4）
adb shell am force-stop com.com2us.inotia4.normal.freefull.google.global.android.common
adb shell monkey -p com.com2us.inotia4.normal.freefull.google.global.android.common -c android.intent.category.LAUNCHER 1

# ④ 等待 API 就绪（8088 端口；curl 轮询比 /proc/net/tcp 可靠）
# API 可达（能返回 JSON）即代表模块已注入、游戏启动完成；轮询到 "screen" 字段说明模块数据通路就绪
until curl -s -m 2 http://100.110.139.83:8088/api/info/gamestate | grep -q '"screen"'; do sleep 2; done

# ⑤ 自动进入游戏世界（触摸注入，主菜单→存档槽1→确认×2）
uv run python scripts/touch_automation.py click 1700,1200 0.1 click 2000,800 0.3 click 1700,350 1.5 click 1680,1030 0.3 click 1715,750 0.1 click 1715,750 0.1
# 验证：screen=world 即进入世界
curl -s http://100.110.139.83:8088/api/info/gamestate
```

> **游戏重启与进程定位**：`am force-stop <包名>` 按包名杀进程，**不需要 pid**（pid 每次重启都变，不必查询）。
> frida attach 也用**进程显示名**（`adb shell ps | grep 包名` 的 NAME 列，如 "Inotia4"），不用 pid。
> 仅当需要 pid 时：`adb shell pidof com.com2us.inotia4.normal.freefull.google.global.android.common`。

### 3.2 常用脚本速查（均须 `uv run`）

| 脚本 | 用途 | 用法 | 默认 |
|---|---|---|---|
| `scripts/analyze/check_symbols.py` | 符号一致性校验（**改 game_symbols.h 后必跑**） | `uv run python scripts/analyze/check_symbols.py [libgame.so路径]` | `apk/decoded/lib/arm64-v8a/libgame.so`，比对 39 符号 |
| `scripts/analyze/api_poll.py` | 连续轮询 player/party/inventory 检测字段变化 | `uv run python scripts/analyze/api_poll.py <IP> [间隔秒] [次数]` | `100.110.139.83`, 2.0s, 30 次 |
| `scripts/analyze/live_session.py` | 联调全自动会话（局域网/Tailscale 通用采样） | `uv run python scripts/analyze/live_session.py [IP] [时长上限分钟]` | `192.168.3.11`, 上限 5min |
| `scripts/parse/package_assets.py` | 静态数据重打包进模块 assets（M3 产物 → module/assets） | `uv run python scripts/parse/package_assets.py` | 28 表 + zh-Hans/en 语言 |
| `scripts/touch_automation.py` | adb 触摸注入（执行模式）+ 实时检测（无参数=检测模式） | `uv run python scripts/touch_automation.py click 100,200 0.5 ...` | 3168x1440 逻辑坐标，自动旋转校准 |

### 3.3 设备连接方式（优先级）

按优先级依次尝试连接真机（adb）：

1. **USB**：`adb devices`
2. **局域网**：`adb connect 192.168.3.11:5555`
3. **Tailscale**：`adb connect 100.110.139.83:5555`

注意：**禁止自行扫描局域网 IP 连接**——无法连接即无设备，跳过需设备的部分并报告用户。

### 3.4 其他常用命令

```bash
# 符号查询（workdir: 项目根，libgame.so 符号表）
grep " INVEN_GetMoney" apk/decompiled/libgame-symbols.txt

# 抓模块日志（tag: Inotia4Export；日志文件在手机 sdcard/Android/data/<包名>/files/）
adb logcat -s Inotia4Export:V

# 反汇编定位（改 game_symbols.h 时用）
tools/ndk/.../llvm-objdump -d --start-address=0x... --stop-address=0x... apk/decoded/lib/arm64-v8a/libgame.so
```

> 构建注意：Gradle 中间产物在 `module/**/build/`，最终 APK 复制到 `output/` 后验收交付；
> `GRADLE_USER_HOME=$PWD/.gradle` 为可选构建缓存隔离（非强制）。

## 4. 环境验证记录（2026-08-05）

| 验证项 | 结果 |
|---|---|
| apktool | ✅ 3.0.3 |
| jadx | ✅ 1.5.6 |
| apksigner | ✅ 0.9（build-tools 37.0.0） |
| adb | ✅ 1.0.41 |
| zipalign | ✅ |
| uber-apk-signer | ✅ 1.3.0 |
| Java | ✅ OpenJDK 17.0.19（已切换默认，原 1.8 弃用） |
| python-frida | ✅ 17.7.2（CLI 未装，可选） |
| Android SDK | ✅ `/opt/android-sdk/`：platforms/android-34 + build-tools/37.0.0 + platform-tools |
| LSPatch | ✅ `tools/lspatch/lspatch.jar`（v0.6，10.8MB，`java -jar` 可运行） |
| APK 解码 | ✅ `apktool d` 成功，输出至 `apk/decoded/` |

## 5. 环境相关已知待办

- [x] **y7000 模拟器环境**（2026-08-05 实测完结：TCG ARM VM boot 25+ 分钟未完成；x86_64 转译路线 frida 不可用 + LSPatch native 高风险，详见 `docs/deployment/emulator-research.md` §6-7）→ 模拟器路线冻结，转向真机
- [x] **实体 root 手机就绪**（✅ oneplus-13 已配置 root + Zygisk-LSPosed 并真机联调）
- [ ] **LSPatch 与 libxposed 101 兼容性**（LSPatch 0.6 内置 runtime 较旧，API 101 模块兼容待实测；必要时降级 API 93 构建）
- [ ] android.jar 引用方式：Gradle 通过 `ANDROID_HOME=/opt/android-sdk` 或 local.properties 指向 SDK（当前已用 `local.properties` 的 `sdk.dir` 解决）

## 5a. 环境踩坑记录（历次会话沉淀）

> 环境/工具链相关的踩坑集中在本文档；模拟器与跨平台相关见 `docs/deployment/emulator-research.md` §6-7；操作端点逆向结论见 `docs/reference/hook-points.md`。

1. **sdkmanager 旧版 JDK 不兼容**（javax.xml.bind 缺失）→ 直接写 license 文件绕过（不跑 sdkmanager --licenses）。
2. **AndServer 坐标**：2.x 是 `com.yanzhenjie.andserver:api/annotation/processor`（+kapt），不是 `com.yanzhenjie:andserver`。
3. **AndServer 2.1.12 插件无 Gradle marker**（`com.yanzhenjie.andserver.gradle.plugin` 在 Maven Central 404）→ 必须 `buildscript { classpath("com.yanzhenjie.andserver:plugin:2.1.12") }` + `apply(plugin=...)`，不能用 `plugins {}` DSL。且 **2.1.12 就是最新版**（勿升级）。
4. **AndServer processor 依赖缺下载**：`commons-collections4`/`commons-lang3` 等首次解析未下载 → 先跑 `:app:dependencies --configuration kaptDebug` 触发下载。
5. **SDK 无 CMake**：NDK 瘦身移除 cmake。系统 cmake 4.4 通过 `local.properties` 加 `cmake.dir=/usr` 使用（AGP 找 `<dir>/bin/cmake`）；`android.ndkVersion` 须显式声明（26.3.11579264 匹配 r26d）。
6. **libxposed 101 写法**：`class XposedMain : XposedModule()` + `override fun onModuleLoaded(param: XposedModuleInterface.ModuleLoadedParam)`（101 起无参构造 + attachFramework 自动调用；参考 LSPosed/CorePatch）。
7. **y7000 跨平台工具链**（2026-08-05 实测）：Windows OpenSSH 结束会话会终止 Start-Process 后台进程（长任务用 schtasks Interactive 登录）；aria2 `--all-proxy` 不支持 socks5://（只认 http://，127.0.0.1:20170 多线程 GB 级/分钟）；wsl.exe 输出为 UTF-16（PowerShell 调用后 grep 判二进制 → 重定向文件再 Get-Content）。
8. **Gradle wrapper zip 曾被清理**：wrapper 需重新下载（services.gradle.org 超时）→ 直接用缓存发行版 `../.gradle/wrapper/dists/gradle-8.11.1-bin/*/gradle-8.11.1/bin/gradle`。
9. **AGP 依赖下载慢**（国外仓库）→ 阿里云镜像（settings.gradle.kts 已配）。

## 6. 关联文档

- 项目总览 / 目录规范：`README.md`
- 代码结构（NDK/CMake/依赖配置说明）：`docs/architecture.md`
- 真机部署与联调流程：`docs/deployment/phone-dev-workflow.md`
