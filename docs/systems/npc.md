# NPC 交互系统逆向笔记（NPC）

> 目录：docs/systems/ ｜ 主题：NPC 对话树/选项结构逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/npc/interact` | ⏳ 未实现（对话结构未探） | — | — |
| `/api/action/npc/dialog/next` | ⏳ 未实现 | — | — |
| `/api/action/npc/dialog/select` | ⏳ 未实现 | — | — |

## 2. 已知线索

- **npc_dialog 面板已识别**（v0.3.9）：`screen=npc`（NPC 对话中）
- 对话树/选项结构未逆向：`UINpc_*` 系列函数未系统反汇编
- 对话面板的 popup state：`SC_NPC_DIALOG`（ui_state_probe KNOWN 映射，具体 id 见 data-sources）
- 相关：`UINpcQuest`（NPC 任务面板）、`UINpc_ButtonSelectExe` 等疑似对话选项回调
- 对话选项触发任务 = **合法**（走游戏 NPC 对话流程，npc/dialog/select）；**不经过 NPC 直接接/交任务 = OP**（/api/op/quest/*）

## 3. 待探索方向

1. 反汇编 `UINpc_*` 系列定位对话选项数据结构（选项列表/文本/分支）
2. 对话状态机（当前对话节点/选项指针的全局位置）
3. interact 的触发函数（对着 NPC 的交互动作，可能复用 CHAR_SetTarget+攻击键或 UIPlay 交互回调）

## 4. 相关符号（待查）

> 符号表检索词：`UINpc`、`NPCSYSTEM`、`NpcDialog`、`Dialog`
