# 开发待办总清单

## 完成标准

- **只有真机验证通过才算完成**：实现后必须在真机（100.110.139.83:5555，游戏运行中）验证行为符合预期且无崩溃。
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

## 问题分级总览

### 🔴 P0 严重+紧急
| # | 系统 | 问题 | 严重点 |
|---|---|---|---|
| 1 | 全局 | ✅ **支付弹窗 hook 阻断（根因）** | v0.4.20：补 NetworkStore 复位 + IapBlocker 模块化 |
| 2 | 架构 | ✅ **支付弹窗拦截 patch 模块化** | v0.4.20：patch/IapBlocker.kt 独立模块 |
| 3 | 背包 | ✅ use-item 仅消耗无效果 | v0.4.20：CHAR_UseItemEx 效果链，真机验证药水消耗/复活卷轴无崩溃 |
| 4 | 战斗 | ✅ attack 改造 | v0.4.20：CHAR_SetActionID(ch,5,target)，狼 HP 120→0 真机验证 |
| 5 | 全局 | ✅ info 端点主菜单假数据 | v0.4.20：game_in_world() 检查，主菜单下全部返回 error |
| 6 | 稳定性 | ✅ 全代码库判空审查与修复 | v0.4.20：fn_get_next_exp/fn_get_bit 判空修复 |

### 🟠 P1 严重
**性能**：
| # | 问题 |
|---|---|
| 1 | 性能优化（减少计算/读取，提高 API 响应） |

**移动/地图**：
| # | 问题 |
|---|---|
| 2 | ✅ walk 行为异常（v0.4.26 返回值语义修复+逐帧驱动，已解决） |
| 3 | ✅ move 瞬移（v0.4.26 FrameTaskManager 逐帧驱动，已解决） |
| 4 | 游戏寻路局限（CHAR_SearchPath 无法绕远路） |
| 5 | ✅ API 移动不触发切图（v0.4.25-26 每步 map_link_check，已解决） |
| 6 | ✅ 切图需移动以外操作触发（同上，move/walk 每步自动检测，已解决） |
| 7 | 移动端点回归 |
| 8 | 运行时 mapId 与静态 ID 两套编号 |

**对话**：
| # | 问题 |
|---|---|
| 9 | ✅ npc/interact 对话未建立（v0.4.27 已解决：interact+get-content+select 全链路真机验证成功，见 npc.md §9） |
| 10 | ✅ npc/dialog/options 读取失败（v0.4.27 由统一 get-content 取代，type=npc 真机验证通过） |
| 11 | npc_quest 面板弹窗读取失败 |
| 12 | ⏳ 剧情对话（AVG）API 已实现（v0.4.27：screen=story 状态机 + 文本读取 + next/skip），**剧情推进/跳过真机待剧情重播验证**（游戏存档不重播，见 npc.md §7） |
| 13 | API 无法准确对应游戏状态（✅ story 已识别 v0.4.27；任务框/部分弹窗仍脱节） |
| 14 | 任务完成弹窗标题未获取 |
| 15 | 任务完成 NPC（路障）无法交互 |
| 16 | ✅ 对话 API 整合方案（v0.4.27 落地：/api/action/dialog/{interact,select} + /api/info/dialog/content 一套 API 覆盖对话框/剧情对话/任务完成框） |
| 17 | 关闭面板 API 未完成 |

**背包**：
| # | 问题 |
|---|---|
| 18 | 物品名映射错位（检查项） |

**角色信息**：
| # | 问题 |
|---|---|
| 19 | 获取角色职业（party 加职业字段） |
| 20 | 技能列表按职业过滤（不含 0-7 普攻） |

**价格**：
| # | 问题 |
|---|---|
| 21 | sell 价格需 ÷5 |

**背包**：
| # | 问题 |
|---|---|
| 23 | 物品 count 字段语义错误 |
| 24 | 装备稀有度显示不全 |
| 25 | 装备词缀/附魔需展开名称与属性值 |
| 26 | inventory bag/{i}/{slot} 单格查询错位 |

**佣兵**：
| # | 问题 |
|---|---|
| 27 | mercenary 槽数/索引与游戏不符（18 vs 88） |

**其他**：
| # | 问题 |
|---|---|
| 28 | 参数错误 403 + 异常透传 |
| 29 | 各函数调用的前提探索 |
| 30 | 商店物品/价格数据结构（DEALSYSTEM） |
| 31 | skill-usage 缺参 {} 返回 ok |
| 32 | character/skill 加点不消耗技能点 |

### 🟡 P2 一般
**战斗/技能**：
| # | 问题 |
|---|---|
| 1 | switch 切换主控未生效 |
| 2 | 普攻 actionId 5/6/7 区别监听 |
| 3 | 技能语义（0-7=普攻非技能，按职业表） |
| 4 | skill-reset 未完全还原 |

**队伍/佣兵**：
| # | 问题 |
|---|---|
| 5 | discharge 后 mercenary/party 不一致 |
| 6 | mercenary inParty 标志一致性 |

**数据/增强**：
| # | 问题 |
|---|---|
| 7 | events 增强（敌人死亡/切图事件） |
| 8 | gameInfo 占位字段修正 |
| 9 | 声音/光效/语言设置 API |
| 10 | 创建新存档 API |
| 11 | socket 字段语义（宝石孔位编码） |
| 12 | 怪物刷新机制（变箱子/罐子） |
| 13 | 怪脱战回血观察 |
| 14 | units 无任务标记（问号）字段 |
| 15 | load 端点与 enter-slot 冗余 |
| 16 | 掉落物数据源未探索 |
| 17 | data 大响应端点 Connection reset |
| 18 | StaticData 缓存并发安全（审计 M1） |
| 19 | 加点分配函数 / 升级技能 / 商店购买出售底层 / 任务列表结构 / 静态表字段 / 附魔对照表 / 背包移动 / 强化镶嵌 / 开箱 / 队友 AI / 交互点 / 融合器 / 佣兵技能 / 随机奖励 / 休息 / NPC 交互 / activeQuest 实测 |
| 20 | 版本产物同步 / LogFile 清理 / HookMain 轮询容错 / 输入白名单 / native 杂项清理 |

### 🟢 P3 暂缓
**任务**：
| # | 问题 |
|---|---|
| 1 | quest/quit 381 not found |
| 2 | 任务列表数据结构 |
| 3 | 任务接取/交付 |
| 4 | 任务完成弹窗标题联动 |
| 5 | 悬赏任务/无限地下城 |

**安全**：
| # | 问题 |
|---|---|
| 6 | HTTP 鉴权 + 网卡绑定 |
| 7 | OP 能力隔离机制 |
| 8 | native 调用前置 ready 检查（遇问题再补） |
| 9 | 弹窗文本安全 |
| 10 | DebugController 处置 |

**其他**：
| # | 问题 |
|---|---|
| 11 | A1-A4 伤害/成长/强化/掉落公式 |
| 12 | B1-B3 装备/技能/怪物表字段 |
| 13 | C2 元素属性系统 |
| 14 | 合成执行（MIXSYSTEM） |
| 15 | 读档（GAMELOADER） |
| 16 | /api/action/get-path 真机验证 |
| 17 | 技能点重置 / 复活 / 敌人 AI 决策 |

### ⏸️ 待定区
| # | 问题 |
|---|---|
| 1 | 模拟器游戏本体启动 |
| 2 | 模块 arm64-v8a 打包后 guest 内 dlopen/dlsym |
| 3 | LSPatch bootstrap 稳定性 |
| 4 | LSPatch 0.6 与 libxposed 101 兼容性 |

---

## 代码审计修复项

### 审计修复·高优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 物品名映射错位检查（**2026-08-09 降为检查项**） | `StaticData.buildItemNames` 用 records 数组下标作 key，但查询传的是 category（ITEMDATABASE id 从 30 起）；**但本次会话实测 name 显示正常（恢复药水/低级宝石等均正确）——需核实是否已修复或复现条件**（可能 buildItemNames 与查询双方都用 category？） | 核实 StaticData.itemName(category) 查询链与 buildItemNames key 是否一致；真机验证 name 与物品一致 | 审计 H1 + 2026-08-09 检查 |
| 未开始 | HTTP 鉴权 + 网卡绑定 | ApiServer 监听 0.0.0.0:8088 零鉴权，局域网任意设备可读写（含 14 个写端点与 dialog 确认） | AndServer 加 token 中间件（Interceptor）+ `inetAddress()` 绑网卡/白名单；未带 token 返回 401/403 | 审计 H2 |
| 未开始 | OP 能力隔离机制 | native/JNI 的 money/exp/statuspoint/teleport/sell（任意定价）已实现，仅靠「不挂路由」隔离，无权限机制 | 加全局开关（默认关闭）或移除 OP native 实现；验证无任何 HTTP 路径可触发 | 审计 H3 |
| 未开始 | events 快照线程安全 | `data_events_json` 局部 static `last`/`has_last` 无锁，多客户端并发轮询丢事件/数据竞争 | 提升为文件级 + `std::mutex` 保护 diff 过程；多客户端并发轮询验证事件不丢 | 审计 H4 |
| 未开始 | 全代码库判空审查与修复（**2026-08-09 扩展自 fn_get_next_exp**） | 审计 H5 发现 `fn_get_next_exp` 漏判空（game_data.cpp:34）；**用户要求扩展到整个代码库**——所有函数指针调用（fn_*，194 处）/野指针读/NewStringUTF 返回值统一判空审查（含 game_data.cpp 中约 30 处疑似未判空调用点） | 系统性审查全部 fn_* 调用点与指针读，补判空；init 失败路径不崩溃 | 审计 H5 + 用户 2026-08-09 |
| 未开始 | 裸地址入符号表 | game_data.cpp 34 个裸 VMA（debug UI 12 + 面板识别 22，含 1 个与 G_POPUP_FPCANCEL_VMA 重复），换版本静默失效 | 全部入 game_symbols.h（G_*_VMA/F_*_PANEL_*）并登记 check_symbols.py 清单；check_symbols 通过 | 审计 H6 |

### 审计修复·中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | StaticData 缓存并发安全 | `cache` 为普通 HashMap，多客户端并发 GET /api/data/* 竞争 | 换 ConcurrentHashMap（或 computeIfAbsent 原子化） | 审计 M1 |
| 未开始 | 写操作并发互斥 | 全部 POST 端点无锁，native 锁仅初始化用；并发 move/equip 并发调游戏函数，attach 快照 read-modify-write 竞态 | 写操作全局 ReentrantLock，操作与快照读取同锁 | 审计 M2 |
| 未开始 | native 调用前置 ready 检查 | controller 直接调 external，loadLibrary 失败抛 UnsatisfiedLinkError（Error 捕不到）→ 500 | 入口统一检查 `NativeBridge.ready`，未就绪返回 503 JSON | 审计 M4 |
| 未开始 | 错误响应语义统一 | 失败全 HTTP 200 + 手写串/native 原串（"-1"）透传，无 400/404/500 | 统一 JSON 包装 + 状态码语义；native 失败值转结构化错误 | 审计 M5 |
| 未开始 | **参数错误 403 + 异常透传** | 2026-08-09 全量探测实测：`/api/info/party/abc`、`/api/info/inventory/bag/xx/info`、`/api/info/mercenary/abc` 返回 **HTTP 403 + 原始 Java 异常串**（`java.lang.NumberFormatException: For input string: "abc"`），非 JSON 包装，违反 architecture §9.3（应 400 + JSON 错误体） | 路由参数解析处捕获 NumberFormatException，统一走 JsonUtil 错误响应 | 本会话全量探测 2026-08-09 |
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
| 进行中 | **完成 api-reference 全部未实现端点** | 已实现（24 个操作端点全部真机验证）：movement(move/move-cancel/walk/walk-stop ✅v0.4.1)、combat(attack/stop ✅v0.4.2 + skill-usage ✅v0.4.10 + cast ✅v0.4.12 + auto-attack/switch 早前)、inventory(sell ✅v0.4.3/move ✅v0.4.4/jewel ✅v0.4.6 + use-item/discard/equip/unequip 早前)、character(stat ✅v0.4.5/stat-reset ✅v0.4.7/skill-reset ✅v0.4.11)、party(discharge ✅v0.4.8/withdraw ✅v0.4.9 + include/exclude 早前)、npc(interact/dialog-select/dialog-next ✅v0.4.13 + GET dialog/options)、shop(buy ✅v0.4.14 + GET /info/shop/items)、quest(quit ✅v0.4.15)、save(save ✅v0.4.16)、ui(dialog-ok/cancel 早前 + main-menu ✅v0.4.17 GAMESTATE_SetState 纯 API 回主菜单)；**剩余卡点**：ui panel(⛔POPUPSTATE_Pop 崩溃)、save/load(⛔主菜单选档 P3)、craft mix(⛔合成链已逆向见 craft.md，需合成器交互验证)；OP 端点受 architecture §9.1 约束暂缓 | 按类别逐项：先探索底层函数（多数 P1/P2 依赖 UI 状态或未逆向，见 api-technical-spec 对应行）→ Service 层接线 → 真机验证 → 文档更新 | 2026-08-08 用户指定 P0 |

## P1 中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 各函数调用的前提探索 | 操作端点通用前置未系统验证 | 探索各操作的前提约束：是否任何界面都能保存？能否跳过确认弹窗直达操作（如直接退到主菜单）？——所有操作端点的通用前置 | 本会话决策 |
| 未开始 | 移动端点回归 | v0.3.12 实测可用：`move→(168,528)` 成功；此前 `no path` 为目标坐标在墙内（不可达），非端点 bug | 无需修复；若后续失效（如换地图/控制态变化）需诊断 SearchPath 路径 | 用户 2026-08-08 指定 P1 |
| 未开始 | **info 端点主菜单下应报错（native 层检查 + API 诚实转发）** | 用户 2026-08-09 要求：current-map/party/mercenary/inventory/npc 在主菜单（非 world）下应报错并说明原因（如 `not in game`）；**报错在 native 数据函数调用时检查（与写操作 `game_in_world()` 一致）**，不在 API 端点代码中判断；API 遇到 native 错误诚实转发。现状：data_map/party/mercenaries/inventory/npc_dialog_options/_player_json 无状态检查，主菜单下返回假数据（mapId=0/x=-1/空槽位/name=null），不诚实。`data_snapshot_json` 独立实现（自读 g_state）不受影响。**用户确认（2026-08-09）：quest 复合端点随 data_player_json 一并报错** | ① native：上述 data_*_json 函数开头加 `if (!game_in_world()) return {"error":"not in game"}`；② Kotlin `InfoApiServiceImpl`：各方法解析 native 返回前检测 error 字段原样转发（不自行判断状态） | 用户 2026-08-09 告知 |
| 未开始 | **API 无法准确对应游戏状态** | 用户 2026-08-09 实测：界面 API 无法准确对应游戏状态——**剧情对话**（AVG 对话框）screen 显示 world、**任务框**（npc_quest 面板）识别到但内容读不到、部分弹窗（路障提示）dialogActive=false——API 的 screen/dialog 状态机与游戏实际 UI 状态脱节。**✅ v0.4.27 已解决剧情对话部分**（screen=story + story 对象，GAMESTATE_nState==1 判定）；任务框/部分弹窗仍脱节 | 剩余：npc_quest 面板内容读取（见 P1 #11）；路障提示弹窗数据源（G_POPUP_TEXT 未覆盖） | 用户 2026-08-09 告知 + v0.4.27 部分解决 |
| 未开始 | data 大响应端点偶发 Connection reset | 2026-08-09 全量探测：`/api/data/map/list`、`ITEMDATABASE`、`text`、`events` 大 JSON 响应偶发 `Connection reset by peer`（复测单发正常；`/api/data/list` 小响应稳定） | 确认是否为 AndServer 大响应写超时/连接重置，必要时调大超时或分页 | 本会话全量探测 2026-08-09 |
| ✅ v0.4.26 | walk 端点行为验证 | 2026-08-09 API 测试：`walk {"direction":1}`（右）坐标完全不动；`{"direction":2}`（下）瞬间移动 432px（88,536→88,104），与文档「持续移动（模拟方向键）」语义不符；另 api-reference §0.4 未注明 walk 需 `{"direction":0-3}` 参数（实测缺参报错 `direction 0-3 required` 才得知） | **已解决**：v0.4.26 CHAR_Move 返回值语义修复（0=成功走一步/非 0=撞墙，判断反了导致只走 1 步）+ 后台线程逐帧驱动（FrameTaskManager）；真机验证 160→192→224→248 逐帧移动每步帧递增；direction 0-3 参数已注文档 | 本会话 API 测试 2026-08-09 + 2026-08-11 修复验证 |
| 未开始 | 商店物品/价格数据结构 | UIStore 商品列表/价格表（DEALSYSTEM）未逆向 | 反汇编 `DEALSYSTEM_FindSaleByID` + UIStore 初始化链 | 商店买卖前置依赖 |
| 未开始 | 释放技能 | `UISkill_SkillMainExe`/`UIPlay_ButtonSKill` 依赖 UI/快捷键状态（战斗价值最高） | 探索底层技能释放函数（CHAR 技能使用链），做 `POST /api/action/player/{role}/cast` | api-technical-spec §2.2 |
| 未开始 | 手动存档 `/api/save` | `SAVE_Save`(0x129600) 依赖存档上下文 `[x0+0x8c0]`；`SAVE_ProcessSave`/`SaveData` 确认不可直接调用 | 逆向 SAVE_Save 完整签名/上下文，或探索 `UIPlay_CallSave` 触发路径 | control-capability §5.2 |

## P2 低优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 加点分配函数 | `CHAR_SetStatusPoint`(0xd9c4c) 是"设剩余点数"（OP 语义），真正"属性+1/能力点-1"分配函数未记录 | frida hook 人物属性页加点按钮抓底层调用；找到后做 `POST /api/action/player/{role}/stat`（≤剩余点校验 = 普通） | 本会话页面探索 |
| 未开始 | 佣兵遣散 | `MERCENARYSYSTEM_Release`(0x118ab4) 未逆向 | 逆向签名 + `POST /api/action/mercenary/discharge` | api-technical-spec §2.6，2026-08-08 降 P2 |
| 未开始 | 升级技能 | `CHAR_ProcessSkillBook`(0xe2488)（技能书路径）；`UISkill_ButtonUpExe` 依赖 UI | 探索升级函数与技能点校验 | api-technical-spec §2.5，2026-08-08 降 P2 |
| 未开始 | **声音/光效/语言设置操作 API** | 用户 2026-08-09 要求测试「主菜单环境设置」修改（声音/光效/语言各一次），**实测无对应操作端点**（底层已逆向，见 data-sources §2.7）：声音 `APPINFO_Set/GetSound`@0xd8538/0xd8528、音量 `APPINFO_Set/GetVolume`@0xd84e8/0xd84f0（开=6 关=0）、画质 bit2 置/清（child 2/3）、语言 `SGL_SetLanguage`@0x944f8 + UI 语言索引 `*(0x2f9000+0xf34)`（0-4 循环，0=简体中文） | 按设置项实现 `/api/action/settings/*` 或 options 类别端点（主菜单 SC_OPTION_MMENU 面板场景，不需 world） | 用户 2026-08-09 告知 |
| 未开始 | **创建新存档操作 API** | 用户 2026-08-09 确认未开发：无 new-game/创建存档端点；现只有 enter-slot（进已有存档，v0.4.18 用 SAVE_CreateSaveSlot 初始化槽区） | 探索新建存档链（SAVE_CreateSaveSlot + 角色初始创建 + 新手流程），实现 `/api/action/save/create` 或类似 | 用户 2026-08-09 告知 |
| 未开始 | **load 端点与 enter-slot 功能冗余** | 2026-08-09 代码核实：`/api/action/save/load`（SaveController.kt:33）硬编码 `{"ok":false,"error":"not implemented"}` 纯占位，无任何逻辑；其设计目标（主菜单/选档界面读档进 world）已被 `/api/action/save/enter-slot`（v0.4.18，复现 SaveSlot_SlotButtonExe 链）完全覆盖 | 二选一：load 路由转发到 enter-slot 逻辑，或移除 load 端点并更新 api-reference §0.4/§3.1 | 本会话测试 2026-08-09 |
| 未开始 | **关闭面板 API 未完成** | 用户 2026-08-09 要求记录：character_info 面板打开后**无 API 可原地关闭**（`panel/close` v0.4.5 因 POPUPSTATE_Pop 在 settings 场景 SIGSEGV 已撤销，`ui/panel/open`/`close`/`close-to` 现 404）；实测只能 main-menu 回主菜单 → enter-slot 重进兜底。doc：api-reference §3.1 ui panel 卡点 | 待 POPUPSTATE popup 栈状态机逆向（pop 顺序敏感性）后恢复 panel/close；或探索按面板类型安全关闭（character_info 场景单独验证） | 本会话测试 2026-08-09 |
| ✅ v0.4.26 | **支付弹窗 hook 阻断导致面板触摸失效（v0.4.19 修复不完整）** | 用户 2026-08-09 实测修正：与 enter-slot 无关——**手动进入存档同样复现**；根因是模块 hook `SelectTarget.iapSelectTarget` 阻断付费弹窗（v0.4.19 为修「HUD 无 UI」引入）的副作用。现象：进 world 后移动/攻击/NPC 对话/界面按钮均正常，但**打开面板（如 character_info）后触摸不响应**，无法点击/关闭（只能 main-menu 重进） | **用户 2026-08-11 实测：触摸已修复，可以触摸了**——原推断（iapSelectTarget hook 副作用）不成立或已被后续修复消除；面板打开/关闭触摸正常 | 用户 2026-08-09 实测告知 + 2026-08-11 确认修复 |
| 未开始 | **运行时 mapId 与静态/文本 ID 不一致（疑似需换算/映射）** | 用户 2026-08-09 实测质疑：当前地图「影子丛林1」运行时 `MAP_nBaseInfo+0`=2056（data-sources §2.4 声称实时地图 ID），但静态 MAPINFOBASE/text 中「影子丛林1」=**3543**；text_id 2056 实际是技能描述文本（非地图名）。**两套编号体系确认**（运行时内部编号 vs text_id），`/api/data/map/2056` → not found、`/api/data/map/3543` → 影子丛林1 均符合现状，但运行时 ID 无法用于静态表查询，调用方需映射 | 逆向运行时 mapId → text_id 映射（怀疑 MAP_nBaseInfo 结构另有地图索引字段，或地图表数组下标换算）；在 api-reference 记录两套编号说明或提供换算端点 | 本会话测试 2026-08-09 |
| 未开始 | **units「地图出口」×4 实为 2 出口 + 2 指示符** | 用户 2026-08-09 实测：当前地图实际只有 2 个地图出口，units 输出 4 个「地图出口」（slot1-4，status=2）——**出口两侧各有一个指示符/标识物，名称同为「地图出口」（CHAR_GetName 来源）无法区分**；enemies 端点同样包含宝箱/泉水等非敌人物件（status==2 过滤语义） | 核对 status/type 语义或地图物件类型字段，区分真出口/指示符/交互物；enemies 过滤条件是否应排除非敌人（宝箱/泉水/出口） | 本会话测试 2026-08-09 |
| 未开始 | **events 增强：被动触发事件** | 用户 2026-08-09 要求（保持现有差异检测逻辑，不做 since）：新增战斗/移动中被动触发的事件——**敌人数量变化、单位死亡、切换地图** 等（当前仅 money/inventory/move/hp/mp/level_up/exp） | 快照结构 Snapshot 增补字段（敌人数/单位死亡标志/地图 ID），diff 时输出新事件类型 | 用户 2026-08-09 告知 |
| 未开始 | **gameInfo 占位字段修正** | 用户 2026-08-09 要求：`InfoApiServiceImpl.gameInfo()` 中 `loggedIn`（null 占位）**删除**；`saveSlots`（空数组占位）改为**当前加载的存档槽位 int**（0/1/2） | 逆向「当前加载存档槽」内存位置（SAVE 链/存档上下文，data-sources §2.7 附近）；修改 gameInfo 返回 | 用户 2026-08-09 告知 |
| 未开始 | **~~attack/cast 无实际伤害~~（已证伪，2026-08-09 实测修正）** | 初判 attack/cast 无伤害，**后证伪**：持续观察（auto-attack 开启）后玩家实际击杀怪（slot8 狼、slot9 蝙蝠 hp=0）；cast actionId=80（治疗）MP 200→128 真实消耗。**正确结论**：① attack 生效需时间（LV2 普攻 + 怪移动，17s 内未见击杀是观察不足）；② cast 用基础技能（actionId 0-3，普攻类）无 MP 消耗无效果，**需用有效技能 ID**（80=治疗确认生效）；③ cast actionId=7 返回 `no target`（需目标型技能）；④ 怪死亡后 cast 无崩溃（14:32 崩溃未复现） | 无（测试已完成，结论已修正） | 本会话战斗测试 2026-08-09 修正 |
| 未开始 | **普攻 actionId 5/6/7 区别监听** | 用户 2026-08-09：实测确认 5/6/7 均为普攻（无 MP 消耗、同 -62 伤害、击杀怪），行为无法从 API 区分——需**监听游戏内点击普攻按钮时触发的 actionId**，判断三者是连击段（依次使用）还是独立技能；SKILLDESCBASE 不含 0-7（基础技能无静态名） | frida hook 普攻按钮/攻击键回调（UIPlay 攻击链），记录每次普攻调用的 actionId 序列 | 用户 2026-08-09 告知 |
| 未开始 | **move 瞬移为同步走完路径（设计确认）** | 2026-08-09 代码确认：`data_op_move` 单请求内循环 `fn_move_as_path`（≤512 次）同步走完全程，视觉表现为瞬移；**非 teleport**（TP=fn_set_position 直设坐标）；原因：清零控制态后游戏主循环不自动续走路径，故手动循环。是否改为「寻路后由主循环逐帧走」（需探索主循环续走机制）待用户决策 | 若需真实移动动画：探索让游戏主循环续走 PATHLIST 的机制（替代手动循环）；或保持现状并文档说明 | 本会话测试 2026-08-09 |
| 未开始 | **掉落物数据源未探索** | 2026-08-09 **测试完成**：击杀怪（slot9 狼 hp→0，exp+4902 确认）后，`/api/info/current-map/drops` 硬编码返回 `{"drops":[]}`（InfoApiServiceImpl.currentMapDrops 占位）、units 无掉落实体、events 无掉落事件——**掉落物当前无法通过任何 API 获取**（用户确认场景实际有大量掉落物） | 逆向掉落物/地面物品数据结构（怪死亡掉落生成链 + 场景掉落实体），实现 drops 端点 | 本会话测试 2026-08-09 |
| 未开始 | **物品 count 字段语义错误（无数量物品误报）** | 用户 2026-08-09 实测指出：**并非所有物品都有数量**——装备/宝石无数量，材料/骰子有数量；单格数量上限 99。当前 count 字段：装备显示 100、宝石显示 0（100=无数量标记、0=无数量），材料显示真实数量（如皮革 1）——count 需按物品类型区分语义，不能直接当数量 | 逆向物品类型→数量字段语义（装备/宝石无 count、材料有 count≤99）；API 对无数量物品的 count 字段应省略或语义化 | 用户 2026-08-09 告知 |
| 未开始 | **装备稀有度显示不全** | 用户 2026-08-09 实测指出：装备稀有度（rarity）字段显示不全（如皮甲 rar2 但实际稀有度信息缺失/错位） | 核对 rarity 来源（fn_get_rarity）与 ITEMRARITYGRADEBASE 映射，确认 5 档稀有度（0白/1绿/2蓝/3黄/4紫）显示 | 用户 2026-08-09 告知 |
| 未开始 | **装备词缀/附魔需展开名称与属性值** | 用户 2026-08-09 要求：格子物品信息对装备，除词缀 ID 外还需输出**词缀名称 + 词缀属性值**；附魔（enchant）同理。静态表 `reference/enchant-table.json` 已有 36 项属性 ID→名称映射（inc=属性ID：0-4 主属性/5 暴击率/14 暴击抵抗/15 暴击伤害/17-29 元素等） | InfoApiServiceImpl 注入：options 词缀 ID 按 enchant-table 展开为 [{id,name,value}]；enchant 附魔同理（ITEMENCHANTBASE） | 用户 2026-08-09 告知 |
| 未开始 | **switch 切换主控未生效** | 2026-08-09 实测（存档0 LV27）：`combat/2/switch` 返回 `ok:true` 但 **mainMercenarySlot 仍=0、leader 仍=凯恩**——切换未生效；`combat/9/switch` 返回 `bad slot`（边界正确）。api-reference 声称 v0.3.2 修复 switch 路由注册（switchPlayer），但实测无效 | 排查 switch 端点实现（nativeOpSwitch → data_op_switch → fn 切换主控链）：可能调用了错误函数/未写 mainMercenarySlot/需要先决条件（如死亡角色不可切） | 本会话测试 2026-08-09 |
| 未开始 | **attack 端点仅设目标不造成伤害** | 2026-08-09 实测确认（存档0 LV27，用户怀疑成立）：`combat/0/attack {"targetSlot":20}` 后**持续 10 秒不停止，目标怪 hp 恒 4095 无伤害**（仅怪物信息条出现=目标锁定）；对照 `cast 5`（普攻）立即 -403。api-reference 声称 v0.4.2 "CHAR_SetTarget+CHAR_MakeDefaultAttack" 攻击，实测 MakeDefaultAttack 未触发实际攻击帧（仅让 AI 决策默认攻击）。**结论：普攻必须用 cast 5，attack 端点不产生伤害** | 排查 CHAR_MakeDefaultAttack 调用条件（是否需战斗态/距离/AI 决策链）；或 attack 端点改为 cast 5 同款普攻逻辑 | 本会话测试 2026-08-09 |
| 未开始 | **怪脱战回血观察** | 2026-08-09 观察：slot19 怪被 cast 5 打至 3692 后（玩家 stop 后）数分钟回升至 4095 满血——疑似脱战回血机制 | 确认怪脱战回血规则（时间/距离条件） | 本会话测试 2026-08-09 |
| 未开始 | **怪物刷新机制（变箱子/罐子）** | 用户 2026-08-09 告知：**怪物会刷新，刷新有几率变成箱子或罐子**（实测 units 中 slot19 由警卫兵变「箱子 hp=1」）——units 的怪身份可能随时变化（刷新后），客户端需注意目标失效 | units 数据观察刷新后形态变化；攻击/锁定前需重新校验目标 | 用户 2026-08-09 告知 |
| 未开始 | **attack/cast 端点行为确认（不改实现）** | 用户 2026-08-09 决策：**attack 端点不改**（保持「仅锁定目标」语义，实际攻击链 = attack 锁定 → cast 5 普攻）；**cast 保持现状**（设置动作由 AI 帧执行，有效），仅完善校验。**新发现**：cast 5（普攻）使用后**若不 stop，角色会自动继续攻击**（AI 自动连击）——「普攻一下」需 cast 5 后立即 stop | attack 不改；cast 完善校验（如 actionId 白名单/MP 校验）；文档记录「attack=锁定、cast5=普攻、不停止=自动连击」 | 用户 2026-08-09 决策 |
| 未开始 | **use-item 仅消耗物品未实现效果** | 用户 2026-08-09 确认：`use-item`（`fn_consume_item`）**只消耗物品不产生效果**——恢复药水（大）使用后角色血量未回满、复活卷轴（无需指定目标，游戏内直接使用即可）使用后队友未复活。`data_op_use_item` = `fn_consume_item(item)` 直接消耗，未触发物品效果链（药水回血/复活卷轴复活） | 探索物品效果正确触发链（ITEMSYS 使用物品 → 效果应用：回血/复活等），修正 use-item 分派；或按物品类型调对应效果函数 | 本会话测试 2026-08-09 |
| 未开始 | **inventory bag/{i}/{slot} 单格查询错位** | 2026-08-09 实测：`bag/0/12` 返回 slot14 中级宝石（实际 bag0 slot12 是低级宝石）。根因：InfoApiServiceImpl.bagSlot 用 `items.optJSONObject(slot)` **按数组下标取**，但 native items 数组只含非空物品（跳过空格）→ 数组下标 ≠ 实际格号，错位 | bagSlot 改为按 `it.optInt("slot")==slot` 匹配（与 bagInfo 的 bag 匹配逻辑一致） | 本会话测试 2026-08-09 |
| 未开始 | **socket 字段语义（宝石孔位编码）** | 2026-08-09 实测 + 用户假设：镶嵌成功装备 socket 64→65（低 bit 疑似已镶标志）；**用户假设：socket 位域前后半段分别存「已镶嵌孔数」与「总孔数」**（如 0x45=0b01000101：前 4 bit 已镶？后 4 bit 总孔？）。需逆向 ITEMSYSTEM_PutJewel/ApplySocket 孔位判定逻辑确认 | 反汇编 socket 位域解码（孔总数/已镶数/孔类型），API 输出时展开为可读孔位信息 | 本会话测试 2026-08-09 |
| 未开始 | **character/skill 加点不消耗技能点** | 2026-08-09 实测（存档1 LV2）：`character/skill {"actionId":80,"level":2}`（80=凯恩第一个真技能，治疗）成功升级但 **skillPoints 恒 1 未消耗**——与 api-reference 声称「学习技能消耗技能点」矛盾（可能改版机制/未校验） | 核对 character/skill 实现是否校验/扣减技能点；与改版技能点机制对齐 | 本会话测试 2026-08-09 |
| 未开始 | **技能语义澄清：actionId 0-7=普攻非技能，技能按职业静态表** | 用户 2026-08-09 澄清：**0-7 号是普攻/基础动作（非技能，同 5/6/7 普攻），80 才是凯恩第一个技能（治疗）**；角色技能列表应从**职业（CHARCLASSBASE 等）**获取静态数据推导，而非仅读技能链表 actionId。此前测试把 0 号当「第一个技能」加点有误（0 号非技能） | API 技能端点（party/{slot}/skills）输出时按职业静态表区分真技能与基础动作；文档修正 skills 语义 | 用户 2026-08-09 告知 |
| 未开始 | **skill-reset 未完全还原（基础技能等级保留）** | 2026-08-09 实测：skill-reset 后 0 号技能仍 LV2（未还原到 LV1），仅移除 80 号非基础技能；技能点 1→2 还原 | 确认 skill-reset 是否应还原基础技能等级（CHAR_InitializeSkill 语义） | 本会话测试 2026-08-09 |
| 未开始 | **discharge 后 mercenary 与 party 数据不一致** | 2026-08-09 实测（存档0）：`discharge slot1`（西雷斯在队）返回 ok 后，**mercenary 列表中西雷斯消失，但 party role2 西雷斯仍在**（hp=8184）——discharge 删 mercenary 登记但 party 角色实例未清理；exclude 确认沃尔达克=quest npc（`cannot exclude quest npc`）；discharge 边界正确（空槽 not found/quest npc 拦截/leader 拦截） | 核对 discharge（MERCENARYSYSTEM_Release）与 party 槽关联清理；mercenary/party 两套索引同步 | 本会话测试 2026-08-09 |
| 未开始 | **api-reference mercenary「18 槽」假设错误（实际 88 槽数组 + 两套索引）** | 用户 2026-08-09 质疑后核实（data-sources §2.5）：佣兵槽数组 `*(*(0x2f6010))` 20B/槽，**槽上限 `*(0x2f3978)`=88**（非文档 18）；mercenary 端点返回 slot=槽数组索引（稀疏 0/1/3/4/5/7/19/27...），而 include/exclude/discharge 参数=角色 +0x352 槽 ID（member[0]=0/1=19/2=1）——**两套索引，端点 slot 字段语义待统一/修正** | 修正 api-reference mercenary 槽数说明；mercenary 端点 slot 字段暴露 +0x352 槽 ID 或加映射说明 | 本会话测试 2026-08-09 |
| 未开始 | **mercenary 槽数/索引与游戏实际不符（18 佣兵 vs 88 槽数组）** | 用户 2026-08-09 实测游戏 UI：**佣兵仓库 2 页 × 9 = 最多 18 个佣兵**，非 88。我方 mercenary 端点读 `*(0x2f3978)`（s8=88）与游戏实际不符——**现有佣兵信息不正确**：①槽上限读错（0x2f3978 可能非佣兵槽数或语义不同）；②返回的稀疏 slot 索引与游戏佣兵仓库索引（0-17）不匹配 | 重新逆向佣兵槽结构（MERCENARYSYSTEM_pSlotList 真实上限与索引语义），对齐游戏 18 佣兵仓库 | 本会话测试 2026-08-09 |
| 未开始 | **游戏寻路局限（CHAR_SearchPath 无法规划远路）** | 用户 2026-08-09 实测告知：**游戏自身寻路（CHAR_SearchPath）不能绕太远的路**——即使存在路径也可能规划失败（返回 no path）；实测存档 1 左上出口 (8,152) 从 (264,112) 起 move 全部 no path（需绕行但寻路规划不出）。move 端点依赖 fn_search_path，受此局限 | **需自行实现更好的寻路机制**（外部 A*/BFS 基于瓦片通行矩阵 + 分段 move），或改进 move 端点（瓦片数据已有 tile 通行查询） | 用户 2026-08-09 告知 |
| 未开始 | **API 移动不触发切图** | 2026-08-09 实测（存档1，地图2056）：玩家通过 move/walk 到达左上出口列（x=8，(8,136)/(8,152) 出口单位旁），**地图始终不切换**（mapId 恒 2056）——游戏切图由主循环检测玩家与出口碰撞触发，move 瞬移式移动未触发切图检测（或 GAMESTATE_ProcessMapChange 未被驱动） | 探索切图触发机制（出口碰撞检测条件/切图状态机），move 端点需支持触发切图；或验证官方切图路径（walk 持续移动进入出口格） | 本会话测试 2026-08-09 |
| 未开始 | **切图需移动以外的额外操作触发** | 用户 2026-08-09 实测：API 移动（move/walk，含顶墙持续走 3s）到出口位置均不切图；**用户手动触摸往左走即切图**——切图触发需要移动以外的额外操作/机制（触摸事件/输入状态/切图判定条件未满足） | 探索切图触发完整链（触摸输入 → 移动帧 → 出口碰撞检测 → GAMESTATE_ProcessMapChange），找出 API 移动缺失的触发条件 | 用户 2026-08-09 告知 |
| ✅ v0.4.27 | **npc/interact 对话建立链研究** | 2026-08-09 实测 interact 返回 ok 但对话未建立；用户 2026-08-11 确认非 hook 副作用。**v0.4.27 已解决**：interact（PLAYER_DoCheckNearNPC+UINpc_InitNPC）→ get-content → select 全链路真机验证成功（杂货商人：type=npc/speaker/text/options=[下一句] 全正确） | — | 本会话测试 + v0.4.27 验证 |
| ✅ v0.4.27 | **npc/dialog/options 读取失败（count=0 实际有选项）** | 2026-08-09 实测：用户手动打开商人对话有选项，API count=0。**v0.4.27 由统一 get-content 取代**：NPC 对话返回 type=npc + options（id=0..5 选择型 / id=next 对话型），真机验证 speaker/text/options 正确 | — | 本会话测试 + v0.4.27 验证 |
| 未开始 | **sell 价格与游戏实际不符（API 20 vs 游戏 4 铜币）** | 2026-08-09 实测：sell 低级宝石返回 `price=20`、money 每次 +20（铜币单位，API money 读数与截图 1S15C=115C 一致）；但**游戏界面显示出售价 4 铜币**（0G 0S 4C）。sell 价格来源 `fn_item_get_price(item)`（ITEM_GetPrice）返回 20 ≠ 游戏实际 4。**用户确认换算：出售价 = 真实价格 ÷ 5**（恢复药水小 购买 15/出售 3、低级宝石 20/出售 4）——ITEM_GetPrice 返回真实价格，**sell 需除以 5**（可能改版 70% + 币制 5 铜=1 银 换算） | sell 端点 price 计算改为 `fn_item_get_price(item) / 5`（并核实除 5 的精确机制：改版系数/币制） | 用户 2026-08-09 确认 |
| 未开始 | **任务完成 NPC（路障）无法 API 交互** | 2026-08-09 实测（地图32896）：任务完成点=路障（slot7，头上有问号），用户贴路障后游戏内点攻击可打开完成任务面板，但 API：`npc/interact` → `no npc nearby`（路障不被 PLAYER_DoCheckNearNPC 识别为 NPC）；`attack 索敌` 锁定成功（state 显示 **activeQuest=381**）但面板未打开。**与商人交互失败（hook 问题）不同——此处是路障不被识别为 NPC**。用户补充：可能与 NPC 交互和互动点交互是同样的机制，需后续探索研究 | 探索任务 NPC 判定（问号标记来源）+ 任务完成面板打开链；路障类任务物如何交互 | 本会话测试 2026-08-09 |
| 未开始 | **units 无任务标记（问号）字段** | 2026-08-09 实测：任务完成 NPC（路障）与其他物件（火把/出口/宝箱）在 units 字段上**无差异**（均 type=2/status=2/level=1/hp=792，仅 name 不同）——「头上问号」任务标记不在 units 数据中，任务 NPC 需从其他数据源获取（用户提示：可能可获取地图中的任务 NPC） | 探索任务 NPC 数据结构（QUEST/CHAR 标记字段、问号绘制来源） | 用户 2026-08-09 告知 |
| 未开始 | **弹窗/对话内容 API 读取失败（ui/dialog 空）** | 2026-08-09 实测（地图32896）：路障提示弹窗（标题「路障」+文本+确认按钮）API `ui/dialog` 返回 active=false。**修正**：标准 dialog 弹窗（screen=dialog，任务奖励确认框）读取**正常**——**问题仅限 npc_quest 等特定面板类型的弹窗数据源**（G_POPUP_TEXT 未覆盖）；v0.4.27 统一 get-content 的 popup 态已覆盖标准弹窗（type=popup + ok/cancel 选项） | 排查 npc_quest 面板弹窗的数据源 | 本会话测试 2026-08-09 |
| 未开始 | **任务完成弹窗标题（<任务名>完成）未获取** | 用户 2026-08-09 实测：任务完成弹窗除内容（再生药水特大 X3）外**还有标题「<任务名称>完成」**（如「XXX完成」），当前 API 未读取该标题字段，需后续探索获取 | 探索任务完成弹窗的标题数据源（任务名+完成态） | 用户 2026-08-09 告知 |
| ✅ v0.4.27 | **剧情对话（AVG 模式）无 API 支持** | 2026-08-09 实测剧情对话中 API 全部读不到。**v0.4.27 已实现**：①状态机检测 `GAMESTATE_nState==1`（Event）→ screen=story（真机验证正确，剧情中 gst=1/evtNState=3/pText 非空）；②内容读取 `/api/info/dialog/content` → type=story + speaker/text/index/count（首次剧情采集文本完整）；③推进/跳过 `dialog/select` action=next/skip（按 EVTSYSTEM_PressKey 反汇编实现）。**待办**：剧情重播验证 next/skip 真机效果（游戏存档不重播剧情，需新存档/新进度触发） | 无（实现完成，验证待剧情可重播时补） | 本会话测试 2026-08-09 + v0.4.27 |
| 未开始 | **quest/quit 主线 381 报 quest not found** | 2026-08-09 实测：`activeQuest=381`（任务激活）但 `quest/quit {"questId":381}` → `quest not found`（QUESTSYSTEM_Find 找不到）——与 api-reference 声称 v0.4.15「真机验证主线 381 删除（槽数 3→2）」矛盾。推测 quit 的 questId 与 activeQuest 的 ID 空间不同（任务槽 ID vs 任务 ID） | 排查 QUESTSYSTEM_Find 参数语义（activeQuest 与任务槽索引映射） | 本会话测试 2026-08-09 |
| 未开始 | **skill-usage 缺参 {} 返回 ok（未报 bad body）** | 2026-08-09 实测：`combat/0/config/skill-usage` 缺 body `{}` 返回 ok（api-reference 声称缺 body→bad body）；on=true/false 均 ok、role 越界 role not found 正确 | 核对 skill-usage 参数解析（on 缺省处理） | 本会话测试 2026-08-09 |
| ✅ v0.4.27 | **对话 API 整合方案（interact + get_content + select 一套 API）** | 用户 2026-08-09 设计。**v0.4.27 落地**：`POST /api/action/dialog/interact`（发起交互）+ `GET /api/info/dialog/content`（统一内容：story/npc/popup/none 四态 + options 选项列表）+ `POST /api/action/dialog/select`（action=next/skip/ok/cancel 或 index 选项选择）；剧情对话 skip/next 作为选项暴露；旧 npc/interact、npc/dialog/next、npc/dialog/select、npc/dialog/options 已移除 | — | 用户 2026-08-09 告知 + v0.4.27 |
| 未开始 | 商店购买/出售 | `UIStore_BuyItem`(0xd242c)/`SellItem`(0xd25f0) 需 ControlObject_GetCursor 选中态（依赖 UI） | 依赖 P1 商店数据结构完成后，探索底层购买/出售函数（绕过 cursor） | api-technical-spec §2.7 |
| 未开始 | 任务列表数据结构 | 仅 `QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID 可读；列表/状态/交付条件无记录 | hook `UIQuestMenu_ButtonClearExe`/`ButtonQuitExe` + 反汇编 QUESTSYSTEM | 本会话页面探索 |
| 未开始 | 静态表字段语义全逆向 | `field_catalog.json` 已验证 71 字段，其余待逆向 | 逐表解析（`*BASE_pData` + `record_index * nRecordSize`） | static-data §7 |
| 未开始 | 附魔属性对照表探索 | `~/Documents/Install/Android/Game/艾诺迪亚4_盗版大修_v1.3.2_20260704_v5.0/` 下 `附魔属性对照表1.xlsx`（附魔属性相关） | 解析 xlsx，整理附魔属性数据（用于强化/附魔数据校验） | 本会话用户提供 |
| 未开始 | 背包移动/整理 | `INVEN_MoveItem`(0x104934) 4 参签名复杂（item+3） | 逆向 4 参签名 + 真机验证 | control-capability §5 |
| 未开始 | 强化/镶嵌 | `ITEMSYSTEM_EnchantItem`(0x10b330)/`PutJewel`(0x10bcb4)/`ApplySocket`(0x10d8a4) 需物品+材料上下文 | 逆向执行路径（消耗校验） | control-capability §5.2 |
| 未开始 | 开箱 | `UIEquip_ButtonOpenBoxExe`/`ITEMSYSTEM_OpenItemBox` 未逆向 | 逆向 + 钥匙校验 | api-technical-spec §2.3 |
| 未开始 | 队友 AI 设置 | 队友自动控制决策选项（是否用技能/是否主动攻击），在技能界面设置；只需**读/写选项**，不关心内部运作 | 逆向 AI 选项数据结构（读写选项），做 `GET/POST /api/action/player/{role}/ai` | api-technical-spec §2.2 |
| 未开始 | 交互点 | 宝箱、恢复泉水等非敌人地图内容未探索 | 探索地图交互点数据（宝箱/泉水结构 + 交互函数） | 本会话决策 |
| 未开始 | 融合器/调合箱结构 | 配方表（Class D-S 五级）/材料合成链结构未逆向（网络资料见 game-systems §6.4） | 反汇编融合器/调合箱相关表结构 + 配方数据 | 用户 2026-08-08 指定 P2 |
| 未开始 | 佣兵技能系统 | 佣兵技能不出战也对全队生效、同种不叠加（game-systems §6.5）；数据结构未逆向 | 逆向佣兵技能表 + 全局生效逻辑 | 用户 2026-08-08 指定 P2 |
| 未开始 | 随机奖励的生成机制 | 掉落物/奖励如何生成（MakeItem 链）未探索 | 逆向 `ITEMSYSTEM_MakeItem` 系列 + 掉落表 | 本会话决策 |
| 未开始 | 休息（营地恢复） | `PARTY_ApplyRest`/`PARTY_GetRestCost` 未逆向 | 逆向 + 费用校验 | api-technical-spec §2.2 |
| 未开始 | NPC 交互数据结构 | npc_dialog 面板已识别（v0.3.9），但对话选项/分支结构未探 | hook `UINpc_*` 抓对话选项 + 反汇编 NPC 系统 | 本会话页面探索 |
| 未开始 | activeQuest 接任务后实测 | 未真机验证 | 真机接任务后对比 `QUESTSYSTEM_nActiveQuest` | api-reference §5 |
| 未开始 | mercenary inParty 标志与 party 槽一致性 | 2026-08-09 API 测试基线观察：snapshot 中 party 含西雷斯(role2) 但 mercenaries 中 slot1 西雷斯 `inParty=false`；另多个 `name=null` 槽 `inParty=true` | 核对 mercenary 槽标志位（flags bit1）与 party 成员映射（两套索引），确认 inParty 语义与 name 注入 | 本会话 API 测试 2026-08-09 |

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
| 未开始 | `/api/action/get-path` 真机验证 | v0.2.34 实现（原 /api/info/path，v0.3.13 迁至 /api/action/get-path POST） | 真机寻路对比 | api-reference §5 |
| 未开始 | 技能点重置 | `UISkill_ButtonSkillPointResetExe` 含 UIInAppProcess=内购 | 依赖内购 | api-technical-spec §2.5 |
| 未开始 | 复活 | `CHAR_ProcessReviveScroll`/`PARTY_AddHPMP`；角色死亡后复活选项 | 用不到（死亡重进即可），暂缓 | api-technical-spec §2.2 |
| 未开始 | 敌人 AI / 队友 AI 决策逻辑 | 决策算法本身（如何决策，非选项读写） | 麻烦且不影响正常游玩，暂缓 | 本会话决策 |
| ✅ v0.4.19 | **enter-slot 进 world 无 UI** | 根因：UIPlay_CallInAppShopProc(0xc7b64) 置 HUD 开关 [0x2f6000+0xc48]=0 + 弹 daily_reward + 触发 Hive 支付，hook 阻断后开关不恢复 → world 无 HUD 卡死 | 修复：hook 阻断 iapSelectTarget 后调 native 恢复（置 [0x2f5000+0xff8]=1 + [0x2f6000+0xc48]=1 + 清 daily_reward 栈）| 本会话 2026-08-09 |

## 待定区

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 待定 | 模拟器游戏本体启动 | 官方 Emulator API30 / Waydroid A13 + libndk 未实证 | PoC 启动测试 | emulator-research §5 |
| 待定 | 模块 arm64-v8a 打包后 guest 内 dlopen/dlsym | 转译层内 dlopen/dlsym 是否可用未实证（高风险） | PoC：模块仅 arm64-v8a → 游戏启动 → System.loadLibrary | emulator-research §5 |
| 待定 | LSPatch bootstrap 稳定性 | liblspatch.so x86_64 与 guest 进程混合无公开先例 | PoC | emulator-research §5 |
| 待定 | LSPatch 0.6 与 libxposed 101 兼容性 | 内置 runtime 较旧 | 必要时降级 API 93 构建 | README 已知待办 |
