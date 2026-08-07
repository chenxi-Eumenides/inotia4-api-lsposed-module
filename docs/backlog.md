# 开发待办总清单（Backlog）

> 日期：2026-08-08 ｜ 集中收录全部文档中"待实现/未验证/暂缓"项，作为唯一开发待办来源。
> 来源文档：player-operations.md / control-capability.md / api-spec.md / data-sources.md / game-systems.md / static-data.md / emulator-research.md / 本会话页面探索结论。
> 优先级为 2026-08-07 重新分配（按通用性 + 收益 + 探索难度综合），模拟器相关归待定区。
> 2026-08-08：P0 两项（API 分层重构 v0.3.13 / 帧率探索）已完成；用户重新分配：单位敌人信息/改版说明图片/SYSTEMMENU/队伍换位提 P0，佣兵遣散/升级技能降 P2。

## 完成标准（强制）

- **只有真机验证通过才算完成**：实现后必须在真机（100.110.139.83:5555，游戏运行中）验证行为符合预期且无崩溃。
- **验证结论写入对应主题文档**（产出类型 → 归属）：
  - 写操作函数签名/VMA/调用机制 → `docs/control-capability.md`
  - 数据结构/偏移/全局 VMA → `docs/data-sources.md`
  - 操作分级判定/实现状态 → `docs/player-operations.md`
  - 端点规格（路由/参数/返回）→ `docs/api-spec.md`
  - 静态表字段语义 → `docs/reference/static-data.md`
  - UI 点击坐标 → `docs/reference/ui-click-coordinates.md`
  - 部署/模拟器结论 → `docs/deployment/emulator-research.md` 或 `docs/deployment/phone-dev-workflow.md`
- **未真机验证 = 未完成**，状态标为 `待真机验证` 而不是完成。
- **完成一项后删除该条**，不留历史；新缺口随时在此追加。

状态标记：未开始 / 探索中 / 实现中 / 待真机验证

---

## 代码审计修复项（2026-08-08 全量代码审计）

> 来源：2026-08-08 代码审计（Kotlin 层 + native 层 + 文档一致性，审计报告见会话记录）。
> 一次性修复任务，与下方游戏逆向探索类待办独立；完成标准同「完成标准（强制）」。
> 持续性规范（编码时必须遵守的规则）已沉淀至 `architecture.md` §9，不在此重复。

### 审计修复·高优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 物品名映射错位修复 | `StaticData.buildItemNames` 用 records 数组下标作 key，但查询传的是 category；ITEMDATABASE id 从 30 起（实证 1018/1018 条错位），/inventory /party /snapshot 的 name 字段全错 | 改用记录内 id 字段（u16[0]）建键；真机验证 name 与物品一致 | 审计 H1 |
| 未开始 | HTTP 鉴权 + 网卡绑定 | ApiServer 监听 0.0.0.0:8088 零鉴权，局域网任意设备可读写（含 14 个写端点与 dialog 确认） | AndServer 加 token 中间件（Interceptor）+ `inetAddress()` 绑网卡/白名单；未带 token 返回 401/403 | 审计 H2 |
| 未开始 | OP 能力隔离机制 | native/JNI 的 money/exp/statuspoint/teleport/sell（任意定价）已实现，仅靠「不挂路由」隔离，无权限机制 | 加全局开关（默认关闭）或移除 OP native 实现；验证无任何 HTTP 路径可触发 | 审计 H3 |
| 未开始 | events 快照线程安全 | `data_events_json` 局部 static `last`/`has_last` 无锁，多客户端并发轮询丢事件/数据竞争 | 提升为文件级 + `std::mutex` 保护 diff 过程；多客户端并发轮询验证事件不丢 | 审计 H4 |
| 未开始 | `fn_get_next_exp` 判空补漏 | `game_data.cpp:34` 全文件唯一漏判空的函数指针调用，init 部分失败即 SIGSEGV 崩溃 | 独立判空后调用；init 失败路径不崩溃 | 审计 H5 |
| 未开始 | 裸地址入符号表 | game_data.cpp 34 个裸 VMA（debug UI 12 + 面板识别 22，含 1 个与 G_POPUP_FPCANCEL_VMA 重复），换版本静默失效 | 全部入 game_symbols.h（G_*_VMA/F_*_PANEL_*）并登记 check_symbols.py 清单；check_symbols 通过 | 审计 H6 |

### 审计修复·中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | StaticData 缓存并发安全 | `cache` 为普通 HashMap，多客户端并发 GET /api/data/* 竞争 | 换 ConcurrentHashMap（或 computeIfAbsent 原子化） | 审计 M1 |
| 未开始 | 写操作并发互斥 | 全部 POST 端点无锁，native 锁仅初始化用；并发 move/equip 并发调游戏函数，attach 快照 read-modify-write 竞态 | 写操作全局 ReentrantLock，操作与快照读取同锁 | 审计 M2 |
| 未开始 | native 调用前置 ready 检查 | controller 直接调 external，loadLibrary 失败抛 UnsatisfiedLinkError（Error 捕不到）→ 500 | 入口统一检查 `NativeBridge.ready`，未就绪返回 503 JSON | 审计 M4 |
| 未开始 | 错误响应语义统一 | 失败全 HTTP 200 + 手写串/native 原串（"-1"）透传，无 400/404/500 | 统一 JSON 包装 + 状态码语义；native 失败值转结构化错误 | 审计 M5 |
| 未开始 | op_* 参数校验补齐 | teleport(x/y/map 无范围)、learn_action(actionId/level 任意、无技能点校验)、sell(price 无约束)、move(x/y 无上限)、exp 截断 int32 校验策略不一致 | 统一入口边界校验（坐标/槽位/枚举/价格≥0/int32 范围）；learn_action 先读技能点 | 审计 M6/M8 |
| 未开始 | 弹窗文本安全 | `G_POPUP_TEXT` 野指针读（256B 无校验）+ 手写转义弱于 json_escape | 指针有效性校验 + 复用 json_escape | 审计 M9 |
| 未开始 | DebugController 处置 | /api/debug/ui 未登记（architecture 表与 api-spec 均无），release 无排除 | 登记文档或 release 排除/鉴权 | 审计 M12 |

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

> 已全部完成（2026-08-08 实机验证）：弹窗按钮文本 / 敌人坐标 / 瓦片通行矩阵 / 游戏系统探索（结论见 data-sources.md / api-spec.md / game-systems.md）、API 分层重构 v0.3.13（见 api-spec §0/§4）、游戏逻辑帧率探索 16.9fps（见 data-sources §3.5）。
> 2026-08-08 用户重新分配：P1 四项提入 P0（单位敌人信息/改版说明图片/SYSTEMMENU/队伍换位）。

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | **单位实时坐标+敌人信息增强** | units 端点已输出坐标/type/status；敌人信息（名字/等级/HP/MP）未输出 | units 增补 level(C_LEVEL 0x0E)/hp(C_HP 0x1F0)/mp(C_MP 0x1F4)；名字用 CHAR_GetName(0xd9c54)（怪物可能空）；数据源已 v0.3.12 实测验证（敌人 lv=1 hp=792） | 用户 2026-08-08 提 P0 |
| 未开始 | **改版说明图片探索** | `~/Documents/Install/Android/Game/艾诺迪亚4_盗版大修_v1.3.2_20260704_v5.0/` 下两张 jpg（附件20260704_v4.2 + 说明20260704_v5.0）为改版作者对原版游戏的修改介绍 | 读取图片内容，梳理改版相对原版的修改点（影响数据结构/机制判断） | 本会话用户提供，2026-08-08 提 P0 |
| 未开始 | **SYSTEMMENU 选项页结构** | 设置项/存档槽 UI 结构完全无逆向记录（最大空白页） | frida 枚举 UI 状态变量 + SAVE 调用链 | 本会话页面探索，2026-08-08 提 P0 |
| 未开始 | **队伍换位** | `PARTY_Swap`(0x11ff5c) `void(int32,int32)` 已确认签名 | 实现 `POST /api/action/party/swap`（真机验证边界） | player-operations §2.6，2026-08-08 提 P0 |

## P1 中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 各函数调用的前提探索 | 操作端点通用前置未系统验证 | 探索各操作的前提约束：是否任何界面都能保存？能否跳过确认弹窗直达操作（如直接退到主菜单）？——所有操作端点的通用前置 | 本会话决策 |
| 未开始 | 移动端点回归 | v0.3.12 实测可用：`move→(168,528)` 成功；此前 `no path` 为目标坐标在墙内（不可达），非端点 bug | 无需修复；若后续失效（如换地图/控制态变化）需诊断 SearchPath 路径 | 用户 2026-08-08 指定 P1 |
| 未开始 | 商店物品/价格数据结构 | UIStore 商品列表/价格表（DEALSYSTEM）未逆向 | 反汇编 `DEALSYSTEM_FindSaleByID` + UIStore 初始化链 | 商店买卖前置依赖 |
| 未开始 | 释放技能 | `UISkill_SkillMainExe`/`UIPlay_ButtonSKill` 依赖 UI/快捷键状态（战斗价值最高） | 探索底层技能释放函数（CHAR 技能使用链），做 `POST /api/action/player/{role}/cast` | player-operations §2.2 |
| 未开始 | 手动存档 `/api/save` | `SAVE_Save`(0x129600) 依赖存档上下文 `[x0+0x8c0]`；`SAVE_ProcessSave`/`SaveData` 确认不可直接调用 | 逆向 SAVE_Save 完整签名/上下文，或探索 `UIPlay_CallSave` 触发路径 | control-capability §5.2 |

## P2 低优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 加点分配函数 | `CHAR_SetStatusPoint`(0xd9c4c) 是"设剩余点数"（OP 语义），真正"属性+1/能力点-1"分配函数未记录 | frida hook 人物属性页加点按钮抓底层调用；找到后做 `POST /api/action/player/{role}/stat`（≤剩余点校验 = 普通） | 本会话页面探索 |
| 未开始 | 佣兵遣散 | `MERCENARYSYSTEM_Release`(0x118ab4) 未逆向 | 逆向签名 + `POST /api/action/mercenary/discharge` | player-operations §2.6，2026-08-08 降 P2 |
| 未开始 | 升级技能 | `CHAR_ProcessSkillBook`(0xe2488)（技能书路径）；`UISkill_ButtonUpExe` 依赖 UI | 探索升级函数与技能点校验 | player-operations §2.5，2026-08-08 降 P2 |
| 未开始 | 商店购买/出售 | `UIStore_BuyItem`(0xd242c)/`SellItem`(0xd25f0) 需 ControlObject_GetCursor 选中态（依赖 UI） | 依赖 P1 商店数据结构完成后，探索底层购买/出售函数（绕过 cursor） | player-operations §2.7 |
| 未开始 | 任务列表数据结构 | 仅 `QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID 可读；列表/状态/交付条件无记录 | hook `UIQuestMenu_ButtonClearExe`/`ButtonQuitExe` + 反汇编 QUESTSYSTEM | 本会话页面探索 |
| 未开始 | 静态表字段语义全逆向 | `field_catalog.json` 已验证 71 字段，其余待逆向 | 逐表解析（`*BASE_pData` + `record_index * nRecordSize`） | static-data §7 |
| 未开始 | 附魔属性对照表探索 | `~/Documents/Install/Android/Game/艾诺迪亚4_盗版大修_v1.3.2_20260704_v5.0/` 下 `附魔属性对照表1.xlsx`（附魔属性相关） | 解析 xlsx，整理附魔属性数据（用于强化/附魔数据校验） | 本会话用户提供 |
| 未开始 | 背包移动/整理 | `INVEN_MoveItem`(0x104934) 4 参签名复杂（item+3） | 逆向 4 参签名 + 真机验证 | control-capability §5 |
| 未开始 | 强化/镶嵌 | `ITEMSYSTEM_EnchantItem`(0x10b330)/`PutJewel`(0x10bcb4)/`ApplySocket`(0x10d8a4) 需物品+材料上下文 | 逆向执行路径（消耗校验） | control-capability §5.2 |
| 未开始 | 开箱 | `UIEquip_ButtonOpenBoxExe`/`ITEMSYSTEM_OpenItemBox` 未逆向 | 逆向 + 钥匙校验 | player-operations §2.3 |
| 未开始 | 队友 AI 设置 | 队友自动控制决策选项（是否用技能/是否主动攻击），在技能界面设置；只需**读/写选项**，不关心内部运作 | 逆向 AI 选项数据结构（读写选项），做 `GET/POST /api/action/player/{role}/ai` | player-operations §2.2 |
| 未开始 | 交互点 | 宝箱、恢复泉水等非敌人地图内容未探索 | 探索地图交互点数据（宝箱/泉水结构 + 交互函数） | 本会话决策 |
| 未开始 | 融合器/调合箱结构 | 配方表（Class D-S 五级）/材料合成链结构未逆向（网络资料见 game-systems §6.4） | 反汇编融合器/调合箱相关表结构 + 配方数据 | 用户 2026-08-08 指定 P2 |
| 未开始 | 佣兵技能系统 | 佣兵技能不出战也对全队生效、同种不叠加（game-systems §6.5）；数据结构未逆向 | 逆向佣兵技能表 + 全局生效逻辑 | 用户 2026-08-08 指定 P2 |
| 未开始 | 随机奖励的生成机制 | 掉落物/奖励如何生成（MakeItem 链）未探索 | 逆向 `ITEMSYSTEM_MakeItem` 系列 + 掉落表 | 本会话决策 |
| 未开始 | 休息（营地恢复） | `PARTY_ApplyRest`/`PARTY_GetRestCost` 未逆向 | 逆向 + 费用校验 | player-operations §2.2 |
| 未开始 | NPC 交互数据结构 | npc_dialog 面板已识别（v0.3.9），但对话选项/分支结构未探 | hook `UINpc_*` 抓对话选项 + 反汇编 NPC 系统 | 本会话页面探索 |
| 未开始 | activeQuest 接任务后实测 | 未真机验证 | 真机接任务后对比 `QUESTSYSTEM_nActiveQuest` | api-spec §7 |

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
| 未开始 | 任务接取/交付 | `QUESTSYSTEM_AcceptReivew`(0x125c70) 硬编码剧情任务 quest 489，非通用 | 依赖 P2 任务列表结构，再找通用接/交函数 | player-operations §2.9 |
| 未开始 | 合成执行 | `UIMix_ButtonMixingExe`(0xc21ec) 依赖材料槽选中态；`MIXSYSTEM_CheckMixture` 仅检查非执行 | 探索 `MIXSYSTEM_*` 底层执行函数 + 材料上下文构造 | player-operations §2.8 |
| 未开始 | 读档 | `SAVE_Load*`/`GAMELOADER`（主菜单操作） | 风险高，暂缓 | player-operations §2.10 |
| 未开始 | `/api/action/get-path` 真机验证 | v0.2.34 实现（原 /api/info/path，v0.3.13 迁至 /api/action/get-path POST） | 真机寻路对比 | api-spec §7 |
| 未开始 | 技能点重置 | `UISkill_ButtonSkillPointResetExe` 含 UIInAppProcess=内购 | 依赖内购 | player-operations §2.5 |
| 未开始 | 复活 | `CHAR_ProcessReviveScroll`/`PARTY_AddHPMP`；角色死亡后复活选项 | 用不到（死亡重进即可），暂缓 | player-operations §2.2 |
| 未开始 | 敌人 AI / 队友 AI 决策逻辑 | 决策算法本身（如何决策，非选项读写） | 麻烦且不影响正常游玩，暂缓 | 本会话决策 |

## 待定区（暂不开发，保留记录）

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 待定 | 模拟器游戏本体启动 | 官方 Emulator API30 / Waydroid A13 + libndk 未实证 | PoC 启动测试 | emulator-research §5 |
| 待定 | 模块 arm64-v8a 打包后 guest 内 dlopen/dlsym | 转译层内 dlopen/dlsym 是否可用未实证（高风险） | PoC：模块仅 arm64-v8a → 游戏启动 → System.loadLibrary | emulator-research §5 |
| 待定 | LSPatch bootstrap 稳定性 | liblspatch.so x86_64 与 guest 进程混合无公开先例 | PoC | emulator-research §5 |
| 待定 | LSPatch 0.6 与 libxposed 101 兼容性 | 内置 runtime 较旧 | 必要时降级 API 93 构建 | README 已知待办 |
