# 安卓游戏信息 API 导出项目

## 项目目标

对一个安卓游戏进行修改与二次开发，将游戏内部信息（运行时状态 + 静态数据）通过 HTTP API 形式导出，供局域网内的其他程序消费。

## 项目背景

- **游戏**：艾诺迪亚4（Inotia 4，Com2uS）盗版大修 v5.0，单机/离线，无源码只有 APK（48.4MB），无加固，已被作者重打包重签。包名：`com.com2us.inotia4.normal.freefull.google.global.android.common`（adb/LSPosed scope/frida 等开发命令均需此包名）
- **引擎**：原生 Java（classes.dex 10MB + classes2.dex 2.8MB）+ 自研引擎 native 库 `libgame.so`（arm64-v8a + armeabi-v7a，**未 strip 符号**）
- **API 消费者**：局域网内其他程序
- **运行环境（真机为主，2026-08-05 确认）**：
  1. **实体 root 手机：LSPosed 模块版（主路线 = 最终部署目标）**——✅ 已就绪（oneplus-13：root + Zygisk-LSPosed，局域网 192.168.3.11，真机联调中）
  2. 服务器（Waydroid 集成版）：受限于游戏 ARM-only + x86 转译层风险（见 `docs/deployment/emulator-research.md` §7），**降级为延伸目标，非当前重点**

## 需求清单

### 功能需求

- 导出运行时状态数据（血量、金币、等级、坐标、背包等）— ✅ 已确认
- 导出静态资源数据（角色数值表、道具配置、地图数据等）— ✅ 已确认（全量提取）
- 通过 HTTP API 对外提供数据（REST + JSON）— ✅ 已确认
- 局域网内其他程序可访问 API — ✅ 已确认

### 非功能需求

- **游戏本体零修改**：模块独立于游戏 APK，以 Xposed 模块形式注入 — ✅ 已确认
- **双交付物，模块版为主**：日常只构建 LSPosed 模块版（`output/inotia4-export-module-*.apk`）；LSPatch 集成版（免 root）仅在需要时集成构建 — ✅ 已确认

### 需求确认记录（2026-08-05）

- ✅ **已确认**：导出字段（金币/HP MP/等级经验/坐标地图/背包装备/技能/佣兵，3 名出战角色）与静态数据全量——**具体字段清单见 `docs/api-spec.md` §2 信息清单**
- [ ] **待确认**：API 消费者数量与访问方式（单程序 / 多客户端）

## 技术方案（概要）

- **一套代码，双交付物（模块版为主）**：导出模块只开发一次，日常构建 LSPosed 模块版；LSPatch 集成版仅在需要时集成
  - **形态 A（模块版）✅ 主路线**：LSPosed 独立模块 APK（libxposed 101 + AndServer 内嵌 HTTP 服务），游戏 APK 零修改，日常开发/构建对象
  - **形态 B（集成版）🔄 按需集成**：LSPatch 集成 APK（免 root 单文件），受游戏 ARM-only 限制，仅在需要免 root 部署时构建
- **数据访问（✅ 真机验证）**：模块 native 层通过 **`/proc/self/maps` 基址 + 符号 VMA 直读** libgame.so 全局变量与 Getter 函数（**不用 dlopen/dlsym**——namespace 隔离会加载独立副本读不到数据）
- **API 服务 = Java（AndServer 内嵌于模块进程）**：数据获取在 native（C/C++），API 服务用 Java，两者通过 JNI 桥接，同一进程内运行
- **静态数据**：Python 脚本解析 game_res 格式（M3 完成）→ JSON 数据库（可交付）
- **Frida**：仅开发期原型验证用，不进交付物

> **详细架构与规范见 `docs/architecture.md`（唯一权威）**；逆向细节见 `docs/reference/hook-points.md`；
> API 端点与数据模型见 `docs/api-spec.md`。

## 开发进度

> 开发方式：**游戏逆向分析与模块开发同步推进**（逆向结论直接指导 API 设计，真机验证反馈修正逆向），而非串行阶段。

| 工作线 | 内容 | 状态 |
|---|---|---|
| **环境** | 开发/分析工具链搭建（Gradle/NDK/Python/frida/真机 oneplus-13） | ✅ 就绪（见 `docs/environment.md`） |
| **游戏分析** | 引擎识别/静态表解析（100 表+6 语言）/hook 点定位/操作函数逆向 | ✅ 完成（见 `docs/reference/`） |
| **模块开发** | libxposed 101 + native 数据访问 + AndServer API（分层重构） | ✅ 完成（v0.3.8，见 `docs/architecture.md`） |
| **真机联调** | 只读端点 v0.2.16-0.2.34 全验证；操作端点 v0.3.2-0.3.6 真机验证修复（switch/use-item/discard/move/equip/party 六项，逆向结论见 `docs/reference/hook-points.md`） | ✅ 完成 |
| **集成版** | LSPatch 集成免 root 版（按需） | 🔄 已构建待验证（`output/inotia4-export-modded-v0.3.1.apk`），仅在需要免 root 部署时集成 |
| **验收交付** | 双产物 + API 文档 | 待开始 |

> 详细进度：会话交接时由 AI 生成临时快照；踩坑经验见 `docs/environment.md` §5a；决策记录见各主题文档。

## 交付物

| 交付物 | 说明 | 部署目标 | 状态 |
|---|---|---|---|
| **导出模块 APK** | Xposed 模块，Hook 游戏 + 提供 REST API | 手机（LSPosed） | ✅ v0.3.8（`output/inotia4-export-module-v0.3.8.apk`） |
| **集成版 APK（modded.apk）** | LSPatch 集成模块+游戏，免 root 单文件 | 按需集成（免 root 部署时） | 🔄 已构建待验证（`output/inotia4-export-modded-v0.3.1.apk`） |
| **静态数据 JSON 数据库** | 解析的数值表/配置/资源 | 两者共用 | ✅ 完成（`static-data/json/`，22MB） |
| API 文档 | 接口清单、参数、示例 | 两者共用 | ✅ 完成（`docs/api-spec.md`） |

## 项目文件结构

```
projects/android-game-api-export/
├── apk/                                    # 【输入物】原始 APK + 解码/反编译中间产物
├── tools/                                  # 【工具】第三方工具（LSPatch/NDK 等，项目内隔离）
├── scripts/                                # 【工作区】开发期脚本（analyze/parse/touch_automation）
├── module/                                 # 【交付·源码】Xposed 导出模块 Gradle 工程
├── static-data/                            # 【交付·数据】静态数据 JSON 数据库（结构见 docs/reference/static-data.md）
├── output/                                 # 【交付·二进制】构建产物 APK（版本见「交付物」表）
├── docs/                                   # 【交付·文档】见下方「文档地图」
├── log/                                    # 运行日志
├── archive/                                # 【归档】探索研究中间产物（frida 探查脚本/反汇编/截图等，不入库）
├── .gradle/                                # 可选构建缓存隔离
└── .tmp/                                   # 临时文件（可随时清空）
```

> 目录明细与构建产物见 `docs/environment.md`；静态数据结构见 `docs/reference/static-data.md`。

## 文档地图（文档结构）

> **分级阅读策略（强制）**：
> - **第一级（最高级）**：**无论什么任务都必读**。新会话/任何操作前先读。
> - **第二级（领域级）**：相关领域的**汇总、概览、整体性要求**——只要任务涉及该领域就必须读，涉及领域即读，无需细分到具体内容。
> - **第三级（细分级）**：细分方向的**探索笔记、专门记录**——只有任务涉及具体相关内容才读。
>
> **禁止重复探索、重复尝试**：文档已包含的操作步骤、结论与踩坑经验，不确定信息归属时，先查下表定位到对应文档再动手。

| 文档 | 主题 | 分级 | 权威性 |
|---|---|---|---|
| **本文件 README.md** | 项目总览、需求、交付物、目录规范 | **第一级** | 总览 |
| **docs/architecture.md** | 模块代码结构 + 规范（分层/常量管理/迁移/新增端点流程） | **第一级** | **代码唯一权威** |
| **docs/api-spec.md** | REST API 规格（数据模型/端点/状态/事件流） | 第二级 | API 权威 |
| **docs/environment.md** | 开发环境与工具链（依赖清单/关键命令/踩坑记录） | 第二级 | 环境权威 |
| docs/reference/hook-points.md | 逆向数据源细节（符号/VMA/结构体偏移/操作函数语义） | 第二级 | 溯源 |
| docs/reference/game-systems.md | 游戏系统总览（19 系统/静态表/动态数据清单） | 第二级 | 参考 |
| docs/operations/player-operations.md | 操作分级（合法 vs OP）+ 实现状态 | 第二级 | 参考 |
| docs/operations/control-capability.md | 写操作函数签名（调用机制/签名表） | 第二级 | 参考 |
| **docs/backlog.md** | 开发待办总清单（唯一待办来源） | 第二级 | 待办权威 |
| docs/verification.md | 全量一致性核查（文档↔代码↔产物↔行为） | 第二级 | 核查清单 |
| docs/reference/ui-click-coordinates.md | 已探索的 UI 点击坐标（界面+元素+坐标+截图） | 第三级 | 参考 |
| docs/reference/static-data.md | M3 静态数据解析（game_res 格式/工具链/字段目录） | 第三级 | 溯源 |
| docs/deployment/emulator-research.md | 模拟器/转译层调研结论 | 第三级 | 决策记录 |
| docs/deployment/phone-dev-workflow.md | 真机开发/部署/联调工作流 | 第三级 | 流程 |

> 职责划分原则：**每个文档只有一个主题，互不重复**。结构/规范以 architecture.md 为唯一权威，API 以 api-spec.md 为准，其余均为补充细节。
> 进度快照不落盘：会话交接时由 AI 当场生成临时快照（完成进度/思考过程等未入档信息），不维护 HANDOFF 文档。

## 目录规范与环境隔离（强制）

> **原则**：本项目的所有文件、中间产物、临时文件、构建产物与最终交付物，一律存放于本目录
> `projects/android-game-api-export/` 内。**工具的输出文件（解码产物、反编译输出、构建产物、生成的 APK、
> 解析结果、日志、截图等）必须落在项目文件夹内**，禁止写到项目文件夹之外。

### 内容归属表

| 内容 | 存放位置 | 说明 |
|---|---|---|
| 工具输出文件 | 项目内对应目录（`apk/decoded`、`output/`、`static-data/`、`.tmp/`） | **输出文件必须落在项目文件夹内** |
| Python 虚拟环境 | `.venv/`（uv 管理） | 禁止向系统 Python 安装项目依赖 |
| 构建产物 APK | `output/` | 最终交付物 |
| 分析脚本/笔记 | `scripts/`、`docs/reference/`、`docs/operations/`、`docs/deployment/` | 项目内 |

### 具体规则

1. **工具输出（必须项目内）**：凡工具会产出文件——解码产物（apktool）、反编译输出（jadx）、构建产物（Gradle/Android 构建）、生成的 APK、解析出的 JSON、日志与截图——输出路径必须落在项目文件夹内（`apk/decoded/`、`output/`、`static-data/`、`.tmp/` 等），禁止散落到系统目录或项目外 `/tmp`
2. **Python**：项目依赖一律用项目内 `.venv/`（`uv run`），禁止向系统 Python 安装项目依赖；系统 pacman 的 python-frida 不用于本项目
3. **构建与交付**：Gradle 中间产物在 `module/**/build/`，最终 APK 复制到 `output/` 后验收交付；如需更干净的构建隔离，可用 `GRADLE_USER_HOME=$PWD/.gradle`（可选强化，非强制）
4. **临时文件**：统一放 `.tmp/`，可随时清空
5. **只读环境依赖**：Android SDK（`/opt/android-sdk`）属运行环境，仅引用，项目文件不写入其中
6. **版本提交（强制）**：每次递增版本号并成功构建出一个新版本 APK 后，必须将代码变更提交到 git；提交信息注明版本号与变更摘要（如 `feat(v0.2.21): 新增 xxx`）。新版本只有代码已提交后才算完成

## 注意事项

- **sudo 由用户执行**（一次性命令模式；用户拒绝长期免密 sudoers）
- 语言：中文交流；错误直接给出「原因+修复」
- **大文件下载可委托用户**（模拟器/工具镜像如 QEMU 用户自装），子代理下载大文件可委托

---
*文档创建日期：2026-08-05，最后更新：2026-08-07（README 精简为总览；文档按类别归档至 docs/reference|operations|deployment；新增「操作前置」强制要求）*
