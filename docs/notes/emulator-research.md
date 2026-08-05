# 模拟器选型调研结论（2026-08-05）

> 背景：本机（Manjaro 开发机）**不跑模拟器**。模拟器/真机是 LSPatch 集成版部署联调（M6）与 frida 动态分析的候选环境。
> 触发：y7000-setup.md 原推荐「官方 Emulator x86_64 API30 镜像（内置 ARM 翻译）」用于 frida 动态分析，但调研证明该定位不成立。
> 本文件仅记录调研结论，**y7000-setup.md 维持原样**，待实际部署时再据本文决策。

## 1. 核心结论（一句话）

**x86_64 模拟器 + ARM 翻译层下，游戏本体能跑，但 frida 无法 hook ARM native 代码，LSPatch 模块 native 层跨架构调用属未实证高风险；frida 动态分析应改用 ARM64 真机或真 ARM VM。**

## 2. 三份并行调研结论

### 2.1 官方 Android Emulator 的 ARM 翻译（libndk_translation）

| 项 | 结论 |
|---|---|
| 支持的镜像 | **仅 API 30 / API 31 x86_64（及 API 28 x86 32 位）内置翻译层**，同时覆盖 ARMv7 + ARM64；API 29 无内置、API 33+ 已移除 |
| 官方立场 | Google 官方 2020-03 公告，仅 Google API/Play 镜像，2025 年仍在修 bug（非新方案） |
| 无新替代 | 无「Google+Intel 新 ARM 转译」发布；Berberis 是 riscv64→x86_64，与本场景无关 |
| 游戏本体 | 艾诺迪亚4（2012 年老游戏，无复杂 ICU/JIT）**大概率能跑** |
| **frida hook ARM** | ❌ **不可行**：x86_64 frida agent 看不到 ARM 模块（enumerateModules 返回空）、无法 patch ARM 指令；`--realm=emulated` 对 native bridge 报 "process is not using emulation" |
| LSPatch 模块 | Java 层 hook ✅ 可用（ART 是 x86 的）；native 层 dlopen/dlsym 仅当模块以 **arm64-v8a ABI 注入**、与游戏同处翻译层内才可能工作，**无可靠实测，高风险** |
| 已知兼容坑 | ICU 调用 SIGABRT（API 30）、F16C CPUID 缺失、ARM 侧 libvulkan 不可翻译、翻译层缓存导致运行时内存 patch 无效、外部线程调翻译库 SIGSEGV |

来源：官方[博客公告](https://android-developers.googleblog.com/2020/03/run-arm-apps-on-android-emulator.html)、[emulator release notes](https://developer.android.com/studio/releases/emulator)、[frida#3497](https://github.com/frida/frida/issues/3497)、[frida#3421](https://github.com/frida/frida/issues/3421)、[dotnet/runtime#109463](https://github.com/dotnet/runtime/issues/109463)、[Ch'Gans 博客](https://chgans.design.blog/2021/05/23/adding-arm-native-bridge-to-the-aosp11-x86-emulator/)

### 2.2 Waydroid x86_64 + libndk/libhoudini 转译

| 项 | 结论 |
|---|---|
| 可行性 | **可复活**（撤销此前「Waydroid 不可行」结论），老式轻量 native 游戏成功率最高；前提：x86_64 镜像 + **Android 11 或 13**（转译 blob 仅这两个）+ 转译层宣告全 ABI 覆盖 |
| libhoudini 时间炸弹 | libhoudini.so 硬编码 **2026-01-01 过期**（2026-01 当日 Blue Archive/FGO 等集体崩溃）；PR #256 已换新版 blob 修复，但 2026 仍有残留崩溃报告；**选 libndk 可彻底绕开** |
| **frida** | ❌ 不可行：Waydroid 不在 frida 支持列表（bootstrapper crash、spawn 失败，见 frida#2914、waydroid#1989） |
| LSPosed + native 符号 | 宿主 x86_64 linker 无法 dlopen 游戏 ARM .so（报 EM_ARM/EM_386 不匹配）；**硬前提：模块 native 层仅 arm64-v8a**、与游戏同跑转译 guest 空间，dlopen/dlsym 才可用；有现成模板 [arm64-houdini-lsposed-framework](https://github.com/Jordan231111/arm64-houdini-lsposed-framework)（2026-05） |
| 未实证风险 | LSPatch 自带 liblspatch.so 含 x86_64 版，其 bootstrap 与 guest 进程混合的稳定性**无公开先例，需 PoC** |
| 成熟度对比 | 低于官方模拟器：社区提取冻结 blob（libndk 0.2.3 源自 2021 guybrush 固件）、官方拒绝内置、时间炸弹事故；优点是容器轻、x86 原生应用近原生性能 |

来源：[waydroid_script PR#256](https://github.com/casualsnek/waydroid_script/pull/256)、[issue#257](https://github.com/casualsnek/waydroid_script/issues/257)、[waydroid-helper#78](https://github.com/waydroid-helper/waydroid-helper/issues/78)、[waydroid#2051](https://github.com/waydroid/waydroid/issues/2051)、[waydroid#968](https://github.com/waydroid/waydroid/issues/968)、[Garuda 论坛实测](https://forum.garudalinux.org/t/ultimate-guide-to-install-waydroid-in-any-arch-based-distro-especially-garuda/15902)

### 2.3 frida 在翻译层环境下的动态分析可行性

| 项 | 结论 |
|---|---|
| 机制 | frida Interceptor 架构绑定：x86_64 agent 无法 patch ARM64 机器码；翻译层把 ARM 代码在 dlopen 时翻译缓存，运行时对 ARM 内存打补丁无效 |
| 仅读内存 | 部分可行：/proc/maps 可见 ARM 库映射，可 Memory.read 读全局变量，但需静态分析 + 手动解析基址，ASLR 下每次启动基址变化，仅临时手段 |
| 可行替代（推荐序） | **① ARM64 真机（root 手机）**：未 strip 符号直接 findExportByName，零成本；**② LineageOS-qemu ARM VM**：frida-server-arm64 同架构，TCG 慢但可用；③ frida-gadget-arm64 注入 APK（侵入式，每更新重打一次，坑多） |
| x86 版 APK | 本游戏无 x86 版，不适用；即使有也是另一个二进制，符号/偏移不能迁移到 ARM 版 |

来源：[frida#3497](https://github.com/frida/frida/issues/3497)、[frida#3421](https://github.com/frida/frida/issues/3421)、[frida#2366](https://github.com/frida/frida/issues/2366)、[frida#920](https://github.com/frida/frida/issues/920)、[frida 官方文档](https://frida.re/docs/android/)、[android-houdini-injection（21 坑）](https://github.com/luruizhe953-netizen/android-houdini-injection)、[jqssun/android-lineage-qemu](https://github.com/jqssun/android-lineage-qemu)

## 3. 方案对比（对本项目）

| 方案 | 游戏本体 | frida hook | LSPatch 集成版 native 层 | 适用定位 |
|---|---|---|---|---|
| 官方 Emulator x86_64 API30/31 | ✅ | ❌ | ⚠️ 未实证 | 仅集成版联调（Java 层） |
| Waydroid A13 + libndk | ✅ | ❌ | ⚠️ 有模板，需 PoC | 服务器 headless 联调 |
| LineageOS-qemu ARM VM | ✅ | ✅ | ✅ 无翻译层问题 | 全功能（慢） |
| **root 手机（用户已有）** | ✅ | ✅ | — | **frida 动态分析首选** |

## 4. 对本项目的建议

1. **frida 动态分析**（M2 后续结构体逆向）：直接在用户 root 手机上做（frida-server 17.16.4 与项目 `.venv/` 匹配），不依赖模拟器
2. **LSPatch 集成版联调**（M6）：任何 x86_64 翻译方案下 native 层跨架构调用均为高风险，需 PoC 验证；PoC 关键路径：模块**只打包 arm64-v8a** → 游戏启动 → `System.loadLibrary` + guest 内 dlopen/dlsym 游戏 .so
3. **LSPatch 0.6 与 libxposed 101 兼容性**（README 已知待办）与上述 PoC 叠加，风险集中在 M6，建议先在真机验证模块功能、再上模拟器验集成版形态
4. 若 PoC 卡壳：官方模拟器（Google 维护翻译层）优先于 Waydroid（社区冻结 blob）

## 5. 待验证项（PoC 清单）

- [ ] 模拟器上游戏本体能否启动（官方 Emulator API30 或 Waydroid A13 + libndk）
- [ ] 模块仅 arm64-v8a 打包后，guest 空间内 dlopen/dlsym 游戏 .so 是否可用
- [ ] LSPatch bootstrap（liblspatch.so x86_64 版）与 guest 进程混合是否稳定
- [ ] LSPatch 0.6 与 libxposed 101 兼容性（必要时降级 API 93）

## 6. 实测记录（2026-08-05，y7000 Windows）

### 6.1 TCG aarch64（LineageOS-qemu ARM VM）→ 判定不可用

- 环境：y7000（Windows 11 26200），QEMU 11（scoop），`-accel tcg,tb-size=1024,thread=multi`，16 线程多线程 TCG
- 计时：t0=12:46:04 启动 → t1=12:47 5555 端口监听（1 分钟）→ **t2 完整 boot 25+ 分钟未完成**（boot 动画 1fps，qemu 进程最终消失）
- 结论：**LineageOS-qemu ARM VM（方案②）正式排除**——TCG 纯软件翻译在笔记本级 CPU 上无法完成 Android 14 boot
- 附带发现：SSH 会话断开会终止 Start-Process 启动的后台进程（Windows OpenSSH 行为）；y7000 无 adb，需自带 platform-tools

### 6.2 Dock-Droid（sickcodes/dock-droid）→ 排除

- 「KVM 加速 ARM 系统」是**误导**：Dockerfile 硬编码 `qemu-system-x86_64`，从不运行 ARM 系统镜像；「Android ARM」指 x86_64 guest 内的 ARM 转译层
- **已死 4 年**（最后 commit 2022-04，22 open issues 无响应），默认 BlissOS Android 9（2020），连 x86 镜像都大量启动失败
- 来源：[dock-droid](https://github.com/sickcodes/dock-droid)（Dockerfile#L212-L216、issue #3/#20/#4）

### 6.3 ReDroid（redroid）→ 仅冒烟测试可用

- 官方立场：x86_64 跑 ARM 应用「可能但不推荐、不稳定」；arm64 镜像实际只能跑 ARM64 主机（binder 依赖，issue #719）
- 转译组合：x86_64 镜像 + libndk（A11=0.2.2 / A12-14=0.2.3）；redroid 15/16 官方镜像已移除 ndk_translation.rc（issue #910）
- **核心限制**：redroid 作者明言「native bridge 主要对 JNI 库有效，**dlopen 库可能无效**」（issue #669）——正中 LSPatch 模块 dlopen/dlsym 机制
- frida：x86_64 + 转译下 hook ARM 不可行（frida#3497），唯一出路 frida-gadget-arm64 注入 APK
- WSL2 前置成本：默认内核无 binder，需编译自定义内核（[redroid-wsl2-android-lab](https://github.com/Veritas-Quaesitor/redroid-wsl2-android-lab) 有现成方案）+ Docker NAT 兼容（iptables-legacy）
- headless ✅；**不依赖 /dev/kvm**（容器直跑宿主内核）
- 结论：只适合「游戏能否在转译层启动」的冒烟测试，不是开发平台

## 7. 最终结论（2026-08-05）

**所有 x86_64 模拟/容器路线（TCG aarch64 / 官方 Emulator / Waydroid / Dock-Droid / ReDroid）共同天花板**：

```
游戏能跑 ✅ ｜ frida hook ARM ❌ ｜ LSPatch native dlopen/dlsym ❌（官方明示可能无效）
```

- frida 动态分析 + LSPatch 模块 native 验证的**唯一可靠路径 = ARM 真机（用户 root 手机）**
- 模拟器/容器仅用于「游戏本体能否启动」冒烟测试，不承担开发验证职责
