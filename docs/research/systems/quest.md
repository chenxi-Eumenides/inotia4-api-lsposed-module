# 任务系统逆向笔记（Quest）

> 目录：docs/systems/ ｜ 主题：任务列表/状态/接交/放弃逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/quest/quit` | QUESTSYSTEM_Find + RemoveSlot | v0.4.15 | ✅ 真机 |
| GET `/api/quest/active` | QUESTSYSTEM_nActiveQuest(0x728ff8) | 早前 | ✅ |
| GET `/api/quest/list` / `list/{id}` / `completed` | ⏳ 占位（任务详情结构未逆） | — | — |

## 2. 任务槽区结构（✅ 逆向 + frida 真机采样）

```
槽数：  [0x2f6000+0x270] 头 +0（u8）
槽数组： [0x2f4000+0x3d0] → 指针 → 槽数组（⚠️ 双解引用：GOT 槽 → 二级指针 → 数组）
槽大小： 12B（CopySlot 0x122974 用 ×16-×4 计算）
槽布局： +0x00 questId（u16）
```

- `QUESTSYSTEM_Find`(0x122914)：`int(int32_t questId)` 遍历槽数组按 questId 匹配返回槽索引，未找到返回 -1
- `QUESTSYSTEM_RemoveSlot`(0x1229a4)：`int(int32_t slot)` 删槽（CopySlot 0x122974 后续前移 + QUEST_Initialize 0x122748 末槽清空 + 槽数-1），返回 1 成功/0 失败
- `QUESTSYSTEM_Add`(0x122a48)：加任务（Find 查重 + AllocateSlot 0x1228fc + 槽数+1 + 槽+0 写 questId）
- `QUESTSYSTEM_RefuseReview`(0x125cd0)：**硬编码 ChangeQuestState(0x1e9=489, 0)** 非通用——被通用方案替代
- ⚠️ **frida 探针陷阱**：槽数组需双解引用（`[0x2f4000+0x3d0].readPointer().readPointer()`）；单解引用读到的是二级指针值（如 7264）而非真实 questId（如 180/2/381）

## 3. 真机验证（v0.4.15）

- 用户接主线任务后槽区：180 / 2 / 381（3 任务）
- `quit {questId:381}` → ok:true；槽区复查 3→2（381 删除，剩余 180/2）✅
- `quit {questId:7264}` → quest not found（非真实任务 ID，二级指针值）✅
- 无效任务 999 → quest not found；缺参 → questId required ✅

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
