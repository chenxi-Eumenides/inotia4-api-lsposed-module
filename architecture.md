# 模块架构与代码规范

> 日期：2026-08-13 ｜ 状态：✅ 现行（v0.5.0 API 7 域分组） ｜ **本文件是代码结构与规范的唯一权威来源**
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
│    └─ ApiServer.kt      AndServer 嵌入式 HTTP（config.json 配置地址/端口，默认 0.0.0.0:8088） │
│          ├─ service/ApiServices.kt  服务注册中心（v0.4.0 重构）│
│          ├─ service/InfoApiService(.kt 接口) + InfoApiServiceImpl │
│          ├─ service/ActionApiService(.kt 接口) + ActionApiServiceImpl │
│          ├─ util/JsonUtil / ControllerGuard   通用工具 + 守卫   │
│          ├─ controller/HealthController.kt     /api/system/health     │
│          ├─ controller/CurrentMapController.kt /api/world/map         │
│          ├─ controller/PartyController.kt      /api/character/party   │
│          ├─ controller/MercenaryController.kt  /api/character/mercenary │
│          ├─ controller/InventoryController.kt  /api/item/inventory    │
│          ├─ controller/QuestController.kt      /api/quest             │
│          ├─ controller/UiController.kt         /api/ui                │
│          ├─ controller/GameController.kt       /api/system/game       │
│          ├─ controller/EventsController.kt     /api/system/events     │
│          ├─ controller/DataController.kt       /api/world/maps/* + /api/system/tables|text|story-events │
│          ├─ controller/MovementController.kt   /api/world/movement/*  │
│          ├─ controller/CombatController.kt     /api/character/combat/* │
│          ├─ controller/InventoryActionController.kt /api/item/inventory/* │
│          ├─ controller/CharacterController.kt  /api/character/grow/* + /api/op/* │
│          ├─ controller/PartyActionController.kt /api/character/party/* │
│          ├─ controller/UiActionController.kt   /api/ui/*              │
│          ├─ controller/NpcController.kt        /api/ui/dialog/*       │
│          ├─ controller/ShopController.kt       /api/item/shop/*       │
│          ├─ controller/QuestActionController.kt /api/quest/*          │
│          ├─ controller/SaveController.kt       /api/system/save/*     │
│          └─ StaticData.kt   assets 静态数据读取              │
│                                                              │
│  调用链：HTTP controller（路由+参数解析）→ ApiServices 接口 →  │
│          ServiceImpl（业务编排）→ NativeBridge → game_data    │
│  多通道预留：ApiServices 接口不绑定 HTTP，未来 Binder/         │
│          LocalSocket 调用方复用同一 Service 层               │
└──────────────────────────────────────────────────────────────┘
```

## 2. native 层文件职责

| 文件 | 职责 | 依赖 |
|---|---|---|
| `game_symbols.h` | **常量单一来源**：结构体偏移、VMA、函数签名。含逆向来源注释 | 无 |
| `game_access.h/cpp` | `/proc/self/maps` 基址定位 + `resolve_global()` 符号解析 + `bridge_init()` | game_symbols.h |
| `game_data.h/cpp` | JSON 构造：member_json / party / inventory / player / map / units / ui / skills / mercenaries / path / init_report + **写操作 op_*（v0.3.0）** + **合法操作 op_*（v0.3.1：move/use-item/discard/include/exclude）** + **events 快照差异检测（v0.3.0）** + **FrameTaskManager 通用帧任务管理器（v0.4.26，§2.1：move/walk 逐帧驱动）** + **惰性/预取混合缓存层（v0.4.59，§2.3：interval>0 预取 / interval=0 惰性）** | game_access.h |
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

### 2.1 FrameTaskManager（通用帧任务管理器，v0.4.26）

**位置**：`game_data.cpp` 匿名 namespace（~L1397 起）。**动机**：需按游戏帧率逐帧驱动的操作（移动/自动战斗/跟随）；同步循环（单次 API 调用内走完全程）导致画面"闪现"。hook 方案（ShadowHook/手写 inline hook）在 LSPosed 环境不可行（见 §2.2）。

**设计**：

```cpp
struct FrameTask {
    bool (*fn)(void*);  // 任务回调：返回 true 继续，false 完成（自动移除）
    void* ctx;          // 任务上下文（角色指针/方向/剩余帧等，任务自定义）
    int id;
};
std::mutex g_task_mtx;            // register/unregister（API 线程）vs 遍历（任务线程）
std::vector<FrameTask> g_tasks;
std::thread g_task_thread;        // 单后台线程
std::atomic<bool> g_task_stop{false};
```

**核心函数**：

| 函数 | 语义 |
|---|---|
| `frame_task_register(fn, ctx)` | **单任务语义**：注册即 clear 旧任务再插入（与游戏"当前操作"一致）；返回任务 id（0=失败：fn 空或非游戏内） |
| `frame_task_unregister(id)` | id<=0 清全部；否则按 id 移除 |
| `stop_all_tasks()` | 置 stop 标志 → join 线程 → 清列表（walk_stop 端点调用） |
| `task_thread_fn()` | 59ms（≈16.9fps，游戏帧率 ~20fps 实测）循环：快照任务列表 → 逐回调调用 → 返回 false 的 unregister |

**现有任务**（game_data.cpp）：

| 任务 | 回调 | ctx | 终止条件 |
|---|---|---|---|
| move（寻路） | `move_task_tick` | 角色指针 | PATHLIST 空 / MoveAsPath 失败 / map_link_check 命中出口切图 |
| walk（方向键） | `walk_task_tick` | `WalkCtx{ch,dir,remaining}` | 60 帧走完 / CHAR_Move 返回非 0（撞墙）/ 切图 |

⚠️ CHAR_Move 返回值语义：**0=正常走一步（成功），非 0=撞墙/阻挡**（反汇编 e98dc `mov w20,#0x1`，v0.4.26 修复）。

**扩展新逐帧操作**（如自动战斗）：写 `bool xxx_task_tick(void* ctx)` 回调（ctx 自定义结构）+ `frame_task_register(xxx_task_tick, &ctx)`——零线程样板。

**线程安全**：任务线程每帧（帧计数变化）调回调（v0.4.57 起帧驱动，此前 59ms 定时），回调直接读写游戏内存（与游戏主循环并发）。玩家控制态下 CHAR_Process 不驱动玩家移动 → 无双驱动竞争（MoveAsPath 前临时清零 C_CTRL_STATE 0x2e2）。新增任务须评估竞争风险。

### 2.2 帧驱动方案演进（为什么不用 hook）

| 方案 | 结果 | 原因 |
|---|---|---|
| **FrameTaskManager 帧计数驱动** | ✅ 采用（v0.4.57 起） | task_thread_fn 轮询 `data_frame_count()`，帧号变化即执行回调——与游戏主循环精确同步，不随实际帧率漂移；此前 59ms 定时（v0.4.26-0.4.56）误差可接受但非严格帧对齐 |
| ShadowHook 1.0.10 | ❌ | LSPosed 环境 stub→new_addr 映射表在错误 linker 命名空间查找，桥跳野地址（0x79299114e4 访问违例） |
| 手写 arm64 inline hook | ❌ | 已修 5 bug（adrp 掩码 0x9F000000、imm21 重组 immhi<<2\|immlo、stp 编码 0xa9b0、blr 数据槽、.S 符号冲突）仍 SIGBUS/SIGILL 崩溃（trampoline lr 污染、Draw 后续指令寄存器依赖） |
| 填 PATHLIST 游戏自驱动 | ❌ | 玩家控制态（0x2e2=7）下游戏每帧重置玩家动作，驱动条件复杂（0x2fa/0xc40/0x2e0 耦合） |

**arm64 inline hook 技术教训**（后续若再尝试）：
- 所有函数入口第 1 条几乎都是 adrp（PC 相对）→ 重放必须重定位
- trampoline 必须保存/恢复原始 lr（blr 污染 lr → 重放区 stp x29,x30 存错 lr → 原函数 ret 跳错）
- AGP 对 .S 汇编不支持 -fPIC 符号重定位（ldr literal/adr 均报错）→ 需纯 C++ mmap 生成指令
- LSPosed 环境下 .S 全局符号跨 TU 引用解析到 base.apk 错误地址

### 2.3 帧同步采集缓存层（v0.4.57）

**位置**：`game_data.cpp`（build_snapshot_json 之后）。**动机**：高频请求时每次实时读游戏内存 + 构造 JSON（units 含 BFS）→ 响应慢/线程爆炸；且请求线程碰游戏内存与主循环竞争。

**设计**（表驱动，改频率只动一行）：

```cpp
CacheSlot g_cache_slots[] = {
    {"player",      1, 0, "", build_player_json},     // interval=帧数，1=每帧
    {"party",       1, 0, "", build_party_json},
    {"map",         1, 0, "", build_map_json},
    {"units",       1, 0, "", build_units_json},
    {"gamestate",   1, 0, "", build_gamestate_json},
    {"snapshot",    1, 0, "", build_snapshot_json},
    {"inventory",   5, 0, "", build_inventory_json},  // 5=每5帧（~250ms）
    {"skills",      5, 0, "", build_skills_json},
    {"mercenaries", 5, 0, "", build_mercenaries_json},
};
```

**核心机制（v0.4.59 惰性/预取双模式）**：
- **表驱动 interval 语义**：`interval>0` = 每 n 帧预取（预取线程主动构造）；`interval=0` = 惰性（请求驱动）——**改一行即切换模式**
- **预取槽（interval>0，如 player/party/map/units/gamestate/snapshot=1）**：预取线程帧计数驱动每 n 帧构造，请求直接读缓存（µs 级命中，无等帧）
- **惰性槽（interval=0，如 inventory/skills/mercenaries）**：请求驱动——同帧复用（本帧已构造 → 返回缓存），跨帧 → 单飞等帧边界构造（帧号变化 = Draw 完成，数据稳定窗口 28-55ms）
- **refreshing 单飞**：`std::atomic<bool> refreshing` CAS 保证同一槽同时只有一个线程构造；构造期间并发请求直接返回旧缓存（不等帧）——避免请求率 > 帧率时全部排队等帧
- **等帧锁外**：等帧边界在 `g_cache_mtx` 外执行，不阻塞其他请求的锁竞争
- **预取线程启动条件**：`frame_cache_start()` 仅在存在 interval>0 槽时启动线程（全惰性配置 = 零线程零空闲负担）
- **写操作强制刷新**：`op_ok()` 内调 `frame_cache_force_refresh()`——同步刷新全部槽（不等帧边界），操作后 attach 立即读最新
- **events**：`data_events_json` 直接 `take_snapshot()` + `g_events_mtx` 锁保护 diff（审计 H4）
- **帧暂停兜底**：`wait_frame_boundary` 100ms 超时后直接构造（读内存与帧无关，帧暂停仍可获取数据）

**JNI 接口不变**：gamebridge.cpp 仍调 `data_*_json()`，Kotlin 层仅 Service 组装（v0.4.58 currentMap 的 unitsJson 去重）。

**性能实测**（真机2，2026-08-12 v0.4.59）：预取槽锁外构造修复后 party 单端点 500 并发 **337.5 req/s**（与 v0.4.57 持平）；纯预取 5 端点 267.8 req/s（units BFS 重构造成本）、纯惰性 2 端点 365.4 req/s（轻量端点）；混合 7 端点 182.4 req/s（端点构成差异，非机制问题）。**吞吐差异主因是端点构造成本（units BFS）而非缓存机制**。v0.4.57（纯预取）338 req/s 最高但空闲负担大；**v0.4.59 双模式 = 高频槽预取（快）+ 低频槽惰性（省），可按需配置**。

## 3. Kotlin 层文件职责

| 文件 | 职责 |
|---|---|
| `HookMain.kt` | 模块入口；核心原则=读内存+调游戏函数为主，hook 仅必要时使用：轮询 `bridge_init()` 直至成功 → 反射拿 context → 启动 ApiServer |
| `NativeBridge.kt` | JNI 声明（`System.loadLibrary("gamebridge")` + external 方法） |
| `ApiServer.kt` | AndServer 启动（监听地址/端口读 ModuleConfig（外部 config.json）、模块 assets 注入、StaticData 挂接） |
| `ModuleConfig.kt` | **配置组件（v0.5.17，v0.5.21 改外部源）**：外部存储 config.json 为唯一配置来源（缺失用默认值并立即写入），提供监听地址/端口/堆叠上限增加/宝石批量合成等配置的获取与修改（每次修改立即持久化） |
| `service/ApiServices.kt` | **服务注册中心（v0.4.0，P0-3 重构）**：controller/调用层从这里取 Service 实例；多调用通道预留（Binder/LocalSocket 复用同一 Service 层） |
| `service/InfoApiService.kt` | **信息查询服务接口（v0.4.0）**：GET /api/info/* 全部信息端点契约，返回结构化 JSON，不绑定 HTTP 语义 |
| `service/InfoApiServiceImpl.kt` | **信息查询服务实现（v0.4.0，迁移自 InfoService）**：从 native 复合 JSON 提取简单端点字段，名称注入（物品名/属性名）统一在此 |
| `service/ActionApiService.kt` | **合法操作服务接口（v0.4.0）**：POST /api/action/* 全部操作端点契约 |
| `service/ActionApiServiceImpl.kt` | **合法操作服务实现（v0.4.0，迁移自 PlayerController 操作编排）**：操作调用 + 快照 attach（attachPlayer/attachParty 等）+ equip-by-category 查找 |
| `util/JsonUtil.kt` | 通用 JSON 工具（解析容错 + 错误响应构造 NOT_FOUND/NOT_READY/BAD_REQUEST） |
| `util/ControllerGuard.kt` | controller 公共守卫：native 未就绪返回 503 语义串（architecture §9.3-9） |
| `controller/HealthController.kt` | **/api/system/health**（服务健康，v0.5.0 由 /api/health 迁移） |
| `controller/CurrentMapController.kt` | **/api/world/map/***（id/exits/units/enemies/interactives/drops/distance 独立端点，v0.5.0 由 /api/info/map 迁移，v0.5.35 移除复合端点） |
| `controller/PartyController.kt` | **/api/character/party**（复合 + count/leader/{1..3} + 槽内子端点，v0.5.0 由 /api/info/party 迁移） |
| `controller/MercenaryController.kt` | **/api/character/mercenary**（复合 + list/{1..18}，v0.5.0 由 /api/info/mercenary 迁移） |
| `controller/InventoryController.kt` | **/api/item/inventory**（复合 + money/items/bag/*，v0.5.0 由 /api/info/inventory 迁移） |
| `controller/QuestController.kt` | **/api/quest**（复合 + active/list/list/{id}/completed，v0.5.0 由 /api/info/quest 迁移） |
| `controller/UiController.kt` | **/api/ui**（复合 + screen/panel/dialog/*，v0.5.0 由 /api/info/ui 迁移） |
| `controller/GameController.kt` | **/api/system/game + /api/system/snapshot**（复合 + snapshot/info/frame；v0.5.0 由 /api/info/game 迁移，v0.5.18 修正 snapshot 独立路径） |
| `controller/EventsController.kt` | **/api/system/events**（事件流，since 参数预留，v0.5.0 由 /api/info/events 迁移） |
| `controller/DataController.kt` | 静态数据端点（/api/world/maps/list、maps/{mapId}；/api/system/tables、tables/{table}、tables/{table}/search、text、story-events，v0.5.0 由 /api/data/* 迁移） |
| `controller/MovementController.kt` | **移动操作（POST /api/world/movement/*，v0.5.0 迁移）**：move/walk/path/stop/interact |
| `controller/CombatController.kt` | **战斗操作（POST /api/character/combat/*，v0.5.0 迁移）**：{role}/config/auto-attack、{role}/switch 等 |
| `controller/InventoryActionController.kt` | **背包操作（POST /api/item/inventory/*，v0.5.0 迁移）**：use-item、discard、{role}/equip（含 category）、{role}/unequip 等 |
| `controller/CharacterController.kt` | **角色成长（POST /api/character/grow/*，v0.5.0 迁移）**：skill、{role}/stat 等；**OP 端点（POST /api/op/character/*、/api/op/inventory/add）** |
| `controller/PartyActionController.kt` | **队伍操作（POST /api/character/party/*，v0.5.0 迁移）**：include、exclude、discharge、withdraw |
| `controller/UiActionController.kt` | **UI 操作（POST /api/ui/*，v0.5.0 迁移）**：dialog/ok、dialog/cancel、main-menu、panel/* |
| `controller/NpcController.kt` | **对话操作（✅ v0.4.27 统一三端点）**：POST /api/ui/dialog/interact、GET /api/ui/dialog/content、POST /api/ui/dialog/select（v0.5.0 归入 ui 域） |
| `controller/ShopController.kt` | **商店（/api/item/shop/*，v0.5.0 归入 item 域）**：GET items + POST buy |
| `controller/QuestActionController.kt` | **任务操作（POST /api/quest/quit，v0.5.0 归入 quest 域）** |
| `controller/SaveController.kt`       | **存档操作（/api/system/save/*，v0.5.0 由 info/action 迁移归并）**：slots 读 + save/enter-slot/create 写；load 待实现 |
| `controller/ConfigController.kt`     | **模块配置（GET /api/config/list + POST /api/config/set，v0.5.21）**：读当前配置 + 设置配置（每次修改立即持久化外部 config.json；监听地址/端口变化时延迟重启 ApiServer 生效；stackLimitIncrease/jewelBatchMix 变化时通知 native 生效；纯 Kotlin 层，不走 ControllerGuard） |
| `controller/DebugController.kt` | 调试端点（/api/debug/ui、/api/debug/path，开发期） |
| `StaticData.kt` | assets 静态数据读取（内存缓存） |
| `LogFile.kt` | 文件日志（/sdcard/Android/data/<游戏包>/files/inotia4-export.log） |

### 约定
- **controller 只做路由 + 参数解析 + 调用 Service**，业务逻辑在 Service 层（InfoApiServiceImpl/ActionApiServiceImpl）或 native 或 StaticData
- **调用层（controller）不直接调 NativeBridge**——统一经 ApiServices 接口（多调用通道预留，v0.4.0）
- **简单端点字段提取统一走 InfoApiService**（v0.3.13：从 native 复合 JSON 提取，controller 不直接解析）
- **静态数据读取统一走 `StaticData`**，controller 不得直接操作 assets
- **API 按实体领域分组（v0.5.0）**：7 域顶层路径 = `character`（角色/佣兵/战斗/成长）、`world`（地图/移动）、`item`（背包/商店）、`quest`（任务）、`ui`（界面/对话）、`system`（健康/游戏/事件/存档/静态表）、`op`（越权操作，独立权限）；**每组内 GET（读）与 POST（写）混合，读写同域，HTTP 方法区分**；废弃 info/data/action 前缀
- 新增端点归组：按实体归入对应域 controller；OP 类端点一律 `/api/op/*` 前缀（安全边界，见 §9.1）
- 新增端点遵循「controller 路由 → ApiServices 接口 → ServiceImpl 实现 → NativeBridge external → native JNI」五段式
- **路由一律全路径写法（v0.5.18 强制）**：controller 类上**禁用 `@RequestMapping`**，方法级 `@GetMapping/@PostMapping` 直接写与 api-reference.md 完全一致的完整路径（如 `/api/character/party/{slot}`、`/api/system/snapshot`）。原因：AndServer 注解处理器对「类级 + 方法级」路径是**纯字符串拼接**（`pPath + cPath`，无「方法级以 `/` 开头即覆盖类级」的语义），类级前缀 + 方法级全路径会拼出重复路径——v0.5.18 前 snapshot 曾注册为 `/api/system/game/api/system/snapshot` 导致文档路径 404。另注意 `/{slot}` 纯模糊首段校验失败

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
4. 按实体归组选 controller 加路由（新增端点按「分组总览」表归入对应域 controller；OP 端点 → `/api/op/*` 前缀；简单端点可走 `service/InfoService` 提取）
5. `docs/api-spec.md` §4 更新端点表（含版本号）
6. 构建 → 真机验证（`scripts/analyze/live_session.py` 采样）

> **写操作端点（v0.3.0）**：额外步骤——先 objdump 逆向函数签名（见 `docs/research/control-capability.md` §5 方法），
> game_symbols.h 加 F_*_VMA + typedef，game_access 解析函数指针，game_data 实现 `data_op_*`（内部检查
> `STATE_nState==5` 并返回 `{"ok":..}` JSON），再走三段式。

### 写操作端点通用规范（v0.3.2-0.3.6 真机验证沉淀）

0. **`STATE_nState==5`（world）前置检查 = 简化假设，非硬性要求**（2026-08-08 修正）：统一要求操作时游戏处于 world 状态，
   是为**减少开发难度与测试广度**——避免在主菜单/存档界面调用游戏函数因数据结构未就绪崩溃。**未经逐操作实证**；
   如需支持非 world 状态操作，可去除该检查并在多种界面状态下实测验证（见 docs/research/control-capability.md §0）。

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
| `docs/research/data-sources.md` | 逆向数据源细节（偏移/VMA 依据/操作函数语义） | 常量溯源引用 game_symbols.h |
| `docs/research/control-capability.md` | 写操作函数签名/调用机制 | 新增写端点时引用 |
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
