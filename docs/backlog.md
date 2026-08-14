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

## P0 阻塞级

> 物品数据结构逆向为当前主线；存档导出为新增阻塞需求。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| ✅ 完成 | **物品数据结构逆向（品质/属性/词条/附魔/强化/宝石孔）** | ✅ **2026-08-12 核心完成 + 2026-08-14 全部子项闭环**（成果 docs/systems/inventory.md §2.4 + data-sources.md 修正）：物品位域全破解（+0x08 type bit2-5=稀有度/bit6-15=类别、+0x10 bit0-7=混沌等级/bit8-15=混沌值率/bit25-31=数量、+0x19 socket bit0-3=已镶/bit4-7=总孔、+0x1A bit0=混沌/bit6-10=附魔等级/bit11-15=附魔ID）；词缀体系（ITEM_AddOptionEx 0x105ec4 节点 bit0-6=索引+bit13-15=type、MakeOption 链候选筛选/值生成、ITEMOPTINFOBASE 12B 记录、词缀名 1114-1150 全量确认）；品质（ITEMSYSTEM_GetRarity 0x10d700、ITEMGRADEBASE 15 条品级前缀）；附魔强化（ITEMENCHANTBASE 32 条 9B、卷轴类别 16-25/946/947）；宝石孔（IsJewel [28,32]、PutJewel/ApplySocket 链）；ITEMSTATICOPTBASE 1409 条固定词条。**API 侧 v0.4.64 已接入 4 项 + v0.5.12 全部真机闭环**：① 词缀名注入（optionIds 索引数组 + optionNames/optionsDetailed，修复词缀名错位 + buildOptionNames 恒空）；② socket/enchant/混沌位域拆解输出（socketFilled/socketTotal/enchantId/enchantLevel/chaos/chaosLevel/chaosRate + socketInfo/enchantInfo/chaosInfo）；③ 装备路径品质前缀（equipOverride 不依赖 count，v0.5.12 修复 raw_rarity 前缀错位）；④ 附魔名注入（enchantName：ITEMENCHANTBASE→ITEMDATABASE 卷轴名）；⑤ count 语义（ITEM_GetCumulateCount 0x106094：可堆叠读 bit25-31/装备返回 1，真机药水=5/装备=1）；⑥ 稀有度档位（GetRarity 0-4→白绿蓝黄紫，rarity_tier 注入）；⑦ ITEMSTATICOPTBASE 静态词条（5B 记录 +2 u16 低字节=词缀索引 0-35，static_options 注入，package_assets 已含表）；⑩ 强化执行端点（EnchantItem 0x10b330 + IsEnchantScroll 0x10b2f0，卷轴消耗 INVEN_ConsumeItem 0x1047bc，真机强化成功+卷轴 3→2；PutJewel 已 v0.4.6；ApplySocket 仅装备生成时调用非用户操作）。**归并**：⑧ 地面掉落实体结构→P2 掉落系统前置；⑨ 附魔属性对照表（xlsx 数据位置已定：ITEMENCHANTBASE+词缀索引，L 列博主数据不保证正确无需全解析）；⑪ 物品效果链 use-item（操作侧）；⑫ A3 强化/混沌机制 + B1 装备表字段→P4 | ①-⑦⑩ 真机验证闭环（结论入 api-reference）；⑧⑨⑪ 归入对应待办；⑫ 归 P4 | 用户 2026-08-12 指定 P0 |
| ✅ 完成 | **/api/system/export_save_file 导出游戏存档** | ✅ **v0.5.12（2026-08-14）实现**：Kotlin 层扫描 applicationInfo.dataDir 一级子目录找 save{slot}.dat（目录名随 UID 变不可硬编码，实测 /data/user/0/<pkg>/fcea920f7412b5da7be0cf42b8c93759/save0.dat）；返回 base64 JSON {ok,slot,path,size,name,content}。SAVE_GetSaveFileName(0x125d08) 依赖 HubSave 云存档系统 frida 实测崩溃弃用。真机验证：slot=0 导出 3253B magic 293a1962 往返一致 / 越界 slot=5 {"error":"slot must be 0-2"} / 不存在 {"error":"save file not found"} | 已完成 | 用户 2026-08-14 指定 P0 |

## P1 高优先级

> 审计高优（稳定/一致性问题）+ 用户指定 P1 的功能与治理项。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 物品名映射错位检查（**2026-08-09 降为检查项**） | `StaticData.buildItemNames` 用 records 数组下标作 key，但查询传的是 category（ITEMDATABASE id 从 30 起）；**但本次会话实测 name 显示正常（恢复药水/低级宝石等均正确）——需核实是否已修复或复现条件**（可能 buildItemNames 与查询双方都用 category？） | 核实 StaticData.itemName(category) 查询链与 buildItemNames key 是否一致；真机验证 name 与物品一致 | 审计 H1 + 2026-08-09 检查 |
| 未开始 | 全代码库判空审查与修复（**2026-08-09 扩展自 fn_get_next_exp**） | 审计 H5 发现 `fn_get_next_exp` 漏判空（game_data.cpp:34）；**用户要求扩展到整个代码库**——所有函数指针调用（fn_*，194 处）/野指针读/NewStringUTF 返回值统一判空审查（含 game_data.cpp 中约 30 处疑似未判空调用点） | 系统性审查全部 fn_* 调用点与指针读，补判空；init 失败路径不崩溃 | 审计 H5 + 用户 2026-08-09 |
| ✅ v0.5.14 | **VMA 治理：裸 VMA 归拢 + dlsym 化改造** | ✅ **2026-08-14 v0.5.14 完成**：①裸 VMA 全量归拢（代码级 8 处 0x1506d8/0x14b858 → F_PANEL_WIPEOUT_ENTER/F_PANEL_NPC_QUEST_ENTER，注释级裸地址更新，补登记 F_CHAR_GET_BLOCK_VMA）；②新增 symbol_resolver.h/.cpp **自研 ELF 内存解析**（PT_DYNAMIC p_offset 定位 + SysV hash 查找 + SHN_UNDEF/STT_TLS 过滤 + PT_LOAD 范围校验防同名撞库，规避 Android linker namespace 隔离，无 dlsym 依赖）；③symbol_registry.h X-macro 注册表 135 条（宏名↔游戏符号名）单一来源，check_symbols.py 同步从注册表提取；④bridge_init 接入 resolver.attach，112 处 F_* 函数指针 ELF 解析优先、VMA 保底回退（g_symbol_report 记录 source） | 已完成（真机验证待 backlog 完成标准：需真机启动确认游戏正常） | 用户 2026-08-12 指定 P1 |
| 未开始 | **日志系统：LogFile 清理 + 为所有操作添加日志** | LogFile init() 死代码、write 无锁 + 每次 appendText 开闭文件（并发写行交错）；操作端点无统一操作日志，无法追溯调用链与参数 | 删 LogFile 死代码 + synchronized/缓冲 writer；所有 POST 操作端点统一记录（时间/端点/参数/结果/耗时），native 层关键函数加日志 | 用户 2026-08-12 指定 P1 + 审计 L3 |
| 未开始 | **info 端点主菜单下应报错（native 层检查 + API 诚实转发）** | 用户 2026-08-09 要求：current-map/party/mercenary/inventory/npc 在主菜单（非 world）下应报错并说明原因（如 `not in game`）；**报错在 native 数据函数调用时检查（与写操作 `game_in_world()` 一致）**，不在 API 端点代码中判断；API 遇到 native 错误诚实转发。现状：data_map/party/mercenaries/inventory/npc_dialog_options/_player_json 无状态检查，主菜单下返回假数据（mapId=0/x=-1/空槽位/name=null），不诚实。`data_snapshot_json` 独立实现（自读 g_state）不受影响。**用户确认（2026-08-09）：quest 复合端点随 data_player_json 一并报错** | ① native：上述 data_*_json 函数开头加 `if (!game_in_world()) return {"error":"not in game"}`；② Kotlin `InfoApiServiceImpl`：各方法解析 native 返回前检测 error 字段原样转发（不自行判断状态） | 用户 2026-08-09 告知 |
| 未开始 | data 大响应端点偶发 Connection reset | 2026-08-09 全量探测：`/api/world/maps/list`、`ITEMDATABASE`、`text`、`events` 大 JSON 响应偶发 `Connection reset by peer`（复测单发正常；`/api/system/tables` 小响应稳定） | 确认是否为 AndServer 大响应写超时/连接重置，必要时调大超时或分页 | 本会话全量探测 2026-08-09 |
| 未开始 | 各函数调用的前提探索 | 操作端点通用前置未系统验证 | 探索各操作的前提约束：是否任何界面都能保存？能否跳过确认弹窗直达操作（如直接退到主菜单）？——所有操作端点的通用前置 | 本会话决策 |
| 未开始 | 商店系统（数据结构 + 购买/出售） | UIStore 商品列表/价格表（DEALSYSTEM）未逆向；`UIStore_BuyItem`(0xd242c)/`SellItem`(0xd25f0) 需 ControlObject_GetCursor 选中态（依赖 UI） | 先反汇编 `DEALSYSTEM_FindSaleByID` + UIStore 初始化链；再探索底层购买/出售函数（绕过 cursor） | api-technical-spec §2.7 |
| 未开始 | 释放技能（⚠️ 需确认：P0 已记录 cast ✅v0.4.12） | `UISkill_SkillMainExe`/`UIPlay_ButtonSKill` 依赖 UI/快捷键状态（战斗价值最高）；P0 总条目已记录 cast 端点 ✅v0.4.12 真机验证——需确认是否仍缺底层技能释放（CHAR 技能使用链） | 确认现状后：探索底层技能释放函数（CHAR 技能使用链），做 `POST /api/action/player/{role}/cast` | api-technical-spec §2.2 |

## P2 中优先级

> 审计中优（并发/错误语义/参数校验）+ 用户实测问题与明确要求的功能 + 进行中的数据缺口（N3/Q2）。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | StaticData 缓存并发安全 | `cache` 为普通 HashMap，多客户端并发 GET /api/system/tables/* 竞争 | 换 ConcurrentHashMap（或 computeIfAbsent 原子化） | 审计 M1 |
| 未开始 | 写操作并发互斥 | 全部 POST 端点无锁，native 锁仅初始化用；并发 move/equip 并发调游戏函数，attach 快照 read-modify-write 竞态 | 写操作全局 ReentrantLock，操作与快照读取同锁 | 审计 M2 |
| 未开始 | native 调用前置 ready 检查 | controller 直接调 external，loadLibrary 失败抛 UnsatisfiedLinkError（Error 捕不到）→ 500 | 入口统一检查 `NativeBridge.ready`，未就绪返回 503 JSON | 审计 M4 |
| 未开始 | 错误响应语义统一（含参数错误 403 透传） | 失败全 HTTP 200 + 手写串/native 原串（"-1"）透传，无 400/404/500；2026-08-09 全量探测实测 `/api/character/party/abc` 等返回 **HTTP 403 + 原始 Java 异常串**（`java.lang.NumberFormatException`），违反 architecture §9.3（应 400 + JSON 错误体） | 统一 JSON 包装 + 状态码语义：路由参数解析处捕获 NumberFormatException 统一走 JsonUtil 错误响应；native 失败值转结构化错误 | 审计 M5/M6 |
| 未开始 | op_* 参数校验补齐 + OP 能力隔离 | teleport(x/y/map 无范围)、learn_action(actionId/level 任意、无技能点校验)、sell(price 无约束)、move(x/y 无上限)、exp 截断 int32 校验策略不一致；OP native 实现（money/exp/statuspoint/teleport/sell 任意定价）仅靠「不挂路由」隔离，无权限机制（原 P0 H3 挂靠） | 统一入口边界校验（坐标/槽位/枚举/价格≥0/int32 范围）；learn_action 先读技能点；OP 隔离：加全局开关（默认关闭）或移除，验证无 HTTP 路径可触发 | 审计 M6/M8 + 审计 H3 |
| 未开始 | 弹窗文本安全 | `G_POPUP_TEXT` 野指针读（256B 无校验）+ 手写转义弱于 json_escape | 指针有效性校验 + 复用 json_escape | 审计 M9 |
| 未开始 | DebugController 处置 | /api/debug/ui 未登记（architecture 表与 api-reference 均无），release 无排除 | 登记文档或 release 排除/鉴权 | 审计 M12 |
| 未开始 | 升级技能（技能点校验机制） | `CHAR_ProcessSkillBook`(0xe2488)（技能书路径）；`UISkill_ButtonUpExe` 依赖 UI；**实测问题**：2026-08-09（存档1 LV2）`character/skill {"actionId":80,"level":2}`（80=凯恩第一个真技能，治疗）成功升级但 **skillPoints 恒 1 未消耗**——与 api-reference 声称「学习技能消耗技能点」矛盾（可能改版机制/未校验） | 探索升级函数与技能点校验/扣减；与改版技能点机制对齐 | api-technical-spec §2.5 + 本会话测试 2026-08-09 |
| 未开始 | **声音/光效/语言设置操作 API** | 用户 2026-08-09 要求测试「主菜单环境设置」修改（声音/光效/语言各一次），**实测无对应操作端点**（底层已逆向，见 data-sources §2.7）：声音 `APPINFO_Set/GetSound`@0xd8538/0xd8528、音量 `APPINFO_Set/GetVolume`@0xd84e8/0xd84f0（开=6 关=0）、画质 bit2 置/清（child 2/3）、语言 `SGL_SetLanguage`@0x944f8 + UI 语言索引 `*(0x2f9000+0xf34)`（0-4 循环，0=简体中文） | 按设置项实现 `/api/action/settings/*` 或 options 类别端点（主菜单 SC_OPTION_MMENU 面板场景，不需 world） | 用户 2026-08-09 告知 |
| 未开始 | **创建新存档操作 API** | 用户 2026-08-09 确认未开发：无 new-game/创建存档端点；现只有 enter-slot（进已有存档，v0.4.18 用 SAVE_CreateSaveSlot 初始化槽区） | 探索新建存档链（SAVE_CreateSaveSlot + 角色初始创建 + 新手流程），实现 `/api/system/save/create` 或类似 | 用户 2026-08-09 告知 |
| 未开始 | **地图非敌人物件（出口指示符/宝箱/泉水）识别** | 用户 2026-08-09 实测：当前地图实际只有 2 个地图出口，units 输出 4 个「地图出口」（slot1-4，status=2）——**出口两侧各有指示符/标识物，名称同为「地图出口」（CHAR_GetName 来源）无法区分**；enemies 端点同样包含宝箱/泉水等非敌人物件（status==2 过滤语义）；宝箱/恢复泉水等交互点数据结构未探索 | 核对 status/type 语义或地图物件类型字段，区分真出口/指示符/交互物；enemies 过滤条件是否应排除非敌人（宝箱/泉水/出口）；探索交互点数据（宝箱/泉水结构 + 交互函数） | 本会话测试 2026-08-09 + 本会话决策 |
| 未开始 | **events 增强：被动触发事件** | 用户 2026-08-09 要求（保持现有差异检测逻辑，不做 since）：新增战斗/移动中被动触发的事件——**敌人数量变化、单位死亡、切换地图** 等（当前仅 money/inventory/move/hp/mp/level_up/exp） | 快照结构 Snapshot 增补字段（敌人数/单位死亡标志/地图 ID），diff 时输出新事件类型 | 用户 2026-08-09 告知 |
| 未开始 | **gameInfo 占位字段修正** | 用户 2026-08-09 要求：`InfoApiServiceImpl.gameInfo()` 中 `loggedIn`（null 占位）**删除**；`saveSlots`（空数组占位）改为**当前加载的存档槽位 int**（0/1/2） | 逆向「当前加载存档槽」内存位置（SAVE 链/存档上下文，data-sources §2.7 附近）；修改 gameInfo 返回 | 用户 2026-08-09 告知 |
| 未开始 | **基础动作 actionId 语义（0-7 普攻非技能 + 5/6/7 区别监听）** | 用户 2026-08-09 澄清：**0-7 号是普攻/基础动作（非技能，同 5/6/7 普攻），80 才是凯恩第一个技能（治疗）**；角色技能列表应从**职业（CHARCLASSBASE 等）**静态数据推导，而非仅读技能链表 actionId（此前把 0 号当「第一个技能」加点有误）；5/6/7 均为普攻（无 MP 消耗、同 -62 伤害、击杀怪），行为无法从 API 区分——需监听普攻按钮触发的 actionId 判断是连击段（依次使用）还是独立技能；SKILLDESCBASE 不含 0-7（基础技能无静态名） | API 技能端点（party/{slot}/skills）按职业静态表区分真技能与基础动作，文档修正 skills 语义；frida hook 普攻按钮/攻击键回调（UIPlay 攻击链）记录每次普攻的 actionId 序列 | 用户 2026-08-09 告知 |
| 未开始 | **attack/cast 行为确认（attack=锁定、cast5=普攻）** | 2026-08-09 实测确认（存档0 LV27，用户怀疑成立）：`combat/0/attack {"targetSlot":20}` 后**持续 10 秒不停止，目标怪 hp 恒 4095 无伤害**（仅怪物信息条出现=目标锁定）；对照 `cast 5`（普攻）立即 -403——api-reference 声称的 CHAR_MakeDefaultAttack 未触发实际攻击帧（仅让 AI 决策默认攻击），**普攻必须用 cast 5**。**用户决策（不改实现）**：attack 端点保持「仅锁定目标」语义（实际攻击链 = attack 锁定 → cast 5 普攻）；cast 保持现状（设置动作由 AI 帧执行，有效），仅完善校验。**新发现**：cast 5 后若不 stop，角色自动继续攻击（AI 自动连击）——「普攻一下」需 cast 5 后立即 stop | cast 完善校验（如 actionId 白名单/MP 校验）；文档记录「attack=锁定、cast5=普攻、不停止=自动连击」 | 本会话测试 2026-08-09 + 用户 2026-08-09 决策 |
| 未开始 | **skill-reset 未完全还原（基础技能等级保留）** | 2026-08-09 实测：skill-reset 后 0 号技能仍 LV2（未还原到 LV1），仅移除 80 号非基础技能；技能点 1→2 还原 | 确认 skill-reset 是否应还原基础技能等级（CHAR_InitializeSkill 语义） | 本会话测试 2026-08-09 |
| 未开始 | **mercenary/party 两套索引一致性（含 discharge 清理）** | 2026-08-09 实测（存档0）：`discharge slot1`（西雷斯在队）返回 ok 后 **mercenary 列表西雷斯消失但 party role2 西雷斯仍在**（hp=8184）——discharge 删 mercenary 登记但 party 角色实例未清理；基线观察：party 含西雷斯(role2) 但 mercenaries 中 slot1 西雷斯 `inParty=false`、多个 `name=null` 槽 `inParty=true`；exclude 确认沃尔达克=quest npc（`cannot exclude quest npc`）；discharge 边界正确（空槽 not found/quest npc 拦截/leader 拦截） | 核对 discharge（MERCENARYSYSTEM_Release）与 party 槽关联清理；核对 mercenary 槽标志位（flags bit1）与 party 成员映射（两套索引），确认 inParty 语义与 name 注入 | 本会话测试 2026-08-09 |
| 进行中 | **任务完成弹窗标题（<任务名>完成）未获取** | 任务完成弹窗有标题（<任务名>完成）+内容。**✅ v0.4.55 部分解决**：奖励内容（再生药水特大 X3）经 popup 态读取正常；标题字段（任务名+完成态）数据源在 UINpcQuest_MakeTextEndPopup(0xc3bd0)/DrawEndPopup(0xc32ec)，留待后续提取 | 已完成（内容）；标题字段留待后续 | 2026-08-12 v0.4.55 |
| 未开始 | 任务系统（列表结构 + 接取/交付） | 仅 `QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID 可读；列表/状态/交付条件无记录。**✅ v0.4.55 部分解决**：quest/list 返回槽数组（12B/槽+0 questId，双层解引用 [0x2f4000+0x3d0]）；quest 状态表（G_NPC_QUEST_STATE_GOT_VMA 三层解引用，0=未接 1=进行 2=可完成 3=已完成）与任务完成链已逆向；`QUESTSYSTEM_AcceptReivew`(0x125c70) 硬编码剧情任务 quest 489 非通用 | 任务描述/奖励/交付条件等其余字段留待后续；通用接取/交付函数（暂缓，见 P4） | 本会话页面探索 + api-technical-spec §2.9 |
| 未开始 | **掉落系统：掉落实体 + 生成链 + 权重表** | 2026-08-09 **测试完成**：击杀怪（slot9 狼 hp→0，exp+4902 确认）后，`/api/info/current-map/drops` 硬编码返回 `{"drops":[]}`（InfoApiServiceImpl.currentMapDrops 占位）、units 无掉落实体、events 无掉落事件——**掉落物当前无法通过任何 API 获取**（用户确认场景实际有大量掉落物）；**2026-08-12 研究进展**：掉落生成链已确认（CHARSYSTEM_Die 0xf5418→DropItem 0xf4d30，frida 实测 3 怪全触发）；MAPITEMSYSTEM_ProcessDrop 读 *(0x2f5000+0x5d8) 链表（实测空）；RemoveItem 反汇编得实体=0x20 步长数组 +0x08=物品type，计数[实例+0x818]；奖励/掉落生成链（ITEMSYSTEM_MakeItem 系列）与掉落表权重（分母 1000）未探索 | 逆向地面掉落实体结构（卡点：存储位置不在 MAPITEMSYSTEM 链表/CHARSYSTEM 池/CHARLOC 池，疑 EFFECTSYSTEM_ProcessDropItem 0xf828c）+ ITEMSYSTEM_MakeItem 生成链 + 掉落表权重，实现 drops 端点（**依赖 P0 物品逆向⑧**；来源含 api-reference §3.1） | 本会话测试 2026-08-09 + 用户 2026-08-08 指定 P2/A4 |
| 未开始 | **craft mix 合成器交互** | ⛔ 原 P0「完成 api-reference 端点」收敛剩余项（2026-08-14 抽出）：合成链已逆向（docs/systems/craft.md），需合成器交互验证（材料槽选中态/合成器上下文）；底层执行函数探索归 P4「合成执行」 | 合成器交互路径验证 + Service 层接线 + 真机验证 | api-technical-spec §2.8 + P0 端点收敛 |
| 进行中 | N3 MAXLEVELBASE 语义逆向 | 结构定案：48=6职业×8档，+0=职业索引(低字节 0,1,4,2,3,5)×档位(高字节 0,1,2,8,3,4,5,6)，+2=装备名 text_id（档6 仅职业0 有值）；运行时表与静态一致；语义=职业×等级档→装备，**引用点待定** | 符号 .bss 0x301620/0x301628/0x30162a；frida 运行时 dump 验证 | character-data-gaps.md S3 |
| 进行中 | Q2 任务进度运行时数据 | ✅ v0.5.5：/api/quest/active 返回已接任务列表（槽数组 12B/槽 +0 questId + G_NPC_QUEST_STATE state 表，实测 quest 180 state=1 + id_name 注入）；`progress.detail` 目标计数未逆向（槽 +2/+6 实测=0，任务 180 无计数；需计数型任务样本） | 计数型任务（猎杀 X 只）运行时槽/事件数据采样 | api-reference §5 |
| ✅ v0.5.9 修复 | **switch 切换主控未生效** | 根因：`fn_set_active_player`（PARTY_SetActivePlayer）只写 PLAYER_pActivePlayer，不同步 SAVE_nMainMercenarySlot → main_mercenary_slot/leader 不更新。**v0.5.9 修复**：data_op_switch_player 成功后补写 g_main_merc_slot；真机验证 slot0 自身 ok、无角色 slot1 返回 switch failed | 多成员档验证 main_mercenary_slot 变化（当前档仅 1 人） | 本会话测试 2026-08-09 + v0.5.9 修复 |
| ✅ v0.5.8 确认 | **inventory bag/{i}/{slot} 单格查询错位** | **已修复并真机验证**（InfoApiServiceImpl.bagSlot L293 按 `it.optInt("slot")==slot` 匹配，2026-08-13 方案已落地）：造空洞（bag0 slots=[0,2,3]）后查 bag/0/2 正确返回 slot2 物品、bag/0/1 空槽返回 not found | 无需再处理 | 本会话测试 2026-08-09 + v0.5.8 真机复验 |
| ✅ v0.5.9 修复 | **skill-usage 缺参 {} 返回 ok（未报 bad body）** | 根因：`o.optBoolean("on")` 缺省静默 false。**v0.5.9 修复**：auto-attack/skill-usage 均加 `!o.has("on")` → `on required`；真机验证 {}→on required、{"on":true}→ok | 无需再处理 | 本会话测试 2026-08-09 + v0.5.9 修复 |

## P3 低优先级

> 审计低优 + 原 P2 探索型（数据结构未逆向/需样本）+ 已有实现待验证项。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 版本与产物同步 | README/verification 已同步至 v0.3.13/48（2026-08-08）；仍缺 output v0.3.9-0.3.13 历史产物；需按 README 规则 6 走版本闭环 | 补建历史产物（可选）；版本闭环走 README 规则 6 | 审计 L1/L2 |
| 未开始 | HookMain 轮询容错 | context 未就绪/init 失败无限重试无终止条件；`nativeGetInitReport()` 无 try-catch 抛异常致 ApiServer 永不启动 | 最大重试/退避；initReport 包 try-catch | 审计 L4 |
| 未开始 | 输入白名单 | DataController `lang`/`tables/{name}` 直接拼路径无校验 | lang 白名单 + name 格式校验 `^[A-Z0-9]+$` | 审计 L7 |
| 未开始 | native 杂项清理 | 哨兵值 -1/0xFFFF/0/null 混用；CMakeLists 无 -Wall/-Wextra、dl 冗余；同址双常量 F_GET_EQUIP_VMA；g_inven 绕 resolve_global；g_party 死代码；json_escape 无长度上限；NewStringUTF 无异常检查；瓦片索引无列上界 | 统一哨兵约定；CMake 警告/标准；删冗余与死代码；补判空与上限 | 审计 L5/L8-L12 |
| 未开始 | 佣兵遣散（需确认与 party/discharge 关系） | `MERCENARYSYSTEM_Release`(0x118ab4) 未逆向；P0 已记录 party/discharge ✅v0.4.8——mercenary/discharge 是否同源需确认 | 逆向签名 + `POST /api/action/mercenary/discharge` | api-technical-spec §2.6，2026-08-08 降 P2 |
| 未开始 | 静态表字段语义全逆向（含 B1 装备表/B2 技能表/B3 怪物表） | `field_catalog.json` 已验证 71 字段，其余待逆向；B1 ITEMDATABASE/ITEMENCHANTBASE 字段（强化/耐久/宝石孔/附魔）、B2 90+ 技能公式参数（Z% 随等级）、B3 怪物属性/掉率/首领强化（等级+3 ATK×1.2 HP×3.6）均未全逆向（B1/B2/B3 原 P3 暂缓，其中 B1 与 P0 物品逆向⑫ 关联） | 逐表解析（`*BASE_pData` + `record_index * nRecordSize`），覆盖装备/技能/怪物表 | static-data §7 + 用户 2026-08-08 指定 P3 |
| 未开始 | 背包移动/整理 | `INVEN_MoveItem`(0x104934) 4 参签名复杂（item+3） | 逆向 4 参签名 + 真机验证 | control-capability §5 |
| 未开始 | 队友 AI 设置 | 队友自动控制决策选项（是否用技能/是否主动攻击），在技能界面设置；只需**读/写选项**，不关心内部运作 | 逆向 AI 选项数据结构（读写选项），做 `GET/POST /api/action/player/{role}/ai` | api-technical-spec §2.2 |
| 未开始 | 融合器/调合箱结构 | 配方表（Class D-S 五级）/材料合成链结构未逆向（网络资料见 game-systems §6.4） | 反汇编融合器/调合箱相关表结构 + 配方数据 | 用户 2026-08-08 指定 P2 |
| 未开始 | 佣兵技能系统 | 佣兵技能不出战也对全队生效、同种不叠加（game-systems §6.5）；数据结构未逆向 | 逆向佣兵技能表 + 全局生效逻辑 | 用户 2026-08-08 指定 P2 |
| 未开始 | 休息（营地恢复） | `PARTY_ApplyRest`/`PARTY_GetRestCost` 未逆向 | 逆向 + 费用校验 | api-technical-spec §2.2 |
| 未开始 | NPC 交互数据结构 | npc_dialog 面板已识别（v0.3.9），但对话选项/分支结构未探 | hook `UINpc_*` 抓对话选项 + 反汇编 NPC 系统 | 本会话页面探索 |
| 未开始 | activeQuest 接任务后实测 | 未真机验证（依赖 P2 任务系统已实现的 quest 端点） | 真机接任务后对比 `QUESTSYSTEM_nActiveQuest` | api-reference §5 |
| 未开始 | `/api/world/movement/path` 真机验证 | v0.2.34 实现（原 /api/info/path，v0.3.13 迁至 /api/action/get-path，v0.4.29 重做为 /api/world/movement/path 自研 BFS） | 真机寻路对比 | api-reference §5 |
| 未开始 | N6 槽记录→角色对象指针偏移 | R4 槽数组/角色池结构已确认；槽记录字段 → 角色对象指针偏移关系待样本 | 槽数据样本（多佣兵存档）frida dump 验证 | character-data-gaps.md R4 |
| 未开始 | Q4 主线/支线任务区分 | active 需 `is_mainline` 字段；QUESTINFOBASE 记录无明确标志（u16[13] 小整数 0-7 疑似任务类型/链，未定性） | 逆向任务类型字段（QUESTSYSTEM 链/任务章节标志），确定主线/支线判定规则 | api-reference §5 |
| 未开始 | S8 text/story-events 并入 tables 适配 | text（带 lang 参数）与 story-events 为特殊表，需在 tables 端点适配（lang 参数传递/特殊表路由） | 实现特殊表分发逻辑 | api-reference §7.4 |

## P4 暂缓 / 待定

> 深度数值逆向（A/C 系列，原 P3 暂缓）、依赖内购/高风险项、占位文档项、模拟器/转译层待定项。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | A1 伤害公式逆向 | 属性系数(0.66/0.5)×1.2、逐项取整、技能公式 Z%（参照 game-systems §6.1） | 反汇编找 ×1.2/0.66 常量 + 取整逻辑；突破口：布甲 12% 魔攻加成 | 用户 2026-08-08 指定 P3 |
| 未开始 | A2 经验/成长曲线 | 105 级经验表、升级规则（参照 game-systems §6.2） | 对比经验表/成长曲线公式 | 用户 2026-08-08 指定 P3 |
| 未开始 | A3 强化/混沌机制 | 卷轴增量、混沌±50%/深渊±100%、宝石上限(CRT 6.1/11.1/17)（参照 §6.3） | 反汇编强化计算 + 混沌随机常量 + 上限表（P0 物品逆向⑫ 引用） | 用户 2026-08-08 指定 P3 |
| 未开始 | C2 元素属性系统 | 风火冰神圣暗黑毒伤害判定（待确认游戏是否含此系统） | 确认存在性后再探索 | 用户 2026-08-08 指定 P3 |
| 未开始 | C4 悬赏任务/无限地下城 | Bounty Hunter/5-6 层地下城结构（待确认） | 确认存在性后探索 | 用户 2026-08-08 指定 P3 |
| 未开始 | 合成执行 | `UIMix_ButtonMixingExe`(0xc21ec) 依赖材料槽选中态；`MIXSYSTEM_CheckMixture` 仅检查非执行 | 探索 `MIXSYSTEM_*` 底层执行函数 + 材料上下文构造 | api-technical-spec §2.8 |
| 未开始 | 读档 | `SAVE_Load*`/`GAMELOADER`（主菜单操作） | 风险高，暂缓 | api-technical-spec §2.10 |
| 未开始 | 技能点重置 | `UISkill_ButtonSkillPointResetExe` 含 UIInAppProcess=内购 | 依赖内购 | api-technical-spec §2.5 |
| 未开始 | 复活 | `CHAR_ProcessReviveScroll`/`PARTY_AddHPMP`；角色死亡后复活选项 | 用不到（死亡重进即可），暂缓 | api-technical-spec §2.2 |
| 未开始 | 敌人 AI / 队友 AI 决策逻辑 | 决策算法本身（如何决策，非选项读写） | 麻烦且不影响正常游玩，暂缓 | 本会话决策 |
| 未开始 | S6 help 帮助文档内容 | `/api/system/help` 与 `/api/system/download` 为占位 | 提供帮助文档内容（API 概览/示例）与文件格式 | api-reference §7.5 |
| 未开始 | S7 tables/{table}/download 与 /api/system/download | 两个 download 端点已决定**先占位**（暂不实现） | 后续需要时实现文件流输出 | api-reference §7.4/§7.5 |
| 待定 | 模拟器游戏本体启动 | 官方 Emulator API30 / Waydroid A13 + libndk 未实证 | PoC 启动测试 | emulator-research §5 |
| 待定 | 模块 arm64-v8a 打包后 guest 内 dlopen/dlsym | 转译层内 dlopen/dlsym 是否可用未实证（高风险；与 P1 VMA 治理的 dlsym 化关联） | PoC：模块仅 arm64-v8a → 游戏启动 → System.loadLibrary | emulator-research §5 |
| 待定 | LSPatch bootstrap 稳定性 | liblspatch.so x86_64 与 guest 进程混合无公开先例 | PoC | emulator-research §5 |
| 待定 | LSPatch 0.6 与 libxposed 101 兼容性 | 内置 runtime 较旧 | 必要时降级 API 93 构建 | README 已知待办 |
