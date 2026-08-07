# 模块架构与代码规范

> 日期：2026-08-05 ｜ 状态：✅ 现行 ｜ **本文件是代码结构与规范的唯一权威来源**
> 其他文档（README/data-sources）中的结构描述应引用本文件，不再重复维护。

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
│          ├─ service/InfoService.kt   简单端点字段提取 + 名称注入 │
│          ├─ util/JsonUtil / ControllerGuard   通用工具 + 守卫   │
│          ├─ controller/HealthController.kt     /api/health     │
│          ├─ controller/CurrentMapController.kt /api/info/current-map │
│          ├─ controller/PartyController.kt      /api/info/party │
│          ├─ controller/MercenaryController.kt  /api/info/mercenary │
│          ├─ controller/InventoryController.kt  /api/info/inventory │
│          ├─ controller/QuestController.kt      /api/info/quest │
│          ├─ controller/UiController.kt         /api/info/ui    │
│          ├─ controller/GameController.kt       /api/info/game  │
│          ├─ controller/EventsController.kt     /api/info/events│
│          ├─ controller/DataController.kt       /api/data/*     │
│          ├─ controller/PlayerController.kt     /api/action/*   │
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
| `game_data.h/cpp` | JSON 构造：member_json / party / inventory / player / map / units / ui / skills / mercenaries / path / init_report + **写操作 op_*（v0.3.0）** + **合法操作 op_*（v0.3.1：move/use-item/discard/include/exclude）** + **events 快照差异检测（v0.3.0）** | game_access.h |
| `gamebridge.cpp` | **JNI 薄层**：仅参数传递 + 字符串转换，无业务逻辑 | game_access.h |

### 关键约定
- **核心实现原则（最高优先级）**：**以读写内存、调用游戏函数为主，hook 只在必要时使用**。
  - 读内存：读符号 VMA 直读（`g_base + VMA` 解引用），覆盖玩家/背包/地图/面板/弹窗文本等存量数据
  - 调游戏函数：函数指针调用（`fn_search_path`/`fn_move_as_path`/`fn_get_member` 等，符号解析后直接执行，不修改游戏代码），覆盖 move/equip/use-item 等操作端点（v0.3.1）
  - 写内存：直接修改游戏状态（如 `*ctrl = 0` 清零控制态），覆盖需要改状态的操作
  - hook（拦截函数/改参数）**默认不用**：修改执行流有崩溃面，且与"数据导出"定位不符；仅在确有拦截需求且读内存/调函数无法实现时才引入，引入前须书面说明理由
- **禁止在 game_data.cpp / gamebridge.cpp 中出现裸偏移或裸地址**——一律通过 `game_symbols.h` 常量（`C_*`/`I_*`/`O_*`/`S_*`/`M_*`/`G_*_VMA`/`F_*_VMA`）
- **结构体访问一律用偏移常量 + 注释**，禁止 magic number
- **JNI 函数名 = `Java_<包>_<类>_<方法名>`**，Kotlin `external fun` 方法名须与导出名精确对应（曾因缺 `native` 前缀导致 UnsatisfiedLinkError，见 `docs/environment.md` §5a 踩坑）
- native 层不抛异常给 Java：失败返回 `-1`/空值，由 Kotlin 层容错
- **带参 JNI**：`nativeGetPathJson(tx, ty)` 等参数经 JNI `jint` 传递（v0.2.33 起）

## 3. Kotlin 层文件职责

| 文件 | 职责 |
|---|---|
| `HookMain.kt` | 模块入口；核心原则=读内存+调游戏函数为主，hook 仅必要时使用：轮询 `bridge_init()` 直至成功 → 反射拿 context → 启动 ApiServer |
| `NativeBridge.kt` | JNI 声明（`System.loadLibrary("gamebridge")` + external 方法） |
| `ApiServer.kt` | AndServer 启动（端口 8088、模块 assets 注入、StaticData 挂接） |
| `service/InfoService.kt` | **信息提取层（v0.3.13 新增）**：从 native 复合 JSON 提取简单端点字段（api-spec §0 新分层），名称注入（物品名/属性名）统一在此；controller 只做路由 |
| `util/JsonUtil.kt` | 通用 JSON 工具（解析容错 + 错误响应构造 NOT_FOUND/NOT_READY/BAD_REQUEST） |
| `util/ControllerGuard.kt` | controller 公共守卫：native 未就绪返回 503 语义串（architecture §9.3-9） |
| `controller/HealthController.kt` | **/api/health**（服务健康，v0.3.13） |
| `controller/CurrentMapController.kt` | **/api/info/current-map**（复合 + id/tile/units/enemies/interactives/drops，v0.3.13） |
| `controller/PartyController.kt` | **/api/info/party**（复合 + count/leader/{1..3} + 槽内子端点，v0.3.13） |
| `controller/MercenaryController.kt` | **/api/info/mercenary**（复合 + list/{1..18}，v0.3.13） |
| `controller/InventoryController.kt` | **/api/info/inventory**（复合 + money/items/bag/*，v0.3.13） |
| `controller/QuestController.kt` | **/api/info/quest**（复合 + active/list/list/{id}/completed，v0.3.13） |
| `controller/UiController.kt` | **/api/info/ui**（复合 + screen/panel/dialog/*，v0.3.13） |
| `controller/GameController.kt` | **/api/info/game**（复合 + snapshot/info，v0.3.13） |
| `controller/EventsController.kt` | **/api/info/events**（事件流，since 参数预留，v0.3.13） |
| `controller/DataController.kt` | 静态数据端点（/api/data/map/list、map/{mapId}、list、{table}、{table}/search、text、events，v0.3.13 重构） |
| `controller/PlayerController.kt` | **玩家操作端点（POST，/api/action/*）**：move、use-item、equip、unequip、auto-attack、skill、switch、inventory/discard、party/include、party/exclude、dialog/ok、dialog/cancel、**get-path（v0.3.13 迁移自 /info/path）**。OP 操作走未来 /api/op/*，不在此 |
| `controller/DebugController.kt` | 调试端点（/api/debug/ui，开发期） |
| `StaticData.kt` | assets 静态数据读取（内存缓存） |
| `LogFile.kt` | 文件日志（/sdcard/Android/data/<游戏包>/files/inotia4-export.log） |

### 约定
- **controller 只做路由 + 数据透传**，业务逻辑在 native 或 StaticData 或 InfoService
- **简单端点字段提取统一走 InfoService**（v0.3.13：从 native 复合 JSON 提取，controller 不直接解析）
- **静态数据读取统一走 `StaticData`**，controller 不得直接操作 assets
- **GET（信息获取）与 POST（操作）分层**：InfoController 已拆分为按系统 controller（v0.3.13）；PlayerController 放玩家操作（/api/action/*）；DataController 放静态数据（/api/data/*）；未来 OP 操作独立 OpController（/api/op/*）
- 新增端点遵循「controller 方法 → NativeBridge external → native JNI」三段式（简单端点可走 InfoService 提取）
- **AndServer 方法级路径必须首段静态**（处理器约束：`/{slot}` 纯模糊首段校验失败，写全路径如 `/api/info/party/{slot}`）

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

> **写操作端点（v0.3.0）**：额外步骤——先 objdump 逆向函数签名（见 `docs/control-capability.md` §5 方法），
> game_symbols.h 加 F_*_VMA + typedef，game_access 解析函数指针，game_data 实现 `data_op_*`（内部检查
> `STATE_nState==5` 并返回 `{"ok":..}` JSON），再走三段式。

### 写操作端点通用规范（v0.3.2-0.3.6 真机验证沉淀）

1. **调游戏函数前先查判定函数**（`*_IsUse` / `*_IsRealEquip` / `*_CanUse` / `*_CanEquip`）——游戏对「能否做某事」通常有现成判定函数，先搜符号表，别自己猜。判定不过返回明确错误（如 `item not usable`），避免触发游戏内部非预期 UI/状态（乱码弹窗）。
2. **返回值不等于成功标志**：ARM64 tail-call（`b` 非 `bl`）会覆盖返回值语义；返回 -1 的函数走 truthy 判断会误判为成功。用**状态观察**（操作后槽位空/坐标变/装备变化）判定生效，或单独处理 -1。
3. **目标槽/位置被占用时先清理**：如 `CHAR_EquipItem` 目标槽被占用返回 0——先 `fn_unequip` 再穿，实现自动替换。
4. **前置校验拦截非法操作**：游戏内「合法操作」对主控/特殊 NPC 会走 UI 弹窗（乱码）——API 层前置校验拦截（`cannot exclude leader` / `cannot exclude quest npc`），绝不直接调游戏函数。
5. **Kotlin 方法名避开 Java 保留字**（switch/object/class 等）：kapt/ksp 注解处理器按 Java 标识符生成代码，保留字方法被**静默跳过**（无报错、无路由）。排查「代码有注解但运行 404」：解包 APK 查 dex 路由字符串。

## 7. 构建与校验命令

> 构建/部署/真机循环命令见 `docs/environment.md` §3.1，本节仅列代码规范相关的校验命令。

```bash
# 符号校验（改 game_symbols.h 后必跑）
uv run python scripts/analyze/check_symbols.py
```

## 8. 文档地图（避免重复）

> 完整文档地图（三级分级）见 `README.md`「文档地图」，本节仅列与代码结构直接相关的文档。

| 文档 | 职责 | 与本文档关系 |
|---|---|---|
| `README.md` | 项目总览、文档地图（三级分级） | 结构概览指向本文档 |
| `docs/data-sources.md` | 逆向数据源细节（偏移/VMA 依据/操作函数语义） | 常量溯源引用 game_symbols.h |
| `docs/control-capability.md` | 写操作函数签名/调用机制 | 新增写端点时引用 |
| `docs/player-operations.md` | 操作分级（合法 vs OP） | 新增操作端点时引用 |

## 9. API 安全与并发基线（强制，2026-08-08 代码审计新增）

> 来源：2026-08-08 全量代码审计。以下为**后续所有编码必须遵守的持续性规范**；
> 审计发现的一次性修复项在 `docs/backlog.md`「代码审计修复项」跟踪，不在本节重复。
> 本节是 §2/§4/§6 的补充维度（鉴权/并发/错误语义/符号登记），不替代既有约定。

### 9.1 暴露面与鉴权

1. **HTTP 端点必须经过鉴权**（token / IP 白名单中间件），**写操作（POST）强制**；鉴权机制未就绪前，禁止新增对外端点。
2. **OP 能力（改数据类 native 函数：money/exp/statuspoint/teleport/sell 等）必须带全局开关且默认关闭**。`/api/op/*` 权限机制就绪前：禁止新增 OP 路由、禁止在 controller 中直接调用 OP 函数——仅靠「不挂路由」隔离不构成安全边界。
3. **调试端点（/api/debug/*）必须登记文档**（architecture + api-spec），release 构建排除或加鉴权。

### 9.2 并发

4. **模块自身共享状态必须线程安全**：缓存用 `ConcurrentHashMap`（禁止裸 `HashMap` 跨请求读写）；跨请求状态（如 events 基线快照）用 `std::mutex`/`atomic` 保护，禁止函数内 `static` 可变状态无锁访问。
5. **写操作（op_*）必须全局互斥串行化**；操作与结果快照读取必须在同一临界区完成（避免 read-modify-write 竞态）。
6. native 全局状态即便「初始化后只读」，初始化窗口的读写仍须同步（读加锁或原子门控）。

### 9.3 错误响应与防御

7. **错误响应统一 JSON 包装 + HTTP 状态码语义**（400 参数错 / 404 未找到 / 500 内部错 / 503 未就绪），**禁止透传 native 失败原串**（"-1"/空值）给客户端。
8. **native 函数指针调用一律判空后再调**（与 `fn_get_next_exp` 事故对齐）；`NewStringUTF` 返回值须判 null/异常。
9. 所有 HTTP 调用 native 前检查 `NativeBridge.ready`，未就绪返回 503（防止 `UnsatisfiedLinkError` 冒泡成 500）。

### 9.4 符号与常量（补充 §4）

10. **新逆向出的 VMA/偏移一律入 game_symbols.h 并登记 `check_symbols.py` 校验清单**；禁止在 game_data/gamebridge 出现裸地址（含面板识别地址、调试字段地址）。
11. **调用游戏函数一律走 game_access 解析层（fn_*）**，禁止 `g_base + VMA` 直接强转调用（dialog_ok/cancel 的反例须收敛）。
