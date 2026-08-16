# 代码重构实施方案（四层架构）

> 状态：v1.1（2026-08-16，经 grilling 评审修订：修复 6 处缺陷 + 4 项用户裁决落地）
> 探索基线：v0.5.43（commit 4c105ee，output/inotia4-export-module-v0.5.43.apk）
> 所有「文件:行号」均按 v0.5.43 实测，实施时以当时代码为准（行号可能漂移，符号名不变）。
>
> **v1.1 变更**：① P0 新增 smoke 脚本交付物（G1：原验收基线依赖不存在的工具）；② 修复 §3.1 迁移表 5 个漏映射函数，story_next/skip 改归 dialog 域消除自造违规（G2）；③ 新增 P2 重复消解——data 层 bag/pool 遍历原语替换 10 处拷贝（G3：蓝图核心诉求原被静默丢弃）；④ P3 新增 host 单测 target（G4 + 用户裁决）；⑤ HTTP 状态码统一移入 P1（G5/D5 用户裁决：P1 顺带做）；⑥ gameInfo 占位字段整项做完（G6 用户裁决）；⑦ D4 用户裁决推翻默认：**OP 门禁 + 已备端点接线**进入 P1。

## 一、探索结论与蓝图勘误

全量探索（Kotlin 层 / native 层 / 死代码与文档滞后三路并行）已完成，v0.1 蓝图的核心判断全部成立，数据有多处修正：

| 蓝图 v0.1 论断 | 实测结果 | 修正 |
|---|---|---|
| Kotlin 35 文件 3441 行 | **38 文件 3441 行**（23 controller + 4 根组件 + 2 patch + 4 service + 1 StaticData + 3 util） | 蓝图漏计 patch/ 与 CorsInterceptor 等 |
| native 20 文件 6608 行 | **31 文件 6608 行**（14 .cpp + 17 .h） | 蓝图漏计 game_patch/game_ptr_hook/symbol_* 等 |
| controller 直调 NativeBridge 3 处文件 | 确认：DebugController 2 处裸调（:16/:20 无 guard）、CharacterController 6 处（:62/70/78/88/115/132）、ConfigController 3 处（:34-39）；另 ApiServer 启动期 3 处 | — |
| 手写错误串约 30 处 | **62 处**（controller 内联 48 + 常量 7 + service/LogFile 7），两套格式并存确认 | 数量翻倍 |
| guard 逐端点 88 次 | 88 次调用 / 87 端点（equip_item 双分支），31 端点无 guard（Data 11 静态、Op 15 占位、Debug 2、Config 2、Combat 1 占位） | — |
| 背包 6×16 遍历 4 份 | **7 处同构**（state:82-90 / state:97-103 / ops_action:41-48 / ops_action:1038-1040 / ops_action:1051-1053 / ops_value:157-165 / patch:180-189）+ read:229-244 stride 风格 | 数量上修 |
| game_in_world 约 70 处 | 74 处调用（ops_action 36 / ops_value 16 / misc 8 / read 5 / cache 4 / patch 3 / motion 1 / state 定义 1） | — |
| 死代码 questListId | **误判**：`/api/quest/{id}` 路由在用（QuestController:26），是恒返 NOT_FOUND 的占位 stub，非死代码 | 处置为决策项 D1（已裁决：保留） |
| 死代码 npcDialogNext/Select、nativeOpDialogOk/Cancel | 半死：Kotlin 三层 + JNI 无调用者可删；**native 函数 data_op_npc_dialog_select / data_op_dialog_ok / data_op_dialog_cancel 存活**（被 data_op_dialog_select 分发器 :652-728 调用），必须保留 | 删 Kotlin/JNI 层，保 native 函数 |

**探索新发现（蓝图未列，纳入本方案）**：

1. **13 个 native OP 函数已实现但无 Kotlin 接线**（nativeOpSetMoney/AddMoney/MinusMoney/AddExperience/SetStatusPoint/PartySwap/Teleport/RemoveItem/DialogOk/DialogCancel/RecoverAfterHiveBlock/SetSkillUsage/QuestList）——其中 4 个与 OpController 定稿端点直接对应（money/status-point/party-swap/teleport），**用户裁决 D4：随重构接线**（先建 OP 门禁，见 P1-v0.5.47）。
2. **文档记录了 3 个不存在的路由**：`/api/ui/dialog/ok`、`/api/ui/dialog/cancel`（architecture.md:198 引用）、`GET /api/ui/dialog/content`（api-reference.md:1246 等 4 处引用）——实际能力由 `/api/ui/dialog/select` + action 参数承担（D2 已裁决：修文档）。
3. **architecture.md 未覆盖的代码实体**：patch/（IapBlocker/ImmersiveMode）、game_patch.cpp、game_ptr_hook.h、CorsInterceptor.kt。
4. **声明/实现错位**：game_ops_action.h:38-46 声明的 4 个函数（data_save_slots_json 等）实现全在 game_misc.cpp。
5. game_data.h:104 存在两条声明挤一行的格式损坏——双头维护踩坑实证。
6. ControllerGuard 把业务异常吞成 NOT_READY（语义错误：异常≠未就绪）。
7. **smoke 基础设施缺失**（grilling G1）：scripts/analyze/ 无全量路由探测脚本（live_session.py 仅采样 5 处、api_poll.py 仅轮询），118 路由全量验收工具需先构建。
8. **story_next/story_skip（ops_action:1222/1228）被 dialog_select 分发器调用**（:680/:691）——归属必须与分发器同域，否则制造新的域间反向依赖（grilling G2，v1.0 曾错分 game_system）。
9. craft 三函数（data_craft_btn_inject/remove/set_enabled，:1350/:1420/:1270）与 set_stack_limit_enabled 同属「配置触发的 UI/行为注入」，机制上走 game_ptr_hook/mmap（grilling G2：v1.0 漏映射，归 game_patch）。

## 二、目标架构（四层定义与依赖规则）

```
┌─ api 层（Kotlin）    controller/        路由绑定 + 参数解析 + 转发；不碰 NativeBridge
├─ service 层（Kotlin）service/           语义修正、名称注入、跨域聚合、快照、OP 编排（含 OP 门禁）、配置下发
├─ parse 层（native）  域文件 + 引擎       原始字段→JSON 构造；写操作直改内存；输出缓存/帧驱动
└─ data 层（native）   符号/访问/查询原语  符号解析、偏移常量、读原始字段、静态元数据缓存、跨域遍历原语
```

### 2.1 各层职责（继承蓝图，细化到 native）

| 层 | native/Kotlin | 职责 | 禁止 |
|---|---|---|---|
| data | game_symbols / symbol_registry / symbol_resolver / game_access / game_state / game_tiles | 符号解析（VMA/ELF）、偏移常量、`resolve_global`、跨域实体查询原语（member_or_null / lead_member / find_inventory_item / inventory_count / inventory_item_at / find_char_by_merc_slot）、**跨域遍历原语（for_each_bag_slot / pool_obj_valid，G3 新增）**、瓦片静态缓存、状态判定（game_in_world / ui_blocked / tutorial_*） | 构造业务 JSON；依赖 parse 层任何头 |
| parse | 域文件（character/party/inventory/world/quest/ui/dialog/shop/save/system）+ 引擎（json/nav/cache/motion/ops_common） | build_*/data_* 读构造、data_op_* 写操作、op_ok/op_err、BFS、帧缓存、FrameTask | 注入游戏语义名称；域间水平互调（仅允许 system 聚合方向向下） |
| service | Kotlin service/ | inject* 名称注入、快照 attach、OP 编排（LogFile.op + **OP 门禁**）、配置下发 native | 解析 HTTP 参数；直接读内存 |
| api | Kotlin controller/ | 路由、参数校验、错误包装（含 HTTP 状态码） | 调 NativeBridge |

### 2.2 依赖方向强制规则（可用 include 检查脚本验证）

```
gamebridge（JNI 顶）→ 所有层
parse 域文件 → data 层 + parse 引擎 + system 聚合方向的域头
parse 引擎 → data 层（cache 额外引用域文件的 build_* 函数指针，同层无环，合法）
data 层 → 仅 STL
禁止：data → parse、域文件之间非聚合方向互调、任何 include 环
```

### 2.3 缓存落位（继承蓝图不变）

- data 层：符号地址、结构偏移、瓦片矩阵（初始化一次常驻）
- service 层：StaticData 物品名/词缀/任务表（懒加载常驻）
- parse 层：`g_cache_slots` 输出缓存（按帧号失效，表不变）
- 游戏状态任何层不缓存

### 2.4 不合并三套帧机制（D6，grilling G5 显式化）

`wait_frame_boundary`（请求驱动等帧）、`cache_prefetch_thread_fn`（后台预取）、`task_thread_fn`（FrameTask 帧调度）三套机制**保持独立**：分别服务惰性请求、预取线程、逐帧任务三个不同场景，语义与锁边界各异，合并=高风险重设计，收益仅是行数——不做。op_ok 内 `frame_cache_force_refresh` 属「写后刷新」第四种模式，同样保留。

## 三、目标文件结构

### 3.1 native 层迁移映射

**现状 → 目标**（14 个 .cpp → 21 个 .cpp）：

| 目标文件 | 层 | 来源（函数级迁移） | 预估行数 |
|---|---|---|---|
| game_symbols.h / symbol_registry.h / symbol_resolver.* | data | 不动 | 533+242+257 |
| game_access.* | data | 不动，仅删 game_data.h include（:2）与 frame_cache_start 调用（:368，移至 gamebridge nativeInit 尾部） | 379+163 |
| game_state.* | data | 瘦身为「状态判定 + 跨域查询原语 + 遍历原语」：保留 game_in_world/ui_blocked/tutorial_*、member_or_null、find_inventory_item/inventory_count/inventory_item_at；**并入 lead_member（自 game_read:173）与 find_char_by_merc_slot（自 game_read:661）**；**新增 for_each_bag_slot / pool_obj_valid（G3）**；移出 op_ok/op_err | ~170 |
| game_tiles.* | data | 不动 | 124+28 |
| game_json.* | parse 引擎 | 不动（P0 删 json_append_int 后仅剩 json_escape） | 50+8 |
| game_nav.* | parse 引擎 | 不动；lead_member 改自 game_state 取（合法 parse→data） | 194+30 |
| game_cache.* | parse 引擎 | 不动（12 槽表、slot 索引、函数指针全冻结）；game_in_world 改经 game_state.h（合法 parse→data） | 192+20 |
| game_motion.* | parse 引擎 | 不动（FrameTaskManager） | 99+10 |
| **game_ops_common.*（新）** | parse 引擎 | op_ok/op_err（自 game_state:67-74，连带 frame_cache_force_refresh 调用合法化）+ 写操作跨域共享 helper | ~60 |
| **game_character.*（新）** | parse 域 | read(member_json/build_player/build_skills/build_mercenaries + 属性追加 helper) + ops_value(set_experience/set_level/add_experience/set_status_point/set_hp/set_mp/set_attr/add_stat/stat_reset/skill_reset/learn_action/set_auto_attack/set_skill_usage) + ops_action(cast/attack/stop_combat 战斗段 1192-1220) | ~450 |
| **game_party.*（新）** | parse 域 | read(build_party) + ops_action(include/exclude/discharge/withdraw/switch_player/party_swap，队伍段 1146-1191 + 角色 312-327) | ~150 |
| **game_inventory.*（新）** | parse 域 | read(build_inventory/item_is_equip/append_item_attrs) + ops_value(set_money/add_money/minus_money/add_item/remove_item) + ops_action(use_item/discard_item/sell_item/move_item/dice_accept/dice_reject/equip/unequip/jewel/enchant + inventory_gained_json，装备段 729-830 + 物品段 974-1145) | ~500 |
| **game_world.*（新）** | parse 域 | read(build_map/build_tiles/build_units_json_impl/build_enemies/build_interactives/build_drops/append_position) + misc(path/distance/debug_path) + ops_action(move/walk/walk_stop/interact/teleport 移动段 858-937 + nav_task_tick/walk_task_tick) | ~450 |
| **game_quest.*（新）** | parse 域 | misc(data_active_quest/quest_list/quest_completed/quest_active) + ops_action(quest_quit 358-372) | ~130 |
| **game_ui.*（新）** | parse 域 | misc(debug_ui/ui_screen/popup_top_vma/top_panel_name/story_active/story_json) + ops_action(main_menu/panel_open/panel_close 槽位/面板段子集) | ~250 |
| **game_dialog.*（新）** | parse 域 | misc(npc_dialog_options/dialog_content) + ops_action(npc_interact/dialog_select 分发器 652-728/dialog_ok/dialog_cancel/npc_dialog_select/**story_next/story_skip（G2 修正：与分发器同域，:680/:691 被其调用）**；删 npc_dialog_next) | ~380 |
| **game_shop.*（新）** | parse 域 | misc(shop_items) + ops_action(shop_buy) | ~60 |
| **game_save.*（新）** | parse 域 | misc(save_slots/current_save_slot) + ops_action(save/enter_slot/create_slot 槽位段子集) | ~120 |
| **game_system.*（新）** | parse 域 | read(build_gamestate/build_snapshot) + misc(frame_count/init_report/events/emit/take_snapshot)；**唯一允许 include 其他域头的聚合域** | ~300 |
| game_patch.* | 机制 | 保留并**扩充为「注入/修改补丁域」**：现有 IAP 屏蔽/沉浸模式/堆叠上限 + **data_op_migrate_stack（自 ops_value 迁入，堆叠迁移补丁语义）+ craft 三函数（G2：data_craft_btn_inject/remove/set_enabled，配置触发的 UI 注入，与 set_stack_limit_enabled 同机制）+ data_recover_after_hive_block（IAP 恢复语义，自 ops_action:546 迁入）** | ~420 |
| gamebridge.cpp | JNI | 不动（JNI 导出名与分发逻辑全冻结） | 495 |
| ~~game_data.h~~ | — | **删除**：域文件迁移时声明随域走，最终清空删除；P2 起标记 deprecated | 0 |

**解散文件**：game_read.cpp（663 行→character/party/inventory/world/system + state）、game_misc.cpp（785 行→world/quest/ui/dialog/shop/save/system）、game_ops_value.cpp（206 行→character/inventory + patch）、game_ops_action.cpp（1434 行→九域 + patch）、game_state.cpp 瘦身。

### 3.2 Kotlin 层调整

| 调整 | 内容 |
|---|---|
| **OP 收口** | CharacterController 6 个已实现 OP 端点（hp/mp/experience/level/set_attr/inventory-add）迁入 OpController；新增 `service/OpApiService.kt`（接口+实现，opSetAttr 批量循环逻辑从 controller 移入 impl；**含 OP 门禁检查**）；OpController 成为 `/api/op/*` 唯一入口，全部经 service |
| **OP 门禁 + 接线（D4 用户裁决）** | ModuleConfig 新增 `opEnabled`（默认 false，外部 config.json 持久化）；OpApiService 所有方法入口统一检查：未启用返回 `403 + {"ok":false,"error":"op disabled"}`；接线 4 个 native 已备端点：`/api/op/inventory/money`（nativeOpSetMoney）、`/api/op/character/{role}/status-point`（nativeOpSetStatusPoint）、`/api/op/party/swap`（nativeOpPartySwap）、`/api/op/movement/teleport`（nativeOpTeleport）；其余 11 个占位保持 NOT_IMPL；有 native 无定稿路由的（add_money/minus_money/add_exp/remove_item/set_skill_usage/recover/quest_list）不新增路由，backlog 登记。满足 architecture §9.1-2「全局开关默认关闭」后，接线不再违反安全基线 |
| **ui 域合并** | NpcController（start_interact/dialog/select 两端点）并入 UiActionController；路由路径零变化；删除 NpcController.kt |
| **Debug 收边** | DebugController 补 ControllerGuard.guard + InfoApiService 增加 debugUi/debugPath 两方法（不再裸调） |
| **Config 收边** | nativeSetStackLimitEnabled/nativeSetJewelBatchMix/nativeSetTilesData 三处直调收口到 `service/ConfigApiService.kt`（applyToNative）；ConfigController 与 ApiServer 启动期统一调用；config/list、set 增加 opEnabled |
| **错误格式 + HTTP 状态码统一（D5 用户裁决：P1 顺带）** | 格式 A `{"ok":false,"error":"..."}` 胜出（native op_ok/op_err 已产此格式、48/62 手写串已是此格式）。JsonUtil 改造：NOT_FOUND(404)/NOT_READY(503)/BAD_REQUEST(400) 常量改格式 A + `err(msg, code)` 工厂 + `parseBody(body)` 入口；替换全部 62 处手写串；删除 8 处 parseBody/BAD_BODY 私有拷贝（CharacterController:135/142、InventoryActionController:106/113、MovementController:38/45、PartyActionController:47/54、CombatController:59/66、ShopController:26/33 + QuestAction/NpcController 内联）；**路由参数解析异常（如 /api/character/party/abc 的 NumberFormatException，现状 403 泄漏原始异常串）统一捕获转 400 + JSON 错误体**。实现机制（AndServer 返回状态码的方式：HttpResponse 构造器 / 全局 ExceptionResolver / Interceptor 改写）在 P1 开工时先用 librarian 查证 AndServer API 再定，方案不限死 |
| **guard 语义修复** | ControllerGuard.guard 异常分支改返回 `err("internal error", 500)`（不再吞成 NOT_READY）；88 处调用形态保持（方法引用/lambda 两式并存合法） |
| **InfoImpl 拆助手** | InfoApiServiceImpl（710 行）抽出 `service/NameInjector.kt`（7 个 inject* + StaticData 查询 ~200 行）；接口 46 方法暂不拆分（scope 控制） |
| **gameInfo 占位字段整项完成（G6 用户裁决）** | 删 `logged_in`（:420）；`saveSlots` 空数组占位改为 `currentSlot: Int`——native 已备（nativeCurrentSaveSlot，NativeBridge:38 → data_current_save_slot_json，misc:444），Kotlin 解析接线 ~10 行；同步 api-reference/game-guide |
| **死代码删除** | filterUnits（InfoApiServiceImpl:530-537）、npcDialogNext/npcDialogSelect（ApiService.kt:91-92 + ActionApiServiceImpl:113-117）、NativeBridge 4 个 external（:86-87/:119-120）、ModuleConfig 4 个 setter（:99-113） |

## 四、分阶段实施

> 每阶段独立可构建、可部署、可验证、可回滚；完成才进入下一阶段。
> 版本与提交：按 README 规则 6/7，每阶段递增 0.0.1 并 git 提交（多步阶段可拆多个版本）。

### P0 死代码清理 + 验证基建（v0.5.44，约 1 天）

**v0.5.44a smoke 脚本先行（G1，验收基建，先于一切删除）**：新建 `scripts/analyze/smoke_all.py`——路由清单化（118 条：路径+方法+参数模板），逐条探测，断言：响应可达、无原始 Java 异常串泄漏（`java.`/`NumberFormatException`）、错误响应符合统一信封；产出基线报告（重构前快照，供 P1-P3 每阶段对比）。

**Kotlin 与 native 同 commit 删除**（保持 JNI 对应）：

| 删除项 | 位置 | 保留项（勿删） |
|---|---|---|
| filterUnits | InfoApiServiceImpl:530-537 | — |
| npcDialogNext/npcDialogSelect 接口+impl | ApiService.kt:91-92、ActionApiServiceImpl:113-117 | — |
| NativeBridge 4 external | NativeBridge.kt:86-87/119-120 | — |
| 4 个 JNI 包装 | gamebridge.cpp:291-297/406-413 | — |
| data_op_npc_dialog_next | game_ops_action.cpp:636 | **data_op_npc_dialog_select（:643，dialog_select 在用）** |
| json_append_int | game_json.cpp:22 + game_json.h:8 | json_escape（12 处在用） |
| ModuleConfig 4 setter | ModuleConfig.kt:99-113 | apply()/load()/fallbackListenPortToDefault()（在用） |

**gameInfo 占位字段（G6）**：删 logged_in + saveSlots→currentSlot int 接线 + api-reference/game-guide 同步。

**验收**：smoke_all.py 基线跑通（重构前 118 路由行为快照留存）；构建通过；`/api/ui/dialog/select` 三种 action（ok/cancel/index）真机验证仍正常（覆盖被保留的 native 分发器）；`/api/system/info` 返回含 currentSlot。

### P1 Kotlin 收边（v0.5.45-47，约 1.5 天）

1. **v0.5.45 错误格式 + 状态码**：JsonUtil 改造（格式 A 常量 + err(msg,code) + parseBody()）；机械替换 62 处手写串、8 处 parseBody 拷贝；ControllerGuard 异常语义修复；PathVariable 解析异常统一 400。开工前置：librarian 查证 AndServer 设置状态码的官方机制。
   验收：controller 无 `"{\"ok\":false` 与 `"{\"error"` 字面量（grep 清零）；smoke 全量对比基线（允许错误信封/状态码差异，不允许路径/成功响应差异）；错误路径抽查（bad body→400、not found→404、not ready→503）。
2. **v0.5.46 结构收边**：OpApiService 新建 + 6 OP 端点迁移（CharacterController 仅剩 4 grow）；NpcController 并入 UiActionController；DebugController 补 guard + service；ConfigApiService 收口三处 native 直调；NameInjector 抽取。
   验收：grep `NativeBridge\.` 在 controller/ 目录清零；路由清单与 v0.5.43 完全一致（路径/方法零变化，解包 APK 核对 dex 路由数=118）；真机跑 OP 流程（set hp → attach 快照）与 ui 流程（dialog/select）。
3. **v0.5.47 OP 门禁 + 接线（D4）**：ModuleConfig.opEnabled（默认 false）；OpApiService 门禁（403 op disabled）；接线 4 端点（money/status-point/party-swap/teleport）；api-reference §8.2 四端点状态更新 + architecture §9.1-2 达成说明；backlog 登记剩余有 native 无路由项。
   验收：门禁关闭时 4 端点全部 403；开启后真机验证 set money（读回 inventory 确认）、teleport（读回 map/坐标确认）、party-swap、status-point 各一次；关闭恢复。

### P2 native 依赖治理 + 重复消解（v0.5.48，约 1 天）

**外科手术（不动函数本体）**：

| 手术 | 内容 | 消除的环/倒挂 |
|---|---|---|
| 1. op_ok/op_err 迁出 | game_state:67-74 → 新 game_ops_common.cpp/h；调用方（ops_value 16 + ops_action 40 处）改 include | 环 B：state⇄cache（state 不再 include game_cache.h） |
| 2. lead_member/find_char_by_merc_slot 下沉 | game_read:173/661 → game_state；game_nav:43 与 game_misc:198/237/267 改引 game_state.h | 环 A：read⇄nav；环 C：read⇄misc（misc→read 方向断） |
| 3. game_access 去倒挂 | 删 :2 `#include "game_data.h"`；:368 frame_cache_start() 移至 gamebridge.cpp nativeInit 成功尾部 | access⇄cache 双向断 |
| 4. 杂项 | game_read.cpp:21/24 重复 include game_nav.h 清理；game_data.h 头部加 deprecated 注释（P3 删） | — |

**重复消解（G3，蓝图核心诉求）**：

| 原语 | 位置 | 替换目标（10 处） |
|---|---|---|
| `for_each_bag_slot(cb)` | game_state（data 层） | 7 处背包 6×16 遍历：state:82-90/97-103、ops_action:41-48/1038-1040/1051-1053、ops_value:157-165、patch:180-189 |
| `pool_obj_valid(o)` | game_state（data 层） | 3 份角色池过滤：read:360-374（mode 参数化）、misc:311-321（situation 变体）、ops_action:296-308 |

> 遍历/过滤的**域特定差异**（mode 过滤、hero 排除、situation 变体）通过回调参数/谓词注入保留，原语只收编「哨兵判断 + 槽遍历」公共骨架。read:229-244 的 stride 风格 build_inventory 保留原样（结构不同，强行统一得不偿失）。

**验收**：构建通过（include 图无环）；真机全量 smoke + 帧缓存性能抽测（party 端点并发 ≥ 300 req/s 基线，见 §七）。

### P3 native 域重组 + host 单测（v0.5.49-51，约 2.5 天）

按「先解散垃圾桶、再拆巨石、最后删伪接口」三步，每步独立构建：

1. **v0.5.49 解散 game_misc.cpp**（785 行 → world/quest/ui/dialog/shop/save/system 六域新文件，函数按 §3.1 映射整函数搬迁，零逻辑改动）；修正 game_ops_action.h:38-46 错位声明（随迁至对应域头）。
2. **v0.5.50 拆分 game_ops_action.cpp**（1434 行 → 九域 + patch 域：craft 三函数与 recover_after_hive_block 迁 game_patch，story_next/skip 迁 game_dialog）；game_ops_value.cpp 随拆入 character/inventory（migrate_stack 迁 patch）。
3. **v0.5.51 拆分 game_read.cpp + 删除 game_data.h + host 单测**：build_* → character/party/inventory/world/system；每 cpp 改 include 域头；game_data.h 清空删除；CMakeLists 更新（14 → 21 cpp）；**新增 CMake host test target（G4 用户裁决）**：纯函数单测覆盖 json_escape、base64_decode、parse_int_field、nav_bfs/nav_bfs_multi（构造瓦片数组 + 阻挡用例）、tiles 解析——host 侧 `ctest` 运行，不依赖设备。

**每步验收**：构建通过；全量 118 路由真机 smoke 对比基线（重点：units BFS 构造成本、snapshot 聚合、events diff）；性能基线不回归；LSPosed 重载稳定性（重启游戏 + 重载模块 3 次无崩溃）；v0.5.51 另验收 host 单测全绿。

### P4 文档同步（不升版本，doc commit，约 0.5 天）

1. **architecture.md 重写**：四层架构 + 21 文件域结构 + 新规则（域间仅 system 聚合方向、错误格式 A + 状态码表、OpController 唯一 OP 入口 + opEnabled 门禁、JNI 面冻结约定、遍历原语规范）+ 补 patch/ 与 game_patch（含 craft/recover/migrate_stack）；修正全部滞后点（见下）。
2. **滞后修正清单**（全部实测确认）：
   - HealthController 路由：文档 `/api/system/health` → 实际 `GET /api/health`（:183、:28 结构图）
   - 接口文件名：文档 InfoApiService.kt/ActionApiService.kt 两文件 → 实际 ApiService.kt 单文件双接口（:25/:177-179）
   - FrameTaskManager 位置：文档 game_data.cpp → 实际 game_motion.cpp:23-98（game_data.cpp 已不存在，:63/:80/:105/:122）
   - move 回调名：文档 move_task_tick → 实际 nav_task_tick（:109）
   - 不存在的路由：`/api/ui/dialog/ok|cancel`（:198）、`/api/ui/dialog/content`（:199）→ 按实际（select+action、GET /api/ui/dialog）改写（D2 裁决：修文档不补实现）
   - ModuleConfig 注释矛盾：文档「实际生效逻辑未实现（预留）」→ 实际已实现且 ConfigController/ApiServer 在调（ModuleConfig.kt:21-24）
3. **api-reference.md**：错误信封 + 状态码统一说明；`/api/system/info` 字段变更（logged_in 删除、currentSlot 新增）；§8.2 四端点从占位转已实现（含 opEnabled 门禁说明）。
4. **backlog.md**：M5（错误语义）完成销项（JSON + 状态码均达成）；gameInfo 占位项销项；新增「OpController 剩余 11 占位端点 + 有 native 无定稿路由 7 项（add_money/minus_money/add_exp/remove_item/set_skill_usage/recover/quest_list）」。

## 五、API 契约影响清单

| 变更 | 影响 | 消费方兼容性 |
|---|---|---|
| 错误信封统一 `{"ok":false,"error":"..."}` + 状态码（400/404/500/503） | info/静态端点错误路径：增 `ok:false` 键；HTTP 状态码从恒 200 改为语义码；路由参数异常从 403+裸异常串改 400+JSON | 局域网消费方为自研脚本；**破坏性变更**（非 2xx 处理）已获用户裁决接受（D5） |
| `/api/system/info` 字段 | 删 `logged_in`（恒 null）、`saveSlots` 空数组 → `currentSlot: Int` | 已获用户裁决（G6） |
| `/api/op/*` 4 端点激活 | money/status-point/party-swap/teleport 从 NOT_IMPL 变可用（opEnabled=true 时） | 门禁默认关，默认行为不变（D4） |
| 其余 | **零变化**：118 条路径/方法/参数/成功响应结构全部保持 | — |

## 六、风险与缓解

| 风险 | 缓解 |
|---|---|
| JNI 名耦合（Java_<包>_<类>_<方法> 精确对应） | **JNI 面全冻结**：NativeBridge 91 个 external 仅删 6 个已验证零调用的死方法，其余名字/签名零改动；native 重组全部在 gamebridge.cpp 之下 |
| 帧缓存 slot 索引耦合（data_player_json=slot0...） | g_cache_slots 表结构与索引全冻结；仅 build_* 函数指针目标随迁移换文件 |
| 域迁移漏带 helper（跨段 static 函数） | 迁移规则：同域 helper 随域走，跨域 helper 入 game_ops_common 或 game_state；每步迁移后 grep 原文件残留符号清零再删文件 |
| AndServer 状态码机制未知（注解处理器限制） | P1 开工前置 librarian 查证；三种候选机制（HttpResponse 构造/ExceptionResolver/Interceptor）不限死，查证后择一 |
| AndServer 注解处理器坑（Java 保留字静默跳过、类级路径纯拼接） | 路由零变化策略：合并/迁移 controller 时方法级全路径注解逐字保留；迁移后解包 APK 核对 dex 路由字符串数 = 118 |
| OP 接线引入越权面 | opEnabled 默认 false + 门禁在 service 层统一入口（controller 不再持有 NativeBridge 引用，无法绕过）；真机验证开关双向 |
| 线程安全回归（迁移改变锁边界） | 零逻辑改动纪律：只搬函数不改实现；g_cache_mtx/g_task_mtx/g_events_mutex 语义不动 |
| 探索行号漂移（v0.5.43 后代码变动） | 各阶段开工前对涉及符号重跑 grep 定位；以符号名为准不以行号为准 |
| 真机验证成本 | smoke_all.py 脚本化全量回归（P0 交付）；P1/P3 额外跑性能基线 |

## 七、验证基线与回滚

- **真机**：真机2 `192.168.3.54`（当前主力，完全 API 操控）；OP 门禁真机验证含开/关双向。
- **smoke**：`scripts/analyze/smoke_all.py`（P0 交付）：118 路由全量探测（路径+方法+参数模板），断言响应可达、无 Java 异常串泄漏、错误信封统一；每次阶段验收与 v0.5.43 基线报告对比（P1 起允许错误路径格式/状态码差异，成功路径必须逐字节等价）。
- **host 单测**：v0.5.51 交付 CMake test target，`ctest` 全绿（json_escape/base64_decode/nav_bfs/tiles 解析）。
- **性能基线**（architecture.md §2.3 实测）：party 单端点 500 并发 ≥ 300 req/s；units/快照混合无异常抖动。P2/P3 复测。
- **回滚**：每阶段独立 git commit + 版本号；出问题 `git revert` 对应 commit 重构建上一版本 APK；P3 三步各自可独立回退（文件级搬迁，revert 干净）。

## 八、决策记录（v1.1 全部裁决完毕）

| # | 事项 | 裁决 | 依据 |
|---|---|---|---|
| D1 | `/api/quest/{id}` 占位 stub | **保留现状** | 用户 2026-08-16 |
| D2 | 文档中不存在的 `GET /api/ui/dialog/content` | **修文档**（不补实现） | 用户 2026-08-16 |
| D3 | guard 显式包裹 vs 全局 Interceptor | **保留显式 guard** | 用户 2026-08-16 |
| D4 | 13 个 native 已备 OP 端点 | **接线**：建 opEnabled 门禁（默认 false）+ 接 4 个有定稿路由端点，其余登记 backlog | 用户 2026-08-16（推翻默认） |
| D5 | HTTP 状态码语义统一 | **P1 顺带做**（接受消费方非 2xx 兼容性影响） | 用户 2026-08-16（grilling G5） |
| D6 | 三套等帧机制 | **不合并**（§2.4：不同场景不同锁边界，合并=高风险重设计） | grilling G5 显式化 |
| G4 | host 单测 | **P3 增加**（+0.5 天，nav_bfs/json_escape/base64_decode 等纯函数） | 用户 2026-08-16 |
| G6 | gameInfo 占位字段 | **整项完成**（删 logged_in + saveSlots→currentSlot） | 用户 2026-08-16 |

## 九、工作量与执行顺序

```
P0 死代码+smoke 基建+gameInfo   v0.5.44    1 天   （smoke 脚本先行 = 验收基建）
P1 Kotlin 收边                  v0.5.45-47 1.5 天 （错误+状态码 → 结构 → OP 门禁接线）
P2 native 依赖治理+重复消解      v0.5.48    1 天   （三环+倒挂外科手术 + bag/pool 原语）
P3 native 域重组+host 单测      v0.5.49-51 2.5 天 （misc → ops_action → read+data.h+单测）
P4 文档同步                     doc commit 0.5 天 （architecture.md 重写 + 滞后修正）
合计                                     ≈ 6.5 天
```

依赖关系：P0 的 smoke 脚本必须最先交付（后续所有验收依赖）；P1 与 P2 在 P0 后可并行（Kotlin/native 互不阻塞，但建议串行保验证清晰）；P2 必须先于 P3（环不破，域文件无法干净搬迁）；host 单测在 P3 末（域文件就位后 nav_bfs 等 include 路径才稳定）；P4 最后。

---

*探索证据存档：三份探索报告（Kotlin 层/native 层/死代码与文档滞后）+ grilling 评审判决（G1-G6，含 2 处方案自违规则修复）已全部并入本文，行号引用以 v0.5.43 为准。*
