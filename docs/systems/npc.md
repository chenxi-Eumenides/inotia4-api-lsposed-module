# NPC 交互系统逆向笔记（NPC）

> 目录：docs/systems/ ｜ 主题：NPC 对话树/选项结构逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）
> 原则：本文件是 NPC 系统的**探索结论唯一归属**；函数签名以 control-capability 为准，端点规格以 api-reference 为准。

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/npc/interact` | `UINpc_InitNPC()`(0xc2cfc) 无参 | v0.4.13 实现 | ✅ 真机 |
| `/api/action/npc/dialog/next` | `NPCTASKLIST_MakeDlg`(0x11e6a4) | v0.4.13 实现 | ✅ 真机 |
| `/api/action/npc/dialog/select` | 写 `NPCTASKLIST_nIndex`(0x307820) + `UINpc_ExeCurrentNpcTask`(0xc3070) | v0.4.13 实现 | ✅ 真机 |

## 2. interact 触发链（✅ 逆向）

```
GAMESTATE_PressKeyPlay(0x9cfc0) 交互分支（0x9d308-0x9d3c0）:
  CHAR_GetDistance(玩家, NPC) ≤ 6 + 面向检查
  → EVTSYSTEM_DoCheckAllEvent == -1（跳过剧情）
  → NPCSYSTEM_CheckFunctionDisplay(*(npc+0xa))
  → UINpc_InitNPC() @0xc2cfc（无参）
  → CHAR_StartActionID(player, 0) 停移动
```

**UINpc_InitNPC(0xc2cfc)**：PLAYER_pNearNPC 指向 NPC 存 0x305fb0 → NPCBOX_Create(0x13c914) → NPCTASKLIST_Create(0x11e574) → 读 npc+0xa(u16) → NPCSYSTEM_MakeFunctionList(0x11ec9c)。失败 NPCTASKLIST_Destroy 回滚。

**PLAYER_pNearNPC = 0x728fb8**（写者 `PLAYER_DoCheckNearNPC`(0x120d14)：遍历角色数组 stride 0x430，type==1 非队员距离<0x18；GOT 0x2f6db8）

## 3. 选项列表数据源（纯内存，无控件依赖）

| 全局 | 地址 | 说明 |
|---|---|---|
| UICHOICE_pItemText | 0x711c60 | 6×8B 指针数组（选项文本） |
| UICHOICE_nItemCount | 0x302d70 | u8 选项数（≤6） |
| UICHOICE_nFocusIndex | 0x302d80 | u8 当前焦点索引 |

## 4. 对话状态机（索引驱动，无持久节点）

| 全局 | 地址 | 说明 |
|---|---|---|
| NPCTASKLIST_nIndex | 0x307820 | u8 当前任务索引（写者 UINpc_ButtonListExe 0xc30bc） |
| NPCTASKLIST_nCount | 0x307821 | u8 任务数 |
| NPCTASKLIST_pData | 0x307818 | 8B→malloc 0x200=32×16B 槽数组 |
| NPCTASKLIST_pDescText | 0x307810 | 描述文本 |
| nSelectedID | 0x728e8e | u16 选中任务 ID |
| nSelectedType | 0x728e90 | u8 选中任务类型 |

- slot 布局 16B：+0 u8 type、+2 u16 id
- `NPCTASKLIST_GetSlot`(0x11e524)：slot=pData+idx*16 越界返 0
- `NPCTASKLIST_SetSelectedTask`(0x11e558)：nSelectedType=w0 / nSelectedID=w1
- `UINpc_ExeCurrentNpcTask`(0xc3070)：slot=GetSlot(nIndex)→SetSelectedTask(slot.type,slot.id)→tail `UINpc_ExeNpcTask(slot)`(0xc2d78 跳转表执行)
- `NPCTASKLIST_MakeDlg`(0x11e6a4)：按 slot type 读 desc 表 +0x10/+0x12 u16 文本 ID → char*（对话下一句）

## 5. 对话文本表

- `MEMORYTEXT_GetText`(0x118674)：textID→char*（GOT 0x2f49a0→0x7118a8 数据，GOT 0x2f3c48 记录数）
- NPC 功能表 pData=0x301830（NPCFUNCBASE_pData GOT 0x2f5b80）；desc 表 GOT 0x2f42f0→0x301730（NPCDESCBASE 区）

## 6. API 端点实现（纯内存+函数调用，不依赖控件树）

1. **interact**：前置 `PLAYER_DoCheckNearNPC` 设 PLAYER_pNearNPC → 调 `UINpc_InitNPC()`(0xc2cfc 无参返回 u8)
2. **dialog 选项**（GET 端点返回候选）：读 UICHOICE_pItemText/nItemCount/nFocusIndex
3. **select**：写 NPCTASKLIST_nIndex(0x307820) → 调 `UINpc_ExeCurrentNpcTask`(0xc3070)
4. **dialog/next**：`NPCTASKLIST_MakeDlg`(0x11e6a4)（对话下一句）

> 剧情过场对话（EVTSYSTEM_DrawDialog 0xf9908）独立系统，端点忽略。

## 6. 真机验证（v0.4.13，存档 2 商人 NPC）

- **interact**（角色在商人旁）→ `{"ok":true}`；frida 确认 `PLAYER_pNearNPC` 指向商人、**taskCount=1 slot0 type=1 id=3**（商店功能任务生成）
- **dialog/select {index:0}** → `{"ok":true}` → **`screen=shop` 进入商店**（type=1 任务执行成功）
- **dialog/next**（商店中）→ `no dialog`（非对话场景安全）
- **dialog/options** → 非选择型任务时 count=0；选择型对话时读 UICHOICE 全局
- 无 NPC 附近时：interact→`no npc nearby`、select→`bad index`、next→`no dialog`（全部安全拒绝无崩溃）

## 7. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| UINpc_InitNPC | 0xc2cfc | u8(void) |
| UINpc_ExeCurrentNpcTask | 0xc3070 | void(void) |
| UINpc_ExeNpcTask | 0xc2d78 | void(void* slot) |
| UINpc_ButtonListExe | 0xc30bc | — |
| NPCTASKLIST_GetSlot | 0x11e524 | void*(u32 idx) |
| NPCTASKLIST_SetSelectedTask | 0x11e558 | void(u8 type, u16 id) |
| NPCTASKLIST_MakeDlg | 0x11e6a4 | char*(void) |
| NPCTASKLIST_Create | 0x11e574 | — |
| NPCSYSTEM_MakeFunctionList | 0x11ec9c | — |
| PLAYER_DoCheckNearNPC | 0x120d14 | — |
| MEMORYTEXT_GetText | 0x118674 | char*(u32 textID) |
