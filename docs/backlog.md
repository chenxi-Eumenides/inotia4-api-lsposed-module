# 开发待办总清单（Backlog）

> 日期：2026-08-07 ｜ 集中收录全部文档中"待实现/未验证/暂缓"项，作为唯一开发待办来源。
> 来源文档：player-operations.md / control-capability.md / api-spec.md / data-sources.md / game-systems.md / static-data.md / emulator-research.md / 本会话页面探索结论。
> 优先级为 2026-08-07 重新分配（按通用性 + 收益 + 探索难度综合），模拟器相关归待定区。

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

## P0 高优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 游戏系统的深入探索 | 游戏机制/系统/功能清单未系统梳理 | 全面梳理游戏系统与功能，输出清单——用于了解开发进度、持续补充待办清单 | 本会话决策 |
| 未开始 | 弹窗按钮文本映射 | v0.3.10/11 弹窗查询/操作已实现，但按钮文案（确认/取消/是/否）是固定资源，未映射输出 | 从按钮控件或静态资源读按钮文案，补 `dialog.buttons` 字段 | v0.3.11 延续（本会话） |
| 未开始 | 敌人坐标字段偏移 | `CHARLOCSYSTEM_pPool` 单位位置字段偏移待逆向 | 反汇编 CHARLOCSYSTEM_* + frida 运行时探测 | game-systems §4.5 |
| 未开始 | 地图瓦片编码 | `MAP_nBaseTile`(0x7148a8, 8192B) 瓦片编码格式待逆向 | **先确认寻路（ASTAR_GeneratePath 0xd93e4）是否强依赖瓦片通行矩阵**：强依赖→维持 P0，否则降 P3；反汇编瓦片解析 + 真机对照 | game-systems / api-spec §6 |

## P1 中优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 各函数调用的前提探索 | 操作端点通用前置未系统验证 | 探索各操作的前提约束：是否任何界面都能保存？能否跳过确认弹窗直达操作（如直接退到主菜单）？——所有操作端点的通用前置 | 本会话决策 |
| 未开始 | 改版说明图片探索 | `~/Documents/Install/Android/Game/艾诺迪亚4_盗版大修_v1.3.2_20260704_v5.0/` 下两张 jpg（附件20260704_v4.2 + 说明20260704_v5.0）为改版作者对原版游戏的修改介绍 | 读取图片内容，梳理改版相对原版的修改点（影响数据结构/机制判断） | 本会话用户提供 |
| 未开始 | 商店物品/价格数据结构 | UIStore 商品列表/价格表（DEALSYSTEM）未逆向 | 反汇编 `DEALSYSTEM_FindSaleByID` + UIStore 初始化链 | 商店买卖前置依赖 |
| 未开始 | 释放技能 | `UISkill_SkillMainExe`/`UIPlay_ButtonSKill` 依赖 UI/快捷键状态（战斗价值最高） | 探索底层技能释放函数（CHAR 技能使用链），做 `POST /api/action/player/{role}/cast` | player-operations §2.2 |
| 未开始 | 手动存档 `/api/save` | `SAVE_Save`(0x129600) 依赖存档上下文 `[x0+0x8c0]`；`SAVE_ProcessSave`/`SaveData` 确认不可直接调用 | 逆向 SAVE_Save 完整签名/上下文，或探索 `UIPlay_CallSave` 触发路径 | control-capability §5.2 |
| 未开始 | SYSTEMMENU 选项页结构 | 设置项/存档槽 UI 结构完全无逆向记录（最大空白页） | frida 枚举 UI 状态变量 + SAVE 调用链 | 本会话页面探索 |
| 未开始 | 佣兵遣散 | `MERCENARYSYSTEM_Release`(0x118ab4) 未逆向 | 逆向签名 + `POST /api/action/mercenary/discharge` | player-operations §2.6 |
| 未开始 | 队伍换位 | `PARTY_Swap`(0x11ff5c) `void(int32,int32)` 已确认签名 | 实现 `POST /api/action/party/swap`（真机验证边界） | player-operations §2.6 |
| 未开始 | 升级技能 | `CHAR_ProcessSkillBook`(0xe2488)（技能书路径）；`UISkill_ButtonUpExe` 依赖 UI | 探索升级函数与技能点校验 | player-operations §2.5 |

## P2 低优先级

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 加点分配函数 | `CHAR_SetStatusPoint`(0xd9c4c) 是"设剩余点数"（OP 语义），真正"属性+1/能力点-1"分配函数未记录 | frida hook 人物属性页加点按钮抓底层调用；找到后做 `POST /api/action/player/{role}/stat`（≤剩余点校验 = 普通） | 本会话页面探索 |
| 未开始 | 商店购买/出售 | `UIStore_BuyItem`(0xd242c)/`SellItem`(0xd25f0) 需 ControlObject_GetCursor 选中态（依赖 UI） | 依赖 P1 商店数据结构完成后，探索底层购买/出售函数（绕过 cursor） | player-operations §2.7 |
| 未开始 | 任务列表数据结构 | 仅 `QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID 可读；列表/状态/交付条件无记录 | hook `UIQuestMenu_ButtonClearExe`/`ButtonQuitExe` + 反汇编 QUESTSYSTEM | 本会话页面探索 |
| 未开始 | 静态表字段语义全逆向 | `field_catalog.json` 已验证 71 字段，其余待逆向 | 逐表解析（`*BASE_pData` + `record_index * nRecordSize`） | static-data §7 |
| 未开始 | 附魔属性对照表探索 | `~/Documents/Install/Android/Game/艾诺迪亚4_盗版大修_v1.3.2_20260704_v5.0/` 下 `附魔属性对照表1.xlsx`（附魔属性相关） | 解析 xlsx，整理附魔属性数据（用于强化/附魔数据校验） | 本会话用户提供 |
| 未开始 | 背包移动/整理 | `INVEN_MoveItem`(0x104934) 4 参签名复杂（item+3） | 逆向 4 参签名 + 真机验证 | control-capability §5 |
| 未开始 | 强化/镶嵌 | `ITEMSYSTEM_EnchantItem`(0x10b330)/`PutJewel`(0x10bcb4)/`ApplySocket`(0x10d8a4) 需物品+材料上下文 | 逆向执行路径（消耗校验） | control-capability §5.2 |
| 未开始 | 开箱 | `UIEquip_ButtonOpenBoxExe`/`ITEMSYSTEM_OpenItemBox` 未逆向 | 逆向 + 钥匙校验 | player-operations §2.3 |
| 未开始 | 队友 AI 设置 | 队友自动控制决策选项（是否用技能/是否主动攻击），在技能界面设置；只需**读/写选项**，不关心内部运作 | 逆向 AI 选项数据结构（读写选项），做 `GET/POST /api/action/player/{role}/ai` | player-operations §2.2 |
| 未开始 | 交互点 | 宝箱、恢复泉水等非敌人地图内容未探索 | 探索地图交互点数据（宝箱/泉水结构 + 交互函数） | 本会话决策 |
| 未开始 | 游戏机制的深入探索 | 各种数值如何计算（伤害/成长/随机范围公式）未逆向 | 逆向伤害/成长/随机公式，输出到 game-systems | 本会话决策 |
| 未开始 | 随机奖励的生成机制 | 掉落物/奖励如何生成（MakeItem 链）未探索 | 逆向 `ITEMSYSTEM_MakeItem` 系列 + 掉落表 | 本会话决策 |
| 未开始 | 休息（营地恢复） | `PARTY_ApplyRest`/`PARTY_GetRestCost` 未逆向 | 逆向 + 费用校验 | player-operations §2.2 |
| 未开始 | NPC 交互数据结构 | npc_dialog 面板已识别（v0.3.9），但对话选项/分支结构未探 | hook `UINpc_*` 抓对话选项 + 反汇编 NPC 系统 | 本会话页面探索 |
| 未开始 | activeQuest 接任务后实测 | 未真机验证 | 真机接任务后对比 `QUESTSYSTEM_nActiveQuest` | api-spec §7 |

## P3 暂缓

| 状态 | 待办项 | 现状 / 卡点 | 需要的探索 / 实现 | 来源 |
|---|---|---|---|---|
| 未开始 | 任务接取/交付 | `QUESTSYSTEM_AcceptReivew`(0x125c70) 硬编码剧情任务 quest 489，非通用 | 依赖 P2 任务列表结构，再找通用接/交函数 | player-operations §2.9 |
| 未开始 | 合成执行 | `UIMix_ButtonMixingExe`(0xc21ec) 依赖材料槽选中态；`MIXSYSTEM_CheckMixture` 仅检查非执行 | 探索 `MIXSYSTEM_*` 底层执行函数 + 材料上下文构造 | player-operations §2.8 |
| 未开始 | 读档 | `SAVE_Load*`/`GAMELOADER`（主菜单操作） | 风险高，暂缓 | player-operations §2.10 |
| 未开始 | 地图瓦片编码（降级分支） | 若寻路不依赖瓦片通行矩阵，由 P0 降级至此 | 见 P0 条目 | game-systems |
| 未开始 | `/api/info/path` 真机验证 | v0.2.34 实现，待真机确认 | 真机寻路对比 | api-spec §7 |
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
