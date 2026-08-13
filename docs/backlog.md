# 开发待办总清单

## 完成标准

- **只有真机验证通过才算完成**：实现后必须在真机（**两台真机**：真机1=`192.168.3.11`/Tailscale `100.110.139.83`，真机2=`192.168.3.54` 当前主力；UI 坐标仅适用真机1，真机2 完全 API 操控）验证行为符合预期且无崩溃。
- **验证结论写入对应主题文档**（产出类型 → 归属）：
  - 写操作函数签名/VMA/调用机制 → `docs/control-capability.md`
  - 数据结构/偏移/全局 VMA → `docs/data-sources.md`
  - 操作分级判定/实现状态 → `docs/api-technical-spec.md`
  - 端点规格（路由/参数/返回）→ `docs/api-reference.md`
  - 静态表字段语义 → `docs/reference/static-data.md`
  - UI 点击坐标 → `docs/reference/ui-click-coordinates.md`
  - 部署/模拟器结论 → `docs/deployment/emulator-research.md` 或 `docs/deployment/phone-dev-workflow.md`
- **未真机验证 = 未完成**，状态标为 `待真机验证` 而不是完成。
- **完成一项后删除该条**，不留历史；新缺口随时在此追加。

状态标记：未开始 / 探索中 / 实现中 / 待真机验证

---

## 代码审计修复项

### 审计修复·高优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 物品名映射错位检查（**2026-08-09 降为检查项**） | `StaticData.buildItemNames` 用 records 数组下标作 key，但查询传的是 category（ITEMDATABASE id 从 30 起）；**但本次会话实测 name 显示正常（恢复药水/低级宝石等均正确）——需核实是否已修复或复现条件**（可能 buildItemNames 与查询双方都用 category？） | 核实 StaticData.itemName(category) 查询链与 buildItemNames key 是否一致；真机验证 name 与物品一致 | 审计 H1 + 2026-08-09 检查 |
| 未开始 | HTTP 鉴权 + 网卡绑定 | ApiServer 监听 0.0.0.0:8088 零鉴权，局域网任意设备可读写（含 14 个写端点与 dialog 确认） | AndServer 加 token 中间件（Interceptor）+ `inetAddress()` 绑网卡/白名单；未带 token 返回 401/403 | 审计 H2 |
| 未开始 | OP 能力隔离机制 | native/JNI 的 money/exp/statuspoint/teleport/sell（任意定价）已实现，仅靠「不挂路由」隔离，无权限机制 | 加全局开关（默认关闭）或移除 OP native 实现；验证无任何 HTTP 路径可触发 | 审计 H3 |
| ✅ **events 快照线程安全** | `data_events_json` 局部 static `last`/`has_last` 无锁，多客户端并发轮询丢事件/数据竞争 | **✅ v0.4.57 修复**：采集线程每帧 `take_snapshot()` 更新 `g_events_snap`（锁内），`data_events_json` 读缓存 diff（`g_events_mtx` 保护 diff 过程）——并发轮询不再竞争，且基线随帧更新不丢事件 | 审计 H4 + v0.4.57 |
| 未开始 | 全代码库判空审查与修复（**2026-08-09 扩展自 fn_get_next_exp**） | 审计 H5 发现 `fn_get_next_exp` 漏判空（game_data.cpp:34）；**用户要求扩展到整个代码库**——所有函数指针调用（fn_*，194 处）/野指针读/NewStringUTF 返回值统一判空审查（含 game_data.cpp 中约 30 处疑似未判空调用点） | 系统性审查全部 fn_* 调用点与指针读，补判空；init 失败路径不崩溃 | 审计 H5 + 用户 2026-08-09 |
| ✅ 2026-08-12 v0.4.62 | 裸地址入符号表 | ~~game_data.cpp 34 个裸 VMA（debug UI 12 + 面板识别 22，含 1 个与 G_POPUP_FPCANCEL_VMA 重复），换版本静默失效~~ | **已完成**：10 个 UI 状态符号（G_UI_*/G_UI_PARTY_MENU_INDEX）readelf 名验证一致入 game_symbols.h + check_symbols.py 登记；22 个面板 enter 符号（F_PANEL_*）+ 5 个未命名面板（F_PANEL_UNK*）+ 3 个 GOT 槽（G_GAME_RESUME_FLAG/G_HUD_GATE/G_DAILY_TRIGGER）+ 3 个教学槽（G_TUTORIAL_FLAG*）全部入符号表；misc/ops_action/read/state 裸地址全部替换 | 审计 H6 |

### 审计修复·中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | StaticData 缓存并发安全 | `cache` 为普通 HashMap，多客户端并发 GET /api/system/tables/* 竞争 | 换 ConcurrentHashMap（或 computeIfAbsent 原子化） | 审计 M1 |
| 未开始 | 写操作并发互斥 | 全部 POST 端点无锁，native 锁仅初始化用；并发 move/equip 并发调游戏函数，attach 快照 read-modify-write 竞态 | 写操作全局 ReentrantLock，操作与快照读取同锁 | 审计 M2 |
| 未开始 | native 调用前置 ready 检查 | controller 直接调 external，loadLibrary 失败抛 UnsatisfiedLinkError（Error 捕不到）→ 500 | 入口统一检查 `NativeBridge.ready`，未就绪返回 503 JSON | 审计 M4 |
| 未开始 | 错误响应语义统一 | 失败全 HTTP 200 + 手写串/native 原串（"-1"）透传，无 400/404/500 | 统一 JSON 包装 + 状态码语义；native 失败值转结构化错误 | 审计 M5 |
| 未开始 | **参数错误 403 + 异常透传** | 2026-08-09 全量探测实测：`/api/character/party/abc`、`/api/item/inventory/bag/xx/info`、`/api/character/mercenary/abc` 返回 **HTTP 403 + 原始 Java 异常串**（`java.lang.NumberFormatException: For input string: "abc"`），非 JSON 包装，违反 architecture §9.3（应 400 + JSON 错误体） | 路由参数解析处捕获 NumberFormatException，统一走 JsonUtil 错误响应 | 本会话全量探测 2026-08-09 |
| 未开始 | op_* 参数校验补齐 | teleport(x/y/map 无范围)、learn_action(actionId/level 任意、无技能点校验)、sell(price 无约束)、move(x/y 无上限)、exp 截断 int32 校验策略不一致 | 统一入口边界校验（坐标/槽位/枚举/价格≥0/int32 范围）；learn_action 先读技能点 | 审计 M6/M8 |
| 未开始 | 弹窗文本安全 | `G_POPUP_TEXT` 野指针读（256B 无校验）+ 手写转义弱于 json_escape | 指针有效性校验 + 复用 json_escape | 审计 M9 |
| 未开始 | DebugController 处置 | /api/debug/ui 未登记（architecture 表与 api-reference 均无），release 无排除 | 登记文档或 release 排除/鉴权 | 审计 M12 |

### 审计修复·低优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 版本与产物同步 | README/verification 已同步至 v0.3.13/48（2026-08-08）；仍缺 output v0.3.9-0.3.13 历史产物；需按 README 规则 6 走版本闭环 | 补建历史产物（可选）；版本闭环走 README 规则 6 | 审计 L1/L2 |
| 未开始 | LogFile 清理 | init() 死代码；write 无锁 + 每次 appendText 开闭文件，并发写行交错 | 删死代码 + synchronized/缓冲 writer | 审计 L3 |
| 未开始 | HookMain 轮询容错 | context 未就绪/init 失败无限重试无终止条件；`nativeGetInitReport()` 无 try-catch 抛异常致 ApiServer 永不启动 | 最大重试/退避；initReport 包 try-catch | 审计 L4 |
| 未开始 | 输入白名单 | DataController `lang`/`tables/{name}` 直接拼路径无校验 | lang 白名单 + name 格式校验 `^[A-Z0-9]+$` | 审计 L7 |
| 未开始 | native 杂项清理 | 哨兵值 -1/0xFFFF/0/null 混用；CMakeLists 无 -Wall/-Wextra、dl 冗余；同址双常量 F_GET_EQUIP_VMA；g_inven 绕 resolve_global；g_party 死代码；json_escape 无长度上限；NewStringUTF 无异常检查；瓦片索引无列上界 | 统一哨兵约定；CMake 警告/标准；删冗余与死代码；补判空与上限 | 审计 L5/L8-L12 |

---

## P0 高优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| ✅ 2026-08-12 核心完成 | **物品数据结构逆向（品质/属性/词条/附魔/强化/宝石孔）** | ✅ **2026-08-12 核心完成**（成果 docs/systems/inventory.md §2.4 + data-sources.md 修正）：物品位域全破解（+0x08 type bit2-5=稀有度/bit6-15=类别、+0x10 bit0-7=混沌等级/bit8-15=混沌值率/bit25-31=数量、+0x19 socket bit0-3=已镶/bit4-7=总孔、+0x1A bit0=混沌/bit6-10=附魔等级/bit11-15=附魔ID）；词缀体系（ITEM_AddOptionEx 0x105ec4 节点 bit0-6=索引+bit13-15=type、MakeOption 链候选筛选/值生成、ITEMOPTINFOBASE 12B 记录、词缀名 1114-1150 全量确认）；品质（ITEMSYSTEM_GetRarity 0x10d700、ITEMGRADEBASE 15 条品级前缀）；附魔强化（ITEMENCHANTBASE 32 条 9B、卷轴类别 16-25/946/947）；宝石孔（IsJewel [28,32]、PutJewel/ApplySocket 链）；ITEMSTATICOPTBASE 1409 条固定词条。**API 侧 v0.4.64 已接入 4 项（待真机验证）**：① 词缀名注入（optionIds 索引数组 + optionNames/optionsDetailed，修复词缀名错位 + buildOptionNames 恒空）；② socket/enchant/混沌位域拆解输出（socketFilled/socketTotal/enchantId/enchantLevel/chaos/chaosLevel/chaosRate + socketInfo/enchantInfo/chaosInfo）；③ 装备路径品质前缀（equipOverride 不依赖 count）；④ 附魔名注入（enchantName：ITEMENCHANTBASE→ITEMDATABASE 卷轴名）。**剩余子项（P2 区已归并汇总）**：⑤ count 语义按类型区分（装备 100/宝石 0/材料≤99）；⑥ 稀有度档位显示（ITEMRARITYGRADEBASE +4 字节）；⑦ ITEMSTATICOPTBASE 静态词条 API 输出；⑧ 地面掉落实体结构（drops 端点前置，P2 掉落物条目）；⑨ 附魔属性对照表 xlsx（用户提供，强化/附魔校验）；⑩ 强化/镶嵌执行路径（操作侧，EnchantItem/PutJewel/ApplySocket 消耗校验）；⑪ 物品效果链 use-item（操作侧）；⑫ A3 强化/混沌机制 + B1 装备表字段（P3 暂缓） | ①②③④ 真机验证后闭环（验证结论入 api-reference）；⑤-⑪ 逐项实现；⑫ 归 P3 | 用户 2026-08-12 指定 P0 |
| ✅ 2026-08-12 | **瓦片矩阵入静态数据（逆向地图数据文件格式）** | ✅ **2026-08-12 完成**（详见 data-sources.md §3.2）：完整逆向文件格式（base layer + MAP_LoadLayer 5 u16 + sections + exit count + exits 6 bytes）；离线解析脚本 `scripts/parse/export_map_tiles.py` 产出 `static-data/json/maps/tiles.json`（416 图，2.2MB base64）；**真机 frida 验证 m31 bit3/bit6/bit7 100% 一致**（9 个 exit 坐标与 runtime/API 完全匹配，仅剩 6 cells bit5 map link area 标记差异 0.15%，不影响寻路）；**✅ v0.4.63 native 接入完成**：game_tiles.cpp 静态矩阵缓存 + nav_tiles() 静态优先（BFS/移动/current-map 全部走静态，真机寻路验证通过），assets 打包 tiles.json，JNI nativeSetTilesData 传递。**剩余（可选）**：① bit 5（link area 区域）逆向（需 MAP_SetEventAreaOn 0x112ca4 反汇编）；② 游戏在 world 状态时跑 `scripts/analyze/dump_all_map_tiles.py` 验证 416 图 | 用户 2026-08-12 指定 P0 |
| 进行中 | **完成 api-reference 全部未实现端点** | 已实现（24 个操作端点全部真机验证）：movement(move/move-cancel/walk/walk-stop ✅v0.4.1)、combat(attack/stop ✅v0.4.2 + skill-usage ✅v0.4.10 + cast ✅v0.4.12 + auto-attack/switch 早前)、inventory(sell ✅v0.4.3/move ✅v0.4.4/jewel ✅v0.4.6 + use-item/discard/equip/unequip 早前)、character(stat ✅v0.4.5/stat-reset ✅v0.4.7/skill-reset ✅v0.4.11)、party(discharge ✅v0.4.8/withdraw ✅v0.4.9 + include/exclude 早前)、npc(interact/dialog-select/dialog-next ✅v0.4.13 + GET dialog/options)、shop(buy ✅v0.4.14 + GET /info/shop/items)、quest(quit ✅v0.4.15)、save(save ✅v0.4.16)、ui(dialog-ok/cancel 早前 + main-menu ✅v0.4.17 GAMESTATE_SetState 纯 API 回主菜单)；**✅ ui panel 已完成（v0.4.32-33）**：panel/open（扫描 g_sPopupStateList 找 state id → UI_SetPopupProcessInfo(1,id) 官方流程1 Push）+ panel/close（栈顶面板匹配 → UI_SetPopupProcessInfo(3,0) 官方流程3 Pop，修复 v0.4.5 POPUPSTATE_Pop 同步崩溃），9 面板白名单真机验证（character_info/choice/inventory/mercenary/quests/settings/skills/wipeout/world_map），options/craft/shop/input_count 等需游戏内上下文的面板返回 `panel requires in-game context`；**✅ save/load 已完成（v0.4.18）**：`/api/system/save/enter-slot` 复现 SaveSlot_SlotButtonExe 链完全覆盖 load 设计目标，`/api/system/save/load` 占位端点已由 enter-slot 取代；**⛔ 剩余卡点**：craft mix(⛔合成链已逆向见 craft.md，需合成器交互验证)；OP 端点受 architecture §9.1 约束暂缓 | 按类别逐项：先探索底层函数（多数 P1/P2 依赖 UI 状态或未逆向，见 api-technical-spec 对应行）→ Service 层接线 → 真机验证 → 文档更新 | 2026-08-08 用户指定 P0 |
| ✅ **性能优化（减少计算/读取，提高 API 响应）** | 大响应端点（/api/world/maps/list、ITEMDATABASE、text、events）数据量大，响应耗时长 | **✅ v0.4.57-60 缓存层完成**（architecture §2.3）：**v0.4.57** 帧计数驱动采集；**v0.4.58** 惰性+帧边界；**v0.4.59** 双模式（interval>0 预取 / 0 惰性）；**v0.4.60 units 多目标单次 BFS**（31 次 BFS → 1 次全图遍历 + O(1) 查表，units 延迟 17.8→9.9ms）；真机并发 500 全成功。剩余：静态表端点（map/list/ITEMDATABASE/text）数据量大仍为实时读取，可后续按需缓存 | 原 P1 提升至 P0（2026-08-12），v0.4.57-60 完成 |

## P1 中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 各函数调用的前提探索 | 操作端点通用前置未系统验证 | 探索各操作的前提约束：是否任何界面都能保存？能否跳过确认弹窗直达操作（如直接退到主菜单）？——所有操作端点的通用前置 | 本会话决策 |
| 未开始 | **为所有操作添加日志** | 当前操作端点无统一操作日志，无法追溯调用链与参数 | 所有 POST 操作端点统一记录（时间/端点/参数/结果/耗时），native 层关键函数加日志 | 用户 2026-08-12 指定 P1 |
| 未开始 | **检查未归拢到 game_symbols.h 的裸 VMA** | game_data.cpp 等文件可能存在未入符号表的裸 VMA，换版本静默失效（审计 H6 同类） | 全量扫描各 cpp/h 裸地址，全部入 game_symbols.h（G_*_VMA/F_*_VMA）并登记 check_symbols.py | 用户 2026-08-12 指定 P1 |
| 未开始 | **VMA 直读改造为 dlsym，VMA 保底回退** | 当前 native 层经 /proc/self/maps 基址 + 符号 VMA 直读（不用 dlopen/dlsym）；换版本 VMA 漂移即失效 | 解析 libgame.so 符号表（dlsym/dlopen 或 ELF 解析）动态定位，已登记的 VMA 作为保底回退 | 用户 2026-08-12 指定 P1 |
| 未开始 | **info 端点主菜单下应报错（native 层检查 + API 诚实转发）** | 用户 2026-08-09 要求：current-map/party/mercenary/inventory/npc 在主菜单（非 world）下应报错并说明原因（如 `not in game`）；**报错在 native 数据函数调用时检查（与写操作 `game_in_world()` 一致）**，不在 API 端点代码中判断；API 遇到 native 错误诚实转发。现状：data_map/party/mercenaries/inventory/npc_dialog_options/_player_json 无状态检查，主菜单下返回假数据（mapId=0/x=-1/空槽位/name=null），不诚实。`data_snapshot_json` 独立实现（自读 g_state）不受影响。**用户确认（2026-08-09）：quest 复合端点随 data_player_json 一并报错** | ① native：上述 data_*_json 函数开头加 `if (!game_in_world()) return {"error":"not in game"}`；② Kotlin `InfoApiServiceImpl`：各方法解析 native 返回前检测 error 字段原样转发（不自行判断状态） | 用户 2026-08-09 告知 |
| ✅ **API 无法准确对应游戏状态** | 用户 2026-08-09 实测：界面 API 无法准确对应游戏状态——**剧情对话**（AVG 对话框）screen 显示 world、**任务框**（npc_quest 面板）识别到但内容读不到、部分弹窗（路障提示）dialogActive=false。**✅ v0.4.27 已解决剧情对话**（screen=story + story 对象）；**✅ v0.4.55 已解决 npc_quest 面板**（dialog/content 返回 {type:npc_quest, questId, state, options}）；路障提示弹窗数据源（G_POPUP_TEXT 未覆盖）留待后续 | 已完成（story+npc_quest）；路障提示弹窗数据源留待后续 | 用户 2026-08-09 告知 + v0.4.27/v0.4.55 解决 |
| 未开始 | data 大响应端点偶发 Connection reset | 2026-08-09 全量探测：`/api/world/maps/list`、`ITEMDATABASE`、`text`、`events` 大 JSON 响应偶发 `Connection reset by peer`（复测单发正常；`/api/system/tables` 小响应稳定） | 确认是否为 AndServer 大响应写超时/连接重置，必要时调大超时或分页 | 本会话全量探测 2026-08-09 |
| 未开始 | 商店物品/价格数据结构 | UIStore 商品列表/价格表（DEALSYSTEM）未逆向 | 反汇编 `DEALSYSTEM_FindSaleByID` + UIStore 初始化链 | 商店买卖前置依赖 |
| 未开始 | 释放技能 | `UISkill_SkillMainExe`/`UIPlay_ButtonSKill` 依赖 UI/快捷键状态（战斗价值最高） | 探索底层技能释放函数（CHAR 技能使用链），做 `POST /api/action/player/{role}/cast` | api-technical-spec §2.2 |
| ✅ 2026-08-12 确认 | 手动存档 `/api/save` | ~~`SAVE_Save`(0x129600) 依赖存档上下文~~ | **已完成**：v0.4.18 `/api/system/save/enter-slot` 复现 SaveSlot_SlotButtonExe 链完全覆盖 load 设计目标（backlog L64 ✅），手动存档无需独立 `/api/save` | control-capability §5.2 |

## P2 低优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 加点分配函数 | `CHAR_SetStatusPoint`(0xd9c4c) 是"设剩余点数"（OP 语义），真正"属性+1/能力点-1"分配函数未记录 | frida hook 人物属性页加点按钮抓底层调用；找到后做 `POST /api/action/player/{role}/stat`（≤剩余点校验 = 普通） | 本会话页面探索 |
| 未开始 | 佣兵遣散 | `MERCENARYSYSTEM_Release`(0x118ab4) 未逆向 | 逆向签名 + `POST /api/action/mercenary/discharge` | api-technical-spec §2.6，2026-08-08 降 P2 |
| 未开始 | 升级技能 | `CHAR_ProcessSkillBook`(0xe2488)（技能书路径）；`UISkill_ButtonUpExe` 依赖 UI | 探索升级函数与技能点校验 | api-technical-spec §2.5，2026-08-08 降 P2 |
| 未开始 | **声音/光效/语言设置操作 API** | 用户 2026-08-09 要求测试「主菜单环境设置」修改（声音/光效/语言各一次），**实测无对应操作端点**（底层已逆向，见 data-sources §2.7）：声音 `APPINFO_Set/GetSound`@0xd8538/0xd8528、音量 `APPINFO_Set/GetVolume`@0xd84e8/0xd84f0（开=6 关=0）、画质 bit2 置/清（child 2/3）、语言 `SGL_SetLanguage`@0x944f8 + UI 语言索引 `*(0x2f9000+0xf34)`（0-4 循环，0=简体中文） | 按设置项实现 `/api/action/settings/*` 或 options 类别端点（主菜单 SC_OPTION_MMENU 面板场景，不需 world） | 用户 2026-08-09 告知 |
| 未开始 | **创建新存档操作 API** | 用户 2026-08-09 确认未开发：无 new-game/创建存档端点；现只有 enter-slot（进已有存档，v0.4.18 用 SAVE_CreateSaveSlot 初始化槽区） | 探索新建存档链（SAVE_CreateSaveSlot + 角色初始创建 + 新手流程），实现 `/api/system/save/create` 或类似 | 用户 2026-08-09 告知 |
| 未开始 | **units「地图出口」×4 实为 2 出口 + 2 指示符** | 用户 2026-08-09 实测：当前地图实际只有 2 个地图出口，units 输出 4 个「地图出口」（slot1-4，status=2）——**出口两侧各有一个指示符/标识物，名称同为「地图出口」（CHAR_GetName 来源）无法区分**；enemies 端点同样包含宝箱/泉水等非敌人物件（status==2 过滤语义） | 核对 status/type 语义或地图物件类型字段，区分真出口/指示符/交互物；enemies 过滤条件是否应排除非敌人（宝箱/泉水/出口） | 本会话测试 2026-08-09 |
| 未开始 | **events 增强：被动触发事件** | 用户 2026-08-09 要求（保持现有差异检测逻辑，不做 since）：新增战斗/移动中被动触发的事件——**敌人数量变化、单位死亡、切换地图** 等（当前仅 money/inventory/move/hp/mp/level_up/exp） | 快照结构 Snapshot 增补字段（敌人数/单位死亡标志/地图 ID），diff 时输出新事件类型 | 用户 2026-08-09 告知 |
| 未开始 | **gameInfo 占位字段修正** | 用户 2026-08-09 要求：`InfoApiServiceImpl.gameInfo()` 中 `loggedIn`（null 占位）**删除**；`saveSlots`（空数组占位）改为**当前加载的存档槽位 int**（0/1/2） | 逆向「当前加载存档槽」内存位置（SAVE 链/存档上下文，data-sources §2.7 附近）；修改 gameInfo 返回 | 用户 2026-08-09 告知 |
| 未开始 | **~~attack/cast 无实际伤害~~（已证伪，2026-08-09 实测修正）** | 初判 attack/cast 无伤害，**后证伪**：持续观察（auto-attack 开启）后玩家实际击杀怪（slot8 狼、slot9 蝙蝠 hp=0）；cast actionId=80（治疗）MP 200→128 真实消耗。**正确结论**：① attack 生效需时间（LV2 普攻 + 怪移动，17s 内未见击杀是观察不足）；② cast 用基础技能（actionId 0-3，普攻类）无 MP 消耗无效果，**需用有效技能 ID**（80=治疗确认生效）；③ cast actionId=7 返回 `no target`（需目标型技能）；④ 怪死亡后 cast 无崩溃（14:32 崩溃未复现） | 无（测试已完成，结论已修正） | 本会话战斗测试 2026-08-09 修正 |
| 未开始 | **普攻 actionId 5/6/7 区别监听** | 用户 2026-08-09：实测确认 5/6/7 均为普攻（无 MP 消耗、同 -62 伤害、击杀怪），行为无法从 API 区分——需**监听游戏内点击普攻按钮时触发的 actionId**，判断三者是连击段（依次使用）还是独立技能；SKILLDESCBASE 不含 0-7（基础技能无静态名） | frida hook 普攻按钮/攻击键回调（UIPlay 攻击链），记录每次普攻调用的 actionId 序列 | 用户 2026-08-09 告知 |
| 未开始 | **掉落物数据源未探索** | 2026-08-09 **测试完成**：击杀怪（slot9 狼 hp→0，exp+4902 确认）后，`/api/info/current-map/drops` 硬编码返回 `{"drops":[]}`（InfoApiServiceImpl.currentMapDrops 占位）、units 无掉落实体、events 无掉落事件——**掉落物当前无法通过任何 API 获取**（用户确认场景实际有大量掉落物）；**2026-08-12 研究进展**：掉落生成链已确认（CHARSYSTEM_Die 0xf5418→DropItem 0xf4d30，frida 实测 3 怪全触发）；MAPITEMSYSTEM_ProcessDrop 读 *(0x2f5000+0x5d8) 链表（实测空）；RemoveItem 反汇编得实体=0x20 步长数组 +0x08=物品type，计数[实例+0x818] | 逆向掉落物/地面物品数据结构（怪死亡掉落生成链 + 场景掉落实体），实现 drops 端点；卡点：掉落实体场景存储位置（不在 MAPITEMSYSTEM 链表/CHARSYSTEM 池/CHARLOC 池，疑走 EFFECTSYSTEM_ProcessDropItem 0xf828c）（归 P0 总条目「物品数据结构逆向」⑧） | 本会话测试 2026-08-09 |
| 进行中 | **物品数据结构子项（归 P0 总条目「物品数据结构逆向」）** | 核心已逆向（inventory.md §2.4）；**v0.4.64 已实现待真机验证**：① 词缀名注入（词缀名错位 + buildOptionNames 恒空修复，输出 optionNames/optionsDetailed[{id,name,value}]）；② socket/enchant/混沌位域拆解输出（socketInfo/enchantInfo/chaosInfo）；③ 装备路径品质前缀注入（equipOverride）；④ 附魔名注入；socket 位域语义已破解（bit0-3=已镶/bit4-7=总孔）。**剩余未开始**：count 语义按类型区分（装备 100=无数量/宝石 0/材料真实数量≤99）、稀有度档位显示（ITEMRARITYGRADEBASE +4）、ITEMSTATICOPTBASE 静态词条输出 | 真机验证 v0.4.64 接入 + 逐子项实现，全部完成后 P0 总条目闭环 | 2026-08-12 归总（原 2026-08-09 用户需求 + 2026-08-12 探索发现合并） |
| 未开始 | **switch 切换主控未生效** | 2026-08-09 实测（存档0 LV27）：`combat/2/switch` 返回 `ok:true` 但 **mainMercenarySlot 仍=0、leader 仍=凯恩**——切换未生效；`combat/9/switch` 返回 `bad slot`（边界正确）。api-reference 声称 v0.3.2 修复 switch 路由注册（switchPlayer），但实测无效 | 排查 switch 端点实现（nativeOpSwitch → data_op_switch → fn 切换主控链）：可能调用了错误函数/未写 mainMercenarySlot/需要先决条件（如死亡角色不可切） | 本会话测试 2026-08-09 |
| 未开始 | **attack 端点仅设目标不造成伤害** | 2026-08-09 实测确认（存档0 LV27，用户怀疑成立）：`combat/0/attack {"targetSlot":20}` 后**持续 10 秒不停止，目标怪 hp 恒 4095 无伤害**（仅怪物信息条出现=目标锁定）；对照 `cast 5`（普攻）立即 -403。api-reference 声称 v0.4.2 "CHAR_SetTarget+CHAR_MakeDefaultAttack" 攻击，实测 MakeDefaultAttack 未触发实际攻击帧（仅让 AI 决策默认攻击）。**结论：普攻必须用 cast 5，attack 端点不产生伤害** | 排查 CHAR_MakeDefaultAttack 调用条件（是否需战斗态/距离/AI 决策链）；或 attack 端点改为 cast 5 同款普攻逻辑 | 本会话测试 2026-08-09 |
| 未开始 | **inventory bag/{i}/{slot} 单格查询错位** | 2026-08-09 实测：`bag/0/12` 返回 slot14 中级宝石（实际 bag0 slot12 是低级宝石）。根因：InfoApiServiceImpl.bagSlot 用 `items.optJSONObject(slot)` **按数组下标取**，但 native items 数组只含非空物品（跳过空格）→ 数组下标 ≠ 实际格号，错位。**方案已定（2026-08-13）**：按 `it.optInt("slot")==slot` 匹配，空槽返回 null，不跳过空格 | 实现按 slot 字段匹配（api-reference §4.1 已更新） | 本会话测试 2026-08-09 + 2026-08-13 定案 |
| 未开始 | **attack/cast 端点行为确认（不改实现）** | 用户 2026-08-09 决策：**attack 端点不改**（保持「仅锁定目标」语义，实际攻击链 = attack 锁定 → cast 5 普攻）；**cast 保持现状**（设置动作由 AI 帧执行，有效），仅完善校验。**新发现**：cast 5（普攻）使用后**若不 stop，角色会自动继续攻击**（AI 自动连击）——「普攻一下」需 cast 5 后立即 stop | attack 不改；cast 完善校验（如 actionId 白名单/MP 校验）；文档记录「attack=锁定、cast5=普攻、不停止=自动连击」 | 用户 2026-08-09 决策 |
| ✅ 已修复 | **use-item 仅消耗物品未实现效果** | ~~用户 2026-08-09 确认：`use-item`（`fn_consume_item`）只消耗不产生效果~~ | **已修复**：`data_op_use_item` 按类别四路径分派——骰子（STATUSDICE）/解封（ReleaseSealed）/开箱（OpenItemBox）/常规物品（CHAR_UseItemEx 效果链，内部成功自动消耗）；常规物品走效果链产生实际效果（药水回血/卷轴等），冷却中返回 `on cooldown` 不消耗 | 本会话测试 2026-08-09 + 代码确认 2026-08-13 |
| 未开始 | **character/skill 加点不消耗技能点** | 2026-08-09 实测（存档1 LV2）：`character/skill {"actionId":80,"level":2}`（80=凯恩第一个真技能，治疗）成功升级但 **skillPoints 恒 1 未消耗**——与 api-reference 声称「学习技能消耗技能点」矛盾（可能改版机制/未校验） | 核对 character/skill 实现是否校验/扣减技能点；与改版技能点机制对齐 | 本会话测试 2026-08-09 |
| 未开始 | **技能语义澄清：actionId 0-7=普攻非技能，技能按职业静态表** | 用户 2026-08-09 澄清：**0-7 号是普攻/基础动作（非技能，同 5/6/7 普攻），80 才是凯恩第一个技能（治疗）**；角色技能列表应从**职业（CHARCLASSBASE 等）**获取静态数据推导，而非仅读技能链表 actionId。此前测试把 0 号当「第一个技能」加点有误（0 号非技能） | API 技能端点（party/{slot}/skills）输出时按职业静态表区分真技能与基础动作；文档修正 skills 语义 | 用户 2026-08-09 告知 |
| 未开始 | **skill-reset 未完全还原（基础技能等级保留）** | 2026-08-09 实测：skill-reset 后 0 号技能仍 LV2（未还原到 LV1），仅移除 80 号非基础技能；技能点 1→2 还原 | 确认 skill-reset 是否应还原基础技能等级（CHAR_InitializeSkill 语义） | 本会话测试 2026-08-09 |
| 未开始 | **discharge 后 mercenary 与 party 数据不一致** | 2026-08-09 实测（存档0）：`discharge slot1`（西雷斯在队）返回 ok 后，**mercenary 列表中西雷斯消失，但 party role2 西雷斯仍在**（hp=8184）——discharge 删 mercenary 登记但 party 角色实例未清理；exclude 确认沃尔达克=quest npc（`cannot exclude quest npc`）；discharge 边界正确（空槽 not found/quest npc 拦截/leader 拦截） | 核对 discharge（MERCENARYSYSTEM_Release）与 party 槽关联清理；mercenary/party 两套索引同步 | 本会话测试 2026-08-09 |
| 未开始 | **api-reference mercenary「18 槽」假设错误（实际 88 槽数组 + 两套索引）** | 用户 2026-08-09 质疑后核实（data-sources §2.5）：佣兵槽数组 `*(*(0x2f6010))` 20B/槽，**槽上限 `*(0x2f3978)`=88**（非文档 18）；mercenary 端点返回 slot=槽数组索引（稀疏 0/1/3/4/5/7/19/27...），而 include/exclude/discharge 参数=角色 +0x352 槽 ID（member[0]=0/1=19/2=1）——**两套索引，端点 slot 字段语义待统一/修正** | 修正 api-reference mercenary 槽数说明；mercenary 端点 slot 字段暴露 +0x352 槽 ID 或加映射说明 | 本会话测试 2026-08-09 |
| 未开始 | **mercenary 槽数/索引与游戏实际不符（18 佣兵 vs 88 槽数组）** | 用户 2026-08-09 实测游戏 UI：**佣兵仓库 2 页 × 9 = 最多 18 个佣兵**，非 88。我方 mercenary 端点读 `*(0x2f3978)`（s8=88）与游戏实际不符——**现有佣兵信息不正确**：①槽上限读错（0x2f3978 可能非佣兵槽数或语义不同）；②返回的稀疏 slot 索引与游戏佣兵仓库索引（0-17）不匹配 | 重新逆向佣兵槽结构（MERCENARYSYSTEM_pSlotList 真实上限与索引语义），对齐游戏 18 佣兵仓库 | 本会话测试 2026-08-09 |
| ✅ 2026-08-13 确认 | **API 移动不触发切图** | ~~2026-08-09 实测 move/walk 到出口不切图、mapId 恒 2056~~ | **已确认解决**（用户 2026-08-13）：move_to/walk_dir 现均为正常走路（后台线程逐帧移动），到达出口区域自动切图正常 | 本会话测试 2026-08-09 + 用户 2026-08-13 确认 |
| ✅ 2026-08-13 确认 | **切图需移动以外的额外操作触发** | ~~用户 2026-08-09 实测：API 移动均不切图，需手动触摸才切~~ | **已确认解决**（用户 2026-08-13）：切图无需触摸输入，API 移动即可触发 | 用户 2026-08-09 告知 + 2026-08-13 确认 |
| ✅ **任务完成 NPC（路障）无法 API 交互** | 2026-08-09 实测：路障不被 PLAYER_DoCheckNearNPC 识别（type==2），API interact → no npc nearby。**✅ v0.4.52 已解决**：interact 回退扫描 type==2 可交互物（<60px+朝向匹配）设 NearNPC + UINpc_InitNPC；**✅ v0.4.55 完整打通**：interact → select index 0 打开 npc_quest 面板 → select complete 执行官方完成链（UINpcQuest_ButtonOKExe 0xc3414：UI_SetPopupProcessInfo(3,0)+ChangeQuestState(id,3)+DoCheckAllEvent），真机验证任务 381 完成（st=3）、奖励 popup「再生药水（特大）X3」、新任务 21 激活、槽数组 [180,2,21] | 已完成 | 2026-08-12 v0.4.55 |
| ✅ **units 无任务标记（问号）字段** | 任务 NPC 的「问号」标记不在 units 数据中（type=2 物件无差异）。**✅ v0.4.55 已解决交互需求**：无需问号标记，type==2 扫描 + npc_quest 面板态即可完成任务；units 是否暴露任务标记留待后续按需处理 | 已完成（交互侧）；units 标记字段留待后续 | 2026-08-12 v0.4.55 |
| ✅ **弹窗/对话内容 API 读取失败（ui/dialog 空）** | 问题仅限 npc_quest 等面板类型弹窗数据源（G_POPUP_TEXT 未覆盖）。**✅ v0.4.55 已解决**：dialog/content 新增 npc_quest 面板态（栈顶 0x14b858）返回 {type:npc_quest, questId, state, options:[complete/close]}；任务奖励 popup（标准 dialog）由既有 popup 态覆盖 | 已完成 | 2026-08-12 v0.4.55 |
| ✅ **任务完成弹窗标题（<任务名>完成）未获取** | 任务完成弹窗有标题（<任务名>完成）+内容。**✅ v0.4.55 部分解决**：奖励内容（再生药水特大 X3）经 popup 态读取正常；标题字段（任务名+完成态）数据源在 UINpcQuest_MakeTextEndPopup(0xc3bd0)/DrawEndPopup(0xc32ec)，留待后续提取 | 已完成（内容）；标题字段留待后续 | 2026-08-12 v0.4.55 |
| 未开始 | **skill-usage 缺参 {} 返回 ok（未报 bad body）** | 2026-08-09 实测：`combat/0/config/skill-usage` 缺 body `{}` 返回 ok（api-reference 声称缺 body→bad body）；on=true/false 均 ok、role 越界 role not found 正确 | 核对 skill-usage 参数解析（on 缺省处理） | 本会话测试 2026-08-09 |
| 未开始 | 商店购买/出售 | `UIStore_BuyItem`(0xd242c)/`SellItem`(0xd25f0) 需 ControlObject_GetCursor 选中态（依赖 UI） | 依赖 P1 商店数据结构完成后，探索底层购买/出售函数（绕过 cursor） | api-technical-spec §2.7 |
| 未开始 | 任务列表数据结构 | 仅 `QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID 可读；列表/状态/交付条件无记录。**✅ v0.4.55 部分解决**：quest/list 返回槽数组（12B/槽+0 questId，双层解引用 [0x2f4000+0x3d0]）；quest 状态表（G_NPC_QUEST_STATE_GOT_VMA 三层解引用，0=未接 1=进行 2=可完成 3=已完成）与任务完成链已逆向 | 任务描述/奖励/交付条件等其余字段留待后续 | 本会话页面探索 |
| 未开始 | 静态表字段语义全逆向 | `field_catalog.json` 已验证 71 字段，其余待逆向 | 逐表解析（`*BASE_pData` + `record_index * nRecordSize`） | static-data §7 |
| 未开始 | 背包移动/整理 | `INVEN_MoveItem`(0x104934) 4 参签名复杂（item+3） | 逆向 4 参签名 + 真机验证 | control-capability §5 |
| 未开始 | 强化/镶嵌 | `ITEMSYSTEM_EnchantItem`(0x10b330)/`PutJewel`(0x10bcb4)/`ApplySocket`(0x10d8a4) 需物品+材料上下文 | 逆向执行路径（消耗校验）（归 P0 总条目「物品数据结构逆向」⑩，操作侧） | control-capability §5.2 |
| ✅ 2026-08-12 确认 | 开箱 | ~~`UIEquip_ButtonOpenBoxExe`/`ITEMSYSTEM_OpenItemBox` 未逆向~~ | **已完成**：v0.4.62 确认已融入使用物品（game_ops_action use_item：fn_is_item_box 检测 + fn_open_item_box 独立路径，成功后手动消耗） | api-technical-spec §2.3 |
| 未开始 | 队友 AI 设置 | 队友自动控制决策选项（是否用技能/是否主动攻击），在技能界面设置；只需**读/写选项**，不关心内部运作 | 逆向 AI 选项数据结构（读写选项），做 `GET/POST /api/action/player/{role}/ai` | api-technical-spec §2.2 |
| 未开始 | 交互点 | 宝箱、恢复泉水等非敌人地图内容未探索 | 探索地图交互点数据（宝箱/泉水结构 + 交互函数） | 本会话决策 |
| 未开始 | 融合器/调合箱结构 | 配方表（Class D-S 五级）/材料合成链结构未逆向（网络资料见 game-systems §6.4） | 反汇编融合器/调合箱相关表结构 + 配方数据 | 用户 2026-08-08 指定 P2 |
| 未开始 | 佣兵技能系统 | 佣兵技能不出战也对全队生效、同种不叠加（game-systems §6.5）；数据结构未逆向 | 逆向佣兵技能表 + 全局生效逻辑 | 用户 2026-08-08 指定 P2 |
| 未开始 | 随机奖励的生成机制 | 掉落物/奖励如何生成（MakeItem 链）未探索 | 逆向 `ITEMSYSTEM_MakeItem` 系列 + 掉落表 | 本会话决策 |
| 未开始 | 休息（营地恢复） | `PARTY_ApplyRest`/`PARTY_GetRestCost` 未逆向 | 逆向 + 费用校验 | api-technical-spec §2.2 |
| 未开始 | NPC 交互数据结构 | npc_dialog 面板已识别（v0.3.9），但对话选项/分支结构未探 | hook `UINpc_*` 抓对话选项 + 反汇编 NPC 系统 | 本会话页面探索 |
| 未开始 | activeQuest 接任务后实测 | 未真机验证 | 真机接任务后对比 `QUESTSYSTEM_nActiveQuest` | api-reference §5 |
| 未开始 | mercenary inParty 标志与 party 槽一致性 | 2026-08-09 API 测试基线观察：snapshot 中 party 含西雷斯(role2) 但 mercenaries 中 slot1 西雷斯 `inParty=false`；另多个 `name=null` 槽 `inParty=true` | 核对 mercenary 槽标志位（flags bit1）与 party 成员映射（两套索引），确认 inParty 语义与 name 注入 | 本会话 API 测试 2026-08-09 |

### character 域数据缺口（v0.5.1 设计草案）

> 来源：api-reference.md 第二章 §2.5「待补齐数据」。实现 character 域设计草案（status 聚合/属性名直写/装备位置/技能名称+等级/两词动作 POST/grow 四动作）的前置缺口。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| ✅ 完成 | R1 stats 属性名已逆向（15 项确认） | 已确认 15 项：0暴击率/3暴击伤害/4攻击/8魔攻/11魔法抵抗/13敏捷/14命中基数/15命中率/17防御/18物理减伤/19WDR/20副手攻击/28等级驱动/30HP上限/31MP上限 + 扩展 113总攻击；其余 12 项（1,2,5,6,7,9,10,12,16,21,22-27）LV1 实测全 0 暂占位 | 见 docs/research/character-data-gaps.md 实机验证章节 | api-reference §2.5 R1
| ✅ 完成 | R2 skill max_level 权威读取路径已定 | 技能信息表 `*(0x2f4000+0x9e0)` 记录 +0x1D int16（负=无）→ 表2 `*(0x2f3758)` +9 角色偏移 → `[ch+0x2B2]` bit1-4（4 位）= 最终 max_level（常规 4/技能书 8，frida 实测写 bit1-4=4 切换验证） | 见 character-data-gaps.md R2 | api-reference §2.5 R2
| ✅ 完成 | R3 set_skill_usage 单技能档位修正 | `CHAR_SetSkillUsage` 写 [ch+0x3a0] bit0-3（非 bit0-2）；技能节点 +0x07=1 恒为激活标志，**native 无单技能档位**，仅全局位域（bit0-3 技能使用/bit4-7 自动反击） | frida 实测 [ch+0x3a0]=0x35 正常 | api-reference §2.5 R3
| ✅ 完成 | R4 merc 两套索引统一 | 槽数 **21**（`*(*(0x2f3978))` 双层解引用实测；88 为 GOT 值 0x58 误读）；读=槽数组下标、写=+0x352，经 CHARSYSTEM_FindAsMercenarySlot(0xf4254) 匹配 | 见 character-data-gaps.md R4 | api-reference §2.5 R4
| ✅ 完成 | R5 装备槽位表实机验证 | 基础法杖→主手槽5、漆黑之皮甲→身体槽3，与 equipment 数组一致；槽位=ITEMCLASSBASE+2→槽位表+4；slot 9 未用（8戒指） | frida hook CHAR_FindEquipSlot(0xe4fd0) 实测 | api-reference §2.5 R5
| ✅ 完成 | S1 ITEMOPTINFOBASE.json 打包修复 | v0.5.1 package_assets.py 加 ITEMOPTINFOBASE → 重打包 → 真机复验 option_names 非空（["敏捷","体力","瞬间恢复","武器格挡率"] 等） | commit 1c45668 | api-reference §2.5 S1
| ✅ 完成 | S2 className 联查依据已定 | CHARCLASSBASE +0x00=职业名 text_id=class_idx×2 已验证；StaticData.kt `className()` 待实现（实现阶段） | 见 character-data-gaps.md S2 | api-reference §2.5 S2
| ✅ 完成 | S3 skillName/skillMaxLevel 权威路径已定 | 技能信息表 recN↔action N，技能名=rec+0 u16 text_id（=1220+rec，凯恩 action50=痛苦之击 text[1270] 实测）；max_level 见 R2（**非 SKILLDESCBASE**，backlog 原假设修正）；StaticData.kt 待实现 | 见 character-data-gaps.md S3 | api-reference §2.5 S3
| ✅ 完成 | S4 佣兵名联查依据已定 | MERCENARYINFOBASE +0x04=佣兵名 text_id（35752+idx，47 名全量验证）；name=null 槽成因=无联查函数；StaticData.kt `mercName()` 待实现 | 见 character-data-gaps.md S4 | api-reference §2.5 S4
| ✅ 完成 | D1 static-data.md §7.2 已修正 | +0x00=职业名（class_idx×2）、+0x04=杂项文本（假阳性）；已改 docs/reference/static-data.md §7.2 | commit 016ac25 | api-reference §2.5 D1
| ✅ 完成 | D2 L63 恒空矛盾已闭环 | S1 修复（v0.5.1 打包 ITEMOPTINFOBASE）后 option_names 非空，矛盾消除 | commit 1c45668 + 复验 | api-reference §2.5 D2
| ✅ 完成 | D3 merc 两套索引对齐 | 槽数 21 实测确认，data-sources.md 已修正 88→21（双层解引用） | 见 data-sources.md §2.5 | api-reference §2.5 D3

### world 域数据缺口（v0.5.3 设计草案）

> 来源：api-reference.md 第三章。world 域设计草案（movement 动作两词化/瓦片静态化/地图名注入）的数据缺口。切图问题已由用户确认解决（move_to/walk_dir 正常走路、可自动切图），不再列入。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | W1 掉落物数据源未探索 | `/api/world/map/drops` 恒返回空数组占位；掉落生成链已确认（CHARSYSTEM_Die 0xf5418→DropItem 0xf4d30），场景掉落实体存储位置未定位 | 逆向地面掉落实体结构（疑 EFFECTSYSTEM_ProcessDropItem 0xf828c），实现 drops 端点 | api-reference §3.1 + P2 掉落物条目 |
| 未开始 | W2 静态瓦片矩阵端点验证 | 资产 maps/tiles.json 已打包 416 图（2.2MB base64）；`maps/{map_id}/tiles` 为设计端点（数据已就绪） | 实现端点（读 assets 静态矩阵）；真机抽查矩阵与运行时一致 | api-reference §3.3 |
| 未开始 | W3 map/id 名称注入 | 地图名在复合端点（map_data.name）与静态端点（maps/{map_id}.name）已有；单值端点未注入 | 实现 `id_name` 注入（MAPINFOBASE 联查，与 character 一致） | api-reference §3.1 |

### 研究后续缺口（character-data-gaps.md 产生，2026-08-13）

> 来源：`docs/research/character-data-gaps.md`（R1-R5/S1-S4/D1-D3 研究+实机验证的**遗留后续项**）。前 12 项缺口已闭环，此处为闭环后仍需推进的条目。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | N1 attr 剩余 12 项名称确认 | R1 已确认 15 项；1,2,5,6,7,9,10,12,16,21,22-27 在 LV1 角色实测全 0（21 默认 1000 疑格挡/抵抗类、16 默认 8） | 高等级/带词缀装备角色实机采样（frida hook CHAR_GetAttr 逐 id），补全 attr 名称 | character-data-gaps.md R1 |
| 未开始 | N2 新逆向 GOT 槽/表地址登记 game_symbols.h | 研究新增约 17 个运行时表地址未入符号表（主属性→attr 映射表 ×3、技能信息表 ×4、ITEMCLASSBASE/槽位表 ×4、等级表驱动公式 ×2、默认属性表 ×3、装备词缀表 ×2），换版本静默失效 | 全部登记 G_*_VMA 并核对 check_symbols.py（审计 H6 同类） | character-data-gaps.md 附：地址清单 |
| 未开始 | N3 MAXLEVELBASE 语义逆向 | 48 条 × 4B，语义未确定；max_level 权威路径已确认为技能信息表 + [ch+0x2B2] bit1-4（R2），MAXLEVELBASE 角色待定 | 反汇编引用点或按 48 记录对照等级，确定字段语义 | character-data-gaps.md S3 |
| 未开始 | N4 MERCENARYINFOBASE 索引↔槽 type 对应 | 佣兵名 text_id 已确认（+0x04=35752+idx）；MERCENARYINFOBASE 记录索引 ↔ 槽结构 type 的对应关系未确认 | 反汇编 MERCENARYSYSTEM_AddCharacter，确定 mercId 来源；修 name=null 槽 | character-data-gaps.md S4 |
| 未开始 | N5 StaticData.kt 新增 4 个联查函数 | className()/skillName()/skillMaxLevel()/mercName() 均未实现（S2-S4 的依据已定，assets 表已就绪） | 参照 buildOptionNames 模式实现；skillName 走技能信息表 rec+0（非 SKILLDESCBASE）；接入 party/skills/mercenary 端点 | character-data-gaps.md S2/S3/S4 |
| 未开始 | N6 槽记录→角色对象指针偏移 | R4 槽数组/角色池结构已确认；槽记录字段 → 角色对象指针偏移关系待样本 | 槽数据样本（多佣兵存档）frida dump 验证 | character-data-gaps.md R4 |
| 未开始 | N7 角色 +0x94 属性存储语义 | id28 等级驱动属性写入 [ch+0x94]，语义待定（等级表公式 960+36×lvl） | 对照等级表与面板显示确认该属性身份 | character-data-gaps.md R1 |

### quest 域数据缺口（v0.5.5 设计草案）

> 来源：api-reference.md 第五章。quest 域设计草案（`/api/quest/{id}` 顶层化/active 统一 id/id_name/任务名注入）的数据缺口。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | Q1 任务名联查 questName() | QUESTINFOBASE.json 已在 assets（507 条，text_2=任务名、text_14=描述）；StaticData.kt 无 questName() | 新增 `questName(questId)`（QUESTINFOBASE records[questId].text_2 → text 表）；`quest_id` 与记录索引对应关系需核实 | api-reference §5 |
| 未开始 | Q2 任务进度运行时数据 | `/api/quest/active` 需返回所有已接任务进度（`progress.state` 状态表已定位：G_NPC_QUEST_STATE 三层解引用 0未接/1进行/2可完成/3已完成）；`progress.detail` 进度详情（目标计数等）未逆向 | 逆向 QUESTSYSTEM 槽数组 12B/槽除 questId 外字段 + 任务目标计数数据结构，实现 active 进度返回 | api-reference §5 |
| 未开始 | Q3 已完成任务数据源 | `/api/quest/completed` 需返回**所有已完成**任务，恒占位空数组；G_NPC_QUEST_STATE=3（已完成）状态已定位 | 按状态表过滤已完成任务，实现 completed 端点 | api-reference §5 + backlog L119 |
| 未开始 | Q4 主线/支线任务区分 | active 需 `is_mainline` 字段；QUESTINFOBASE 记录无明确标志（u16[13] 小整数 0-7 疑似任务类型/链，未定性） | 逆向任务类型字段（QUESTSYSTEM 链/任务章节标志），确定主线/支线判定规则 | api-reference §5 |

### ui 域数据缺口（v0.5.7 设计草案）

> 来源：api-reference.md 第六章。ui 域设计草案（统一 dialog 检测/select_option 唯一选择）的数据缺口。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | U1 弹窗类型检测扩展 | 当前仅识别五态（story/npc/popup/wipeout/none + npc_quest 面板态）；save/sell/quest/商人对话等类型未纳入统一检测 | 逆向各弹窗/对话类型识别（保存/出售/任务/商人等 UI 状态数据源），统一生成 `type`+`title`/`text`+`options` | api-reference §6.2 |
| 未开始 | U2 option id ↔ 底层动作映射 | `select_option` 需把 options id（confirm/cancel/next/skip/quit/shop/revive/...）映射到底层动作（OK 按钮/取消/推进/索引选择/面板跳转） | 建立各类型 id → 底层动作转换表（参照现有五态白名单扩展） | api-reference §6.2 |

## P3 暂缓

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | A1 伤害公式逆向 | 属性系数(0.66/0.5)×1.2、逐项取整、技能公式 Z%（参照 game-systems §6.1） | 反汇编找 ×1.2/0.66 常量 + 取整逻辑；突破口：布甲 12% 魔攻加成 | 用户 2026-08-08 指定 P3 |
| 未开始 | A2 经验/成长曲线 | 105 级经验表、升级规则（参照 game-systems §6.2） | 对比经验表/成长曲线公式 | 用户 2026-08-08 指定 P3 |
| 未开始 | A3 强化/混沌机制 | 卷轴增量、混沌±50%/深渊±100%、宝石上限(CRT 6.1/11.1/17)（参照 §6.3） | 反汇编强化计算 + 混沌随机常量 + 上限表 | 用户 2026-08-08 指定 P3 |
| 未开始 | A4 掉落权重表 | 千分比掉率、掉落生成（参照 §6.4/§6.6） | 反汇编掉落表权重（分母 1000）+ 生成链 | 用户 2026-08-08 指定 P3 |
| 未开始 | B1 装备表字段 | ITEMDATABASE/ITEMENCHANTBASE 字段语义未全逆向 | 装备字段解析（强化/耐久/宝石孔/附魔） | 用户 2026-08-08 指定 P3 |
| 未开始 | B2 技能表字段 | 90+ 技能公式参数、Z% 随等级 | 技能表字段解析 | 用户 2026-08-08 指定 P3 |
| 未开始 | B3 怪物表字段 | 属性/掉率/首领强化（等级+3 ATK×1.2 HP×3.6） | 怪物表字段解析 | 用户 2026-08-08 指定 P3 |
| 未开始 | C2 元素属性系统 | 风火冰神圣暗黑毒伤害判定（待确认游戏是否含此系统） | 确认存在性后再探索 | 用户 2026-08-08 指定 P3 |
| 未开始 | C4 悬赏任务/无限地下城 | Bounty Hunter/5-6 层地下城结构（待确认） | 确认存在性后探索 | 用户 2026-08-08 指定 P3 |
| 未开始 | 任务接取/交付 | `QUESTSYSTEM_AcceptReivew`(0x125c70) 硬编码剧情任务 quest 489，非通用 | 依赖 P2 任务列表结构，再找通用接/交函数 | api-technical-spec §2.9 |
| 未开始 | 合成执行 | `UIMix_ButtonMixingExe`(0xc21ec) 依赖材料槽选中态；`MIXSYSTEM_CheckMixture` 仅检查非执行 | 探索 `MIXSYSTEM_*` 底层执行函数 + 材料上下文构造 | api-technical-spec §2.8 |
| 未开始 | 读档 | `SAVE_Load*`/`GAMELOADER`（主菜单操作） | 风险高，暂缓 | api-technical-spec §2.10 |
| 未开始 | `/api/world/movement/path` 真机验证 | v0.2.34 实现（原 /api/info/path，v0.3.13 迁至 /api/action/get-path，v0.4.29 重做为 /api/world/movement/path 自研 BFS） | 真机寻路对比 | api-reference §5 |
| 未开始 | 技能点重置 | `UISkill_ButtonSkillPointResetExe` 含 UIInAppProcess=内购 | 依赖内购 | api-technical-spec §2.5 |
| 未开始 | 复活 | `CHAR_ProcessReviveScroll`/`PARTY_AddHPMP`；角色死亡后复活选项 | 用不到（死亡重进即可），暂缓 | api-technical-spec §2.2 |
| 未开始 | 敌人 AI / 队友 AI 决策逻辑 | 决策算法本身（如何决策，非选项读写） | 麻烦且不影响正常游玩，暂缓 | 本会话决策 |

## 待定区

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 待定 | 模拟器游戏本体启动 | 官方 Emulator API30 / Waydroid A13 + libndk 未实证 | PoC 启动测试 | emulator-research §5 |
| 待定 | 模块 arm64-v8a 打包后 guest 内 dlopen/dlsym | 转译层内 dlopen/dlsym 是否可用未实证（高风险） | PoC：模块仅 arm64-v8a → 游戏启动 → System.loadLibrary | emulator-research §5 |
| 待定 | LSPatch bootstrap 稳定性 | liblspatch.so x86_64 与 guest 进程混合无公开先例 | PoC | emulator-research §5 |
| 待定 | LSPatch 0.6 与 libxposed 101 兼容性 | 内置 runtime 较旧 | 必要时降级 API 93 构建 | README 已知待办 |
