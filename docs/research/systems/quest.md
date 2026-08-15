# 任务系统逆向笔记（Quest）

> 目录：docs/systems/ ｜ 主题：任务列表/状态/接交/放弃逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 / 数据源 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/quest/quit_quest` | QUESTSYSTEM_Find + RemoveSlot | v0.4.15 | ✅ 真机 |
| GET `/api/quest/active` | 槽数组（12B/槽 +0 questId）+ G_NPC_QUEST_STATE 状态表 | v0.5.5 | ✅ 真机 |
| GET `/api/quest/details` | 槽数组（12B/槽）+ G_NPC_QUEST_STATE + QUESTS.json | v0.5.0（list）/ v0.5.41 改名 | ✅ 真机 |
| GET `/api/quest/completed` | 状态表过滤 state==3 | v0.5.4 | ✅ 真机 |
| GET `/api/quest/{id}` | 静态表 QUESTINFOBASE（按下标） | — | ⏳ 详情结构部分未逆 |
| `POST /api/op/quest/accept` / `complete` | — | — | ⏳ NOT_IMPL（占位） |

## 2. 任务槽区结构（✅ 逆向 + frida 真机采样）

```
槽数：  [0x2f6000+0x270] 头 +0（u8，单层解引用：GOT → u8）
槽数组： [0x2f4000+0x3d0] → 指针 → 槽数组（⚠️ 双解引用：GOT 槽 → 二级指针 → 数组）
槽大小： 12B（CopySlot 0x122974 用 ×16-×4 计算）
槽布局： +0x00 questId（u16）
```

- `QUESTSYSTEM_Find`(0x122914)：`int(int32_t questId)` 遍历槽数组按 questId 匹配返回槽索引，未找到返回 -1
- `QUESTSYSTEM_RemoveSlot`(0x1229a4)：`int(int32_t slot)` 删槽（CopySlot 0x122974 后续前移 + QUEST_Initialize 0x122748 末槽清空 + 槽数-1），返回 1 成功/0 失败
- `QUESTSYSTEM_Add`(0x122a48)：加任务（Find 查重 + AllocateSlot 0x1228fc + 槽数+1 + 槽+0 写 questId）
- `QUESTSYSTEM_RefuseReview`(0x125cd0)：**硬编码 ChangeQuestState(0x1e9=489, 0)** 非通用——被通用方案替代
- ⚠️ **frida 探针陷阱**：槽数组需双解引用（`[0x2f4000+0x3d0].readPointer().readPointer()`）；单解引用读到的是二级指针值（如 7264）而非真实 questId（如 180/2/381）

## 2.1 questId 语义（✅ 2026-08-16 定案：= QUESTINFOBASE 记录下标）

> 澄清 P0 误报：**QUESTSYSTEM 槽数组 +0 questId = QUESTINFOBASE 记录下标**（非静态表 u16[0] 字段）。证据链：
> 1. **运行时 quest 总数 = 507**（`[0x2f6000+0xe08]` 单解引用 u16，真机 frida 实测）——与 QUESTINFOBASE 记录数 507 完全一致；而 u16[0] 值域仅 0-259（234 唯一值），不可能是索引空间
> 2. **frida 槽区采样**（真机2，2026-08-16）：槽 = [180, 2, 21]，与 `/api/quest/active` 输出一致；按下标联查静态表 → 第1章路障 / 刺杀格里普顿伯爵 / 导入战斗03任务，与游戏内名称全部精确配对
> 3. **反汇编**：`QUESTSYSTEM_ChangeQuestState`(0x123bb4) 用 `ldrh [0x2f6000+0xe08]`（总数 507）做 questId 边界校验（`cmp w19, w2; b.lt`）——questId 取值空间 [0,507) = 记录下标空间
> 4. **QUESTSYSTEM_Find** 反汇编确认槽读取 = 双解引用 + 12B 步长 + +0 u16，与 game_misc.cpp 实现一致
>
> **结论**：模块 quest_id 读取与 StaticData.questName（按下标 key）联查**均正确**，P0（2026-08-16 报告「quest_id=21 指向突破军用仓库」）为**误报**——报告者误把静态表 u16[0] 字段当作 quest_id。**无需改代码**。

## 2.2 QUESTINFOBASE.u16[0] 字段 = 任务链 ID（✅ 2026-08-16 静态表分析）

- 静态表 `QUESTINFOBASE.u16[0]`（api-reference 曾误称 quest_id）实为**任务链/任务组 ID**：
  - u16[0]=13：下标 19/20/21/22/381（导入战斗01/02/03、格里普顿伯爵护卫兵_战斗、序幕_战斗任务）
  - u16[0]=21：下标 61/62（突破军用仓库01/02）——**这就是 P0 误报里「quest_id=21 是突破军用仓库」的真相**
  - u16[0]=4：下标 5-9 + 180（第1章路障所属链）
- 全表 507 条 / 234 组 / 95 组含 ≥2 条——同一链的多条记录共享组 ID
- **状态表索引 = questId = 记录下标**（非 u16[0]）

## 2.3 任务状态表（✅ 2026-08-16 定案：两层解引用）

```
G_NPC_QUEST_STATE_GOT_VMA = 0x2f6000 + 0xb40
结构：GOT 槽 → 二级指针（.bss 内，如 0x7bef137830）→ 状态表数组（堆，如 0x7c05dc7e6c）
取值：states[questId] = 0 未接 / 1 进行 / 2 可完成 / 3 已完成
```

- ⚠️ **解引用层数：两层**（`**st_got`，game_misc.cpp:414 实现正确）。旧注释「三层解引用」有误；frida 若读三层会把状态表数组内容当指针（实测读得 0x00010000 = state[0..3] = 0,0,1,0），造成「状态表=0x10000 未初始化」假象
- 真机实测（2026-08-16）：`**st_got` = 0x7c05dc7e6c（堆），state[2]=1（刺杀格里普顿伯爵进行中）与 `/api/quest/active` 输出一致

## 2.4 主线/支线判定（✅ 2026-08-16 真机定案：QUESTGROUPBASE 组 +2 字节 bit0 = main 标识）

> **任务面板按「组」显示**（用户真机观察 + UIQuestMenu_MakeGroupList 0xcba40 反汇编）：显示组标题（QUESTGROUPBASE 标题）+ 组内当前任务详情/目标；组内前序任务完成后变灰显示在下方。**不显示任务名称**。

- **主线标志 = QUESTGROUPBASE 组记录 +2 字节 bit0**（1=主线，0=非主线）：
  - 实机验证：组4「黑暗教团教主的任务」bit0=1（面板 main 标识，任务 180/6 主线）；组249「初级融合器」bit0=0（重复任务 353）；组196「弱化的原因」bit0=0（支线 181）——与用户面板观察（3 组：主线组带 main、支线/重复组不带）完全一致
  - 全表：157/260 组 bit0=1，覆盖 392/507 条任务（主线剧情链：刺杀格里普顿伯爵→第1章→圣女救援→第2章→…→暗精灵剧情）
- **任务菜单隐藏标志 = QUESTINFOBASE u16[6] 高字节 bit5（0x2000）**（MakeGroupList 0xcba40 读 `questId*recordSize+13` 后 `tbnz w0,#5` 跳过）：战斗/教学/自动任务不显示在任务菜单（组13「序幕_战斗任务」整组 5 任务全 hidden → 面板无此组，用户观察证实；组4 的 8/180 单任务 hidden 但组通过 5/6/7/9 显示）
- ⚠️ **u16[15]（0/256）不是支线标志**（2026-08-16 证伪）：实机支线 181（弱化的原因）u16[15]=0；u16[15]=256 的 10 个任务（洛格的订单/传递铁矿石/露西的委托等，多为递送/系列任务）语义未定，保留原始值 side_flag 供后续逆向
- ⚠️ **u16[5] 不是支线标志**：主线 6（去找费罗赛普妮）u16[5]=298 ≠ 0；u16[5]=0 的多为战斗/测试任务

## 3. 真机验证（v0.4.15 + 2026-08-16）

- 用户接主线任务后槽区：180 / 2 / 381（3 任务）
- `quit {questId:381}` → ok:true；槽区复查 3→2（381 删除，剩余 180/2）✅
- `quit {questId:7264}` → quest not found（非真实任务 ID，二级指针值）✅
- 无效任务 999 → quest not found；缺参 → questId required ✅
- **2026-08-16 真机2（v0.5.36）**：`/api/quest/active` 输出 3 任务全对——
  `quest_id=180→第1章路障`、`quest_id=2→刺杀格里普顿伯爵`、`quest_id=21→导入战斗03任务`（state 全 1）；
  frida 槽区采样 [180, 2, 21] 与 API 一致；quest 总数 507 = QUESTINFOBASE 记录数；状态表 state[2]=1 与 API 一致（P0 quest_id 误报澄清，见 §2.1）

## 4. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| QUESTSYSTEM_Find | 0x122914 | int(int32_t) |
| QUESTSYSTEM_RemoveSlot | 0x1229a4 | int(int32_t) |
| QUESTSYSTEM_CopySlot | 0x122974 | void(int32_t, int32_t) |
| QUEST_Initialize | 0x122748 | void(void*) |
| QUESTSYSTEM_Add | 0x122a48 | int(int32_t) |
| QUESTSYSTEM_AllocateSlot | 0x1228fc | int(void) |
| QUESTSYSTEM_RefuseReview | 0x125cd0 | void(void)（硬编码 489） |
| QUESTSYSTEM_nActiveQuest | 0x728ff8 | u16（当前追踪任务） |
