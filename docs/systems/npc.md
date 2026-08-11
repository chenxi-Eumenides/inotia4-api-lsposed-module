# NPC 交互系统逆向笔记（NPC）

> 目录：docs/systems/ ｜ 主题：NPC 对话树/选项结构逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）
> 原则：本文件是 NPC 系统的**探索结论唯一归属**；函数签名以 control-capability 为准，端点规格以 api-reference 为准。

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/action/dialog/interact` | `UINpc_InitNPC()`(0xc2cfc) 无参 | v0.4.13（旧 /api/action/npc/interact）→ **v0.4.27 迁移** | ✅ 真机 |
| `GET /api/info/dialog/content` | 统一对话内容（story/npc/popup/none 四态） | **v0.4.27 新增** | ✅ 真机（npc/story/none 态） |
| `POST /api/action/dialog/select` | 统一选择（next/skip/ok/cancel/index） | **v0.4.27 新增**（替代旧 dialog/next + dialog/select） | ✅ 真机（参数校验/安全拒绝） |
| ~~POST /api/action/npc/dialog/next~~ | ~~NPCTASKLIST_MakeDlg(0x11e6a4)~~ | v0.4.13 → v0.4.27 移除 | — |
| ~~POST /api/action/npc/dialog/select~~ | ~~写 NPCTASKLIST_nIndex + UINpc_ExeCurrentNpcTask~~ | v0.4.13 → v0.4.27 移除 | — |
| ~~GET /api/info/npc/dialog/options~~ | ~~读 UICHOICE 全局~~ | v0.4.13 → v0.4.27 移除 | — |

> **v0.4.27 整合**：NPC 对话、剧情对话（AVG）、任务完成框、通用弹窗统一为一套 API
> （interact 发起 + content 取内容/选项 + select 选选项），见「§8 统一对话内容判定」。

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
2. **content**（v0.4.27 统一）：`data_dialog_content_json()` 按 §8 优先级返回四态（story/npc/popup/none）
3. **select**（v0.4.27 统一）：`data_op_dialog_select(action, index)` 按 §8 分派（next/skip/ok/cancel/index）

> 剧情过场对话（EVTSYSTEM）已在 v0.4.27 支持，见「§7 剧情对话系统（EVTSYSTEM）」。

## 7. 剧情对话系统（EVTSYSTEM，✅ v0.4.27 逆向 + 真机验证）

> 剧情对话（AVG 模式，说话人立绘 + 名字 + 文本 + SKIP 跳过 + ≫ 推进箭头）是**独立系统**，不经过
> NPC 对话/popup 栈，由切图/事件脚本触发（如 3080 地图右侧出口切图后自动触发）。

### 7.1 状态判定（screen=story）

剧情对话激活的**唯一可靠判定 = `GAMESTATE_nState==1`（Event）**：
- `GAMESTATE_nState` @0x72b068 (u32)：**0=Play(世界)、1=Event(剧情)、2=MapChange(切图)**（GAMESTATE_SetState 0x151590 分派实测）
- ⚠️ 不能单独用 `EVTSYSTEM_nState!=0` 或 `pText!=NULL` 判定——剧情结束后残留 `gst=0/evtNState=1/pText=NULL` 会误判（v0.4.27 修复）
- 剧情中 State 函数指针（0x309980 区 GOT 槽 [0x2f4890]/[0x2f3938]/[0x2f4930]/[0x2f5580]/[0x2f6248]）= EVT_Enter(0x9c4dc)/EVT_Process(0x9c618)/EVT_Draw(0x9c640)/EVT_PressKey(0x9c73c)/EVT_Exit(0x9c5c4)；Play=Enter0x9ca70/Process0x9cae4/PressKey0x9cfc0/Draw0x9d6cc；MapChange=Enter0x9c75c/Process0x9c7ec

### 7.2 数据源（readelf 符号 + 反汇编确认）

| 全局 | VMA | 语义 |
|---|---|---|
| EVTSYSTEM_pObject | 0x712ef0 | 立绘对象/说话 CHAR 指针 |
| EVTSYSTEM_pFocusChar | 0x712ef8 | 焦点角色 |
| EVTSYSTEM_nState | 0x713034 (u32) | 剧情事件状态：0=无、剧情对话中=3 |
| EVTSYSTEM_nInfo | 0x713048 (u8) | 未确认语义（剧情中=0） |
| EVTSYSTEM_nIndex | 0x713018 (u32) | 剧情文本索引（推进时递增 30→33→42→80→113） |
| EVTSYSTEM_nID | 0x71300c (u32) | 事件 ID（剧情中=1） |
| EVTSYSTEM_nDataCount | 0x713010 (u32) | 数据计数（剧情中=113） |
| EVTSYSTEM_pTeller | 0x713028 (8B) | 说话人 CHAR 结构指针（type@+0、x@+2、y@+4） |
| EVTSYSTEM_pText | 0x3075d0 (8B) | 当前对话文本指针（UTF-8，多句连续 00 00 分隔，pText 指向当前句） |
| EVTSYSTEM_TextCtrl | 0x713050 (128B) | 文本控件：+0x0=文本指针(=pText)、+0x2e=推进标志、+0x58=总页、+0x5a=当前页 |
| EVTSYSTEM_nDisplayAlpha | 0x713008 (u8) | 显示透明度（world=100） |
| EVTSYSTEM_pEventState | 0x3075c8 | 场景事件状态数组指针（剧情激活标记 eventState[0]=1，非完成标记） |

### 7.3 推进/跳过机制（EVTSYSTEM_Draw 0xf9e18 + EVTSYSTEM_PressKey 0xfb34c）

- `EVTSYSTEM_Draw(0xf9e18)`：读按键状态 [0x2f5000+0x48]+4，key==0x2→DrawDialog(0xf9908)、key==0x2d/0x4c→DrawTellCenter(0xf9798)
- `EVTSYSTEM_PressKey(0xfb34c)`：key==0x2（推进）：TextCtrl+0x2e==0→置1；当前页<总页→TEXTCTRL2_MoveNextPage(0x13d3c0)；否则 SetState(7)+INSTANTSYSTEM_DestroyType(2)（结束当前对话段，写场景事件状态数组[索引]=0xFFFFFFFF）
- **API 推进（story_next）**：TextCtrl+0x2e==0→置1；cur+1<total→MoveNextPage；否则写场景状态数组[索引]=0xFFFFFFFF
- **API 跳过（story_skip）**：场景状态数组[索引]= cur<=5?6:0xFFFFFFFF（≤5 跳下一段，否则结束）

### 7.4 真机验证（2026-08-11，存档 3080 地图右侧出口切图触发）

- 剧情中：gst=1、evtNState=3、nIndex 递增（30→33→42→80→113）、nID=1、nDataCount=113、pTeller=说话人 CHAR、pText=当前句
- 剧情文本示例：『卓拉德大人，你找我？』『嗯，凯恩，你来的正好。』（凯恩与卓拉德对话，说话人随 pTeller 变化）
- 剧情结束：nIndex=113 后 pText=NULL、popupOn=1 → 任务获得弹窗（该弹窗 API 读取正常）
- 屏幕分辨率 854×384：推进箭头 ≫ (836,326)、SKIP 按钮 (810,31)

## 8. 统一对话内容判定（v0.4.27 get-content 逻辑）

`data_dialog_content_json()` 按优先级返回四态之一（**一套 API 覆盖所有对话场景**）：

| 优先级 | 条件 | type | 内容 |
|---|---|---|---|
| 1 | GAMESTATE_nState==1（剧情中） | `story` | speaker(pTeller 名字) + text(pText) + index/count |
| 2 | popupOn（阻塞弹窗） | `popup` | text + options=[ok/cancel] |
| 3 | UICHOICE count>0 或 NPCTASKLIST count>0 | `npc` | speaker(PLAYER_pNearNPC) + text(NPCTASKLIST_pDescText) + options=[{id,label}]（选择型 id=0..5；对话型 id=next"下一句"） |
| 4 | 其他 | `none` | options=[] |

`data_op_dialog_select(action, index)` 统一分派：
- action=next：剧情中→story_next()；否则 NPCTASKLIST_MakeDlg（NPC 下一句）
- action=skip：story_skip()（跳过剧情）
- action=ok/cancel：dialog_ok()/dialog_cancel()（弹窗确认/取消）
- index≥0：写 NPCTASKLIST_nIndex → UINpc_ExeCurrentNpcTask（NPC 选项选择）
- ⚠️ story_next 在 pText==NULL 或 totalPages==0 时安全返回 op_ok（曾 bug：total=0 时写场景状态 -1 破坏剧情脚本）

## 9. 真机验证（v0.4.13，存档 2 商人 NPC）

- **interact**（角色在商人旁）→ `{"ok":true}`；frida 确认 `PLAYER_pNearNPC` 指向商人、**taskCount=1 slot0 type=1 id=3**（商店功能任务生成）
- **dialog/select {index:0}** → `{"ok":true}` → **`screen=shop` 进入商店**（type=1 任务执行成功）
- **dialog/next**（商店中）→ `no dialog`（非对话场景安全）
- **dialog/options** → 非选择型任务时 count=0；选择型对话时读 UICHOICE 全局
- 无 NPC 附近时：interact→`no npc nearby`、select→`bad index`、next→`no dialog`（全部安全拒绝无崩溃）

## 10. 相关符号表

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
| EVTSYSTEM_SetState | 0xfab38 | void(u32)（剧情状态写入） |
| TEXTCTRL2_MoveNextPage | 0x13d3c0 | void(void)（剧情文本翻页） |
| KEY_SetCode | 0x10f7f4 | void(u32)（注入按键码，保留未用） |
| GAMESTATE_SetState | 0x151590 | void(u32)（0=Play 1=Event 2=MapChange） |

> EVTSYSTEM 全局变量见 §7.2 表（G_EVT_* 已登记 game_symbols.h + check_symbols.py）。
