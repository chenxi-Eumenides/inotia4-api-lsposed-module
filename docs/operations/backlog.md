# 开发待办总清单（Backlog）

> 日期：2026-08-07 ｜ 集中收录全部文档中"待实现/未验证/暂缓"项，作为唯一开发待办来源。
> 来源文档：player-operations.md / control-capability.md / api-spec.md / hook-points.md / game-systems.md / static-data.md / emulator-research.md / 本会话页面探索结论。

## 完成标准（强制）

- **只有真机验证通过才算完成**：实现后必须在真机（100.110.139.83:5555，游戏运行中）验证行为符合预期且无崩溃。
- **验证结论写入对应主题文档**：操作类写入 `docs/operations/player-operations.md`（或 api-spec.md 端点表），数据类写入 `docs/reference/hook-points.md`，UI/坐标类写入 `docs/reference/ui-click-coordinates.md`，部署类写入 `docs/deployment/emulator-research.md`。
- **未真机验证 = 未完成**，状态标为 `待真机验证` 而不是完成。
- **完成一项后删除该条**，不留历史；新缺口随时在此追加。

状态标记：未开始 / 探索中 / 实现中 / 待真机验证

---

## P0 高优先级（核心操作，真机已具备验证条件）

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | **加点分配函数** | `CHAR_SetStatusPoint`(0xd9c4c) 是"设剩余点数"（OP 语义），真正"属性+1/能力点-1"分配函数未记录 | frida hook 人物属性页加点按钮抓底层调用；找到后做 `POST /api/action/player/{role}/stat`（≤剩余点校验 = 普通） | 本会话页面探索 |
| 未开始 | 商店购买/出售 | `UIStore_BuyItem`(0xd242c)/`SellItem`(0xd25f0) 需 ControlObject_GetCursor 选中态（依赖 UI） | 探索商店底层购买/出售函数（绕过 cursor 依赖），或状态模拟 | player-operations §4.2 P0 |
| 未开始 | 释放技能 | `UISkill_SkillMainExe`/`UIPlay_ButtonSKill` 依赖 UI/快捷键状态（战斗价值最高） | 探索底层技能释放函数（CHAR 技能使用链），做 `POST /api/action/player/{role}/cast` | player-operations §4.2 P1（提升） |

## P1 中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 任务接取/交付 | `QUESTSYSTEM_AcceptReivew`(0x125c70) 硬编码剧情任务 quest 489，非通用 | 先破任务列表数据结构（见数据层），再找通用接/交函数 | player-operations §4.2 P1 |
| 未开始 | 合成执行 | `UIMix_ButtonMixingExe`(0xc21ec) 依赖材料槽选中态；`MIXSYSTEM_CheckMixture` 仅检查非执行 | 探索 `MIXSYSTEM_*` 底层执行函数 + 材料上下文构造 | player-operations §4.2 P1 |
| 未开始 | 背包移动/整理 | `INVEN_MoveItem`(0x104934) 4 参签名复杂（item+3） | 逆向 4 参签名 + 真机验证 | hook-points/control-capability §5 |
| 未开始 | 手动存档 `/api/save` | `SAVE_Save`(0x129600) 依赖存档上下文 `[x0+0x8c0]`；`SAVE_ProcessSave`/`SaveData` 确认不可直接调用 | 逆向 SAVE_Save 完整签名/上下文，或探索 `UIPlay_CallSave` 触发路径 | control-capability §4/§5.2 |
| 未开始 | 佣兵遣散 | `MERCENARYSYSTEM_Release`(0x118ab4) 未逆向 | 逆向签名 + `POST /api/action/mercenary/discharge` | player-operations §2.6 P1 |
| 未开始 | 队伍换位 | `PARTY_Swap`(0x11ff5c) `void(int32,int32)` 已确认签名 | 实现 `POST /api/action/party/swap`（真机验证边界） | player-operations §2.6 P1 |
| 未开始 | 升级技能 | `CHAR_ProcessSkillBook`(0xe2488)（技能书路径）；`UISkill_ButtonUpExe` 依赖 UI | 探索升级函数与技能点校验 | player-operations §2.5 P1 |

## P2 低优先级（待逆向）

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 强化/镶嵌 | `ITEMSYSTEM_EnchantItem`(0x10b330)/`PutJewel`(0x10bcb4)/`ApplySocket`(0x10d8a4) 需物品+材料上下文 | 逆向执行路径（消耗校验） | control-capability §5.2 |
| 未开始 | 开箱 | `UIEquip_ButtonOpenBoxExe`/`ITEMSYSTEM_OpenItemBox` 未逆向 | 逆向 + 钥匙校验 | player-operations §2.3 P1 |
| 未开始 | 鉴定掷骰 | `UIEquip_ButtonRollDiceExe` 依赖 UI | 逆向底层 | player-operations §2.3 P2 |
| 未开始 | 解封装备/技能书 | `ITEMSYSTEM_ReleaseSealed`/`ReleaseSealedSkillBook` 未逆向 | 逆向 | player-operations §2.3 P2 |
| 未开始 | 技能点重置 | `UISkill_ButtonSkillPointResetExe` 含 UIInAppProcess=内购 | 依赖内购，暂缓 | player-operations §2.5 P2 |
| 未开始 | AI 模式设置 | `UISkill_ButtonAIExe`/`UISkill_MakeAIInfo` 依赖 UI | 逆向 AI 数据结构 | player-operations §2.2 P1 |
| 未开始 | 休息/复活 | `PARTY_ApplyRest`/`GetRestCost`、`CHAR_ProcessReviveScroll`/`PARTY_AddHPMP` 未逆向 | 逆向 + 费用校验 | player-operations §2.2 P1 |
| 未开始 | 读档 | `SAVE_Load*`/`GAMELOADER`（主菜单操作） | 风险高，暂缓 | player-operations §2.10 P2 |

## 数据层（待探索，均为读内存缺口）

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | **任务列表数据结构** | 仅 `QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID 可读；列表/状态/交付条件无记录 | hook `UIQuestMenu_ButtonClearExe`/`ButtonQuitExe` + 反汇编 QUESTSYSTEM | 本会话页面探索 / player-operations §4.2 |
| 未开始 | **SYSTEMMENU 选项页结构** | 设置项/存档槽 UI 结构完全无逆向记录（最大空白页） | frida 枚举 UI 状态变量 + SAVE 调用链 | 本会话页面探索 |
| 未开始 | 敌人坐标字段偏移 | `CHARLOCSYSTEM_pPool` 单位位置字段偏移待逆向 | 反汇编 CHARLOCSYSTEM_* + frida 运行时探测 | game-systems §4.5 |
| 未开始 | 地图瓦片编码 | `MAP_nBaseTile`(0x7148a8, 8192B) 瓦片编码格式待逆向 | 反汇编瓦片解析 + 真机对照 | game-systems / api-spec §6 |
| 未开始 | 静态表字段语义全逆向 | `field_catalog.json` 已验证 71 字段，其余待逆向 | 逐表解析（`*BASE_pData` + `record_index * nRecordSize`） | static-data §7 |
| 未开始 | activeQuest 接任务后实测 | 未真机验证 | 真机接任务后对比 `QUESTSYSTEM_nActiveQuest` | api-spec §7 |
| 未开始 | `/api/info/path` 真机验证 | v0.2.34 实现，待真机确认 | 真机寻路对比 | api-spec §7 |

## 部署 / 环境（PoC 待验证）

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 模拟器游戏本体启动 | 官方 Emulator API30 / Waydroid A13 + libndk 未实证 | PoC 启动测试 | emulator-research §5 |
| 未开始 | 模块 arm64-v8a 打包后 guest 内 dlopen/dlsym | 转译层内 dlopen/dlsym 是否可用未实证（高风险） | PoC：模块仅 arm64-v8a → 游戏启动 → System.loadLibrary | emulator-research §5 |
| 未开始 | LSPatch bootstrap 稳定性 | liblspatch.so x86_64 与 guest 进程混合无公开先例 | PoC | emulator-research §5 |
| 未开始 | LSPatch 0.6 与 libxposed 101 兼容性 | 内置 runtime 较旧 | 必要时降级 API 93 构建 | README 已知待办 |
