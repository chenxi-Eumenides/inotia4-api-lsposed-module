# 模块架构与代码规范

> 日期：2026-08-05 ｜ 状态：✅ 现行 ｜ **本文件是代码结构与规范的唯一权威来源**
> 其他文档（README/HANDOFF/hook-points）中的结构描述应引用本文件，不再重复维护。

## 1. 模块架构总览

```
┌──────────────────────────────────────────────────────────────┐
│  游戏进程（LSPosed 注入，libxposed 101）                       │
│                                                              │
│  HookMain.kt       模块入口：onModuleLoaded → 轮询初始化      │
│    │                                                         │
│    ├─ NativeBridge.kt   JNI 桥（声明 external + loadLibrary） │
│    │     │                                                    │
│    │     ▼                                                    │
│    │  libgamebridge.so（native 层）                            │
│    │  ├─ gamebridge.cpp    JNI 薄层（导出 Java_* 函数）        │
│    │  ├─ game_access.h/cpp 基址定位 + 符号解析（/proc/self/maps│
│    │  ├─ game_data.h/cpp   JSON 数据构造                      │
│    │  └─ game_symbols.h    常量单一来源（VMA + 结构体偏移）    │
│    │                                                          │
│    └─ ApiServer.kt      AndServer 嵌入式 HTTP（0.0.0.0:8088） │
│          ├─ controller/InfoController.kt     信息获取（GET）  │
│          ├─ controller/PlayerController.kt   玩家操作（POST） │
│          ├─ controller/DataController.kt     静态数据端点     │
│          └─ StaticData.kt   assets 静态数据读取              │
│                                                              │
│  数据流：libgame.so（游戏数据）→ base+VMA 直读 → game_data    │
│          JSON 构造 → NativeBridge → Controller → HTTP 客户端  │
└──────────────────────────────────────────────────────────────┘
```

## 2. native 层文件职责

| 文件 | 职责 | 依赖 |
|---|---|---|
| `game_symbols.h` | **常量单一来源**：结构体偏移、VMA、函数签名。含逆向来源注释 | 无 |
| `game_access.h/cpp` | `/proc/self/maps` 基址定位 + `resolve_global()` 符号解析 + `bridge_init()` | game_symbols.h |
| `game_data.h/cpp` | JSON 构造：member_json / party / inventory / player / map / units / ui / skills / mercenaries / path / init_report + **写操作 op_*（v0.3.0）** + **合法操作 op_*（v0.3.1：move/use-item/discard/sell/include/exclude）** + **events 快照差异检测（v0.3.0）** | game_access.h |
| `gamebridge.cpp` | **JNI 薄层**：仅参数传递 + 字符串转换，无业务逻辑 | game_access.h |

### 关键约定
- **禁止在 game_data.cpp / gamebridge.cpp 中出现裸偏移或裸地址**——一律通过 `game_symbols.h` 常量（`C_*`/`I_*`/`O_*`/`S_*`/`M_*`/`G_*_VMA`/`F_*_VMA`）
- **结构体访问一律用偏移常量 + 注释**，禁止 magic number
- **JNI 函数名 = `Java_<包>_<类>_<方法名>`**，Kotlin `external fun` 方法名须与导出名精确对应（曾因缺 `native` 前缀导致 UnsatisfiedLinkError，见 HANDOFF 踩坑）
- native 层不抛异常给 Java：失败返回 `-1`/空值，由 Kotlin 层容错
- **带参 JNI**：`nativeGetPathJson(tx, ty)` 等参数经 JNI `jint` 传递（v0.2.33 起）

## 3. Kotlin 层文件职责

| 文件 | 职责 |
|---|---|
| `HookMain.kt` | 模块入口；零 hook 方案：轮询 `bridge_init()` 直至成功 → 反射拿 context → 启动 ApiServer |
| `NativeBridge.kt` | JNI 声明（`System.loadLibrary("gamebridge")` + external 方法） |
| `ApiServer.kt` | AndServer 启动（端口 8088、模块 assets 注入、StaticData 挂接） |
| `controller/InfoController.kt` | **信息获取端点（GET，只读）**：/api/info/player、/party、/inventory、/map、/quest、/units、/ui、/player/skills、/player/mercenaries、/path、/events + Kotlin 后处理（withItemNames/withAttrNames 注入物品名与属性名） |
| `controller/PlayerController.kt` | **玩家操作端点（POST，/api/action/*，v0.3.1）**：合法操作 = 玩家游戏内可做的事——money(add/minus)、move、use-item、equip、unequip、auto-attack、skill、switch、inventory/discard、inventory/sell、party/include、party/exclude、teleport。OP 操作走未来 /api/op/*，不在此 |
| `controller/DataController.kt` | 静态数据端点（/api/data/*，映射静态表 JSON） |
| `StaticData.kt` | assets 静态数据读取（内存缓存） |
| `LogFile.kt` | 文件日志（/sdcard/Android/data/<游戏包>/files/inotia4-export.log） |

### 约定
- **controller 只做路由 + 数据透传**，业务逻辑在 native 或 StaticData
- **静态数据读取统一走 `StaticData`**，controller 不得直接操作 assets
- **GET（信息获取）与 POST（操作）分层**：InfoController 放只读信息（/api/*）；PlayerController 放玩家操作（/api/action/*）；DataController 放静态数据（/api/data/*）；未来 OP 操作独立 OpController（/api/op/*）
- 新增端点遵循「controller 方法 → NativeBridge external → native JNI」三段式

## 4. 常量与符号管理（换版本核心）

**`game_symbols.h` 是全部游戏版本相关常量的唯一位置**。校验工具：

```bash
uv run python scripts/analyze/check_symbols.py [libgame.so 路径]
```

- 解析 `game_symbols.h` 的 `G_*_VMA`/`F_*_VMA` 常量（`SYMBOL_TO_MACRO` 映射表）
- 与新 libgame.so 符号表对比，输出 `✅ 一致 / ⚠️ 需更新`
- **改动 game_symbols.h 后必须运行此校验**

### 换游戏版本迁移流程
1. 新 APK 解包 → 提取 `lib/arm64-v8a/libgame.so`
2. `uv run python scripts/analyze/check_symbols.py <新so>` → 列出变化的 VMA
3. 更新 `game_symbols.h` 的 VMA；结构体偏移（`C_*`/`I_*`）按 check 输出人工判断是否变化
4. 重新提取静态数据（M3 流程，`scripts/parse/` 脚本）
5. 构建 v0.3.x 并真机验证

## 5. 命名约定

- native 文件：`game_*` 前缀（symbols/access/data）+ `gamebridge`（JNI 边界，库名绑定）
- Kotlin：类名明确职责（HookMain/NativeBridge/ApiServer/StaticData/LogFile）
- **避免语义重叠命名**（曾出现 `game_bridge` 与 `gamebridge` 混淆，已改名 `game_access`）

## 6. 新增端点标准流程

1. native `game_data` 加 JSON 构造函数（用 symbols.h 常量）
2. `gamebridge.cpp` 加 JNI 导出（名与 Kotlin external 精确一致）
3. `NativeBridge.kt` 加 external 声明
4. `InfoController`（信息）/ `PlayerController`（操作）/ `DataController`（静态）按类型加路由
5. `docs/api-spec.md` §4 更新端点表（含版本号）
6. 构建 → 真机验证（`scripts/analyze/live_session.py` 采样）

> **写操作端点（v0.3.0）**：额外步骤——先 objdump 逆向函数签名（见 `docs/notes/control-capability.md` §5 方法），
> game_symbols.h 加 F_*_VMA + typedef，game_access 解析函数指针，game_data 实现 `data_op_*`（内部检查
> `STATE_nState==5` 并返回 `{"ok":..}` JSON），再走三段式。

## 7. 构建与校验命令

```bash
# 构建（module/ 目录）
GRADLE_USER_HOME=$PWD/../.gradle \
  ../.gradle/wrapper/dists/gradle-8.11.1-bin/*/gradle-8.11.1/bin/gradle \
  :app:assembleDebug --no-daemon

# 产物复制到 output/ + 版本号递增（build.gradle.kts 的 versionCode/versionName）

# 符号校验（改 game_symbols.h 后必跑）
uv run python scripts/analyze/check_symbols.py

# 真机采样（局域网）
uv run python scripts/analyze/live_session.py
```

## 8. 文档地图（避免重复）

| 文档 | 职责 | 与本文档关系 |
|---|---|---|
| **本文件 architecture.md** | 代码结构 + 规范（唯一权威） | — |
| `README.md` | 项目总览、目录规范、里程碑 | 结构概览指向本文档 |
| `docs/HANDOFF.md` | 会话交接（进度/决策/踩坑） | 结构结论指向本文档 |
| `docs/api-spec.md` | API 规格（端点/数据模型/状态） | 与结构无关 |
| `docs/notes/hook-points.md` | 逆向数据源细节（偏移/VMA 依据） | 常量溯源引用 game_symbols.h |
| `docs/notes/static-data.md` | M3 静态数据交付说明 | 与结构无关 |
