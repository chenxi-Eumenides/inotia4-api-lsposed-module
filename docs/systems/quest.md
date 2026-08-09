# 任务系统逆向笔记（Quest）

> 目录：docs/systems/ ｜ 主题：任务列表/状态/接交/放弃逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/quest/quit` | ⛔ 卡点（RefuseReview 硬编码 489 非通用） | — | — |
| GET `/api/info/quest/list` / `list/{id}` / `completed` | ⏳ 占位（任务列表结构未逆） | — | — |

## 2. 已实现

- GET `/api/info/quest/active`：`QUESTSYSTEM_nActiveQuest`(0x728ff8) 当前任务 ID
- GET `/api/info/quest`：复合端点（active 真实 + list/completed 占位空）

## 3. 已知逆向

- `QUESTSYSTEM_RefuseReview` @0x125cd0：**硬编码 quest 0x1e9=489** 的 `ChangeQuestState(489, 0)`——非通用放弃函数，不可用于任意任务
- `QUESTSYSTEM_RemoveSlot(slot)` @0x1229a4：任务槽删除（[0x2f6000+0x270] 槽数，CopySlot 前移）——需目标任务槽定位
- `QUESTSYSTEM_ChangeQuestState` @0x123bb4：任务状态机（RefuseReview 的跳转目标）
- `QUESTSYSTEM_AcceptReivew` @0x125c70：接任务（硬编码剧情任务）
- `QUESTSYSTEM_nActiveQuest` @0x728ff8：当前激活任务 ID（u16）

## 4. 待探索方向

1. 任务列表数据结构（槽数组结构：QUESTSYSTEM 槽区 0x2f6000+0x270 区域）
2. 任务详情（进度/目标/交付条件字段）
3. quest/quit 通用实现：任务槽定位（遍历槽区匹配 ID）→ RemoveSlot 或 ChangeQuestState

## 5. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| QUESTSYSTEM_RefuseReview | 0x125cd0 | void(void)（硬编码 489） |
| QUESTSYSTEM_RemoveSlot | 0x1229a4 | void(int32_t) |
| QUESTSYSTEM_ChangeQuestState | 0x123bb4 | void(int32_t, int32_t) |
| QUESTSYSTEM_AcceptReivew | 0x125c70 | — |
| QUESTSYSTEM_Add | — | 接任务（OP 用） |
| QUESTSYSTEM_ApplyReward | — | 交任务（OP 用） |
