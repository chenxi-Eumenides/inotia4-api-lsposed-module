# UI 状态系统逆向笔记（UI）

> 目录：docs/systems/ ｜ 主题：界面状态/弹窗/面板切换逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/ui/dialog/ok` | `UIPopupMsg` 确认按钮动作链 | 早前 | ✅ 真机 |
| `/api/action/ui/dialog/cancel` | 弹窗取消（Free 路径） | 早前 | ✅ 真机 |
| `/api/action/ui/panel/open` | ⛔ 卡点（popup 节点结构未逆） | — | — |
| `/api/action/ui/panel/close` | ⛔ 撤销（POPUPSTATE_Pop 崩溃） | — | — |

## 2. 界面状态结构（✅ GET 已实现）

- `screen`：当前界面（world/main_menu/loading/character_info/settings/...）
  - **`story`（✅ v0.4.27）**：剧情对话（AVG），判定 = `GAMESTATE_nState==1`（Event）；详细逆向结论见 `docs/systems/npc.md` §7
- popup 栈：`g_arrPopupStack` @0x728fd8 + `g_sPopupStateList` @0x2f9f58（ui_state_probe.js 探针）
- 已知 popup state（ui_state_probe KNOWN 映射）：
  - SC_SAVESLOT=0x14c720、SC_SYSTEMMENU=0x14fb38（proc 0x14fab0/evt 0x14fe64）
  - SC_OPTION_MMENU=0x14be20（proc 0x14c07c/evt 0x14c11c）、SC_DAILY_REWARD=0x16f050
  - SC_CHARACTER_INFO id=3、SC_SYSTEMMENU id=8、SC_OPTION_MMENU id=2

## 2.1 GAMESTATE 状态机（✅ v0.4.27 逆向）

- `GAMESTATE_nState` @0x72b068 (u32)：**0=Play(世界)、1=Event(剧情)、2=MapChange(切图)**（GAMESTATE_SetState 0x151590 分派）
- 状态函数指针变量区 0x309980（enter@+8/proc@+0x20/draw@+0x10/pk@+0x18/exit@+0，GOT 槽 0x2f4890/0x2f3938/0x2f4930/0x2f5580/0x2f6248）
- 各状态函数：EVT_Enter 0x9c4dc / EVT_Process 0x9c618 / EVT_Draw 0x9c640 / EVT_PressKey 0x9c73c / EVT_Exit 0x9c5c4；Play Enter 0x9ca70 / Process 0x9cae4 / PressKey 0x9cfc0 / Draw 0x9d6cc；MapChange Enter 0x9c75c / Process 0x9c7ec

## 3. 面板关闭崩溃（⛔ v0.4.5 实测）

```
POPUPSTATE_Pop @0x122600 = mov w0,#1 + tail-call POPUPSTATE_PopInternal(1)
PopInternal：ArrayStack_Pop(0x2f3000+0x590) → 销毁回调(+0x28) → 新栈顶 → 遍历栈节点触发回调
崩溃：POPUPSTATE_PopInternal+132 → STATE_ResumeGame → GAMESTATE_DrawPlay → MAP_DrawLayer+1396 SIGSEGV
```
**popup 栈状态机对 pop 顺序敏感**——绕过 UI 触摸直接调 Pop 不安全（settings 场景必崩，character_info 偶发成功）。panel/close 端点已撤销。

## 4. 弹窗结构（GET 已实现）

- `UIPopupMsg_CreateOKFromTextData`(0xca778)：OK 弹窗（无按钮，需 dialog/ok 确认）
- `UIPopupMsg_CreateYesNoFromTextData`(0xca7d4)：是/否弹窗
- 弹窗文本读取：`G_POPUP_TEXT`（256B 无校验，审计 M9 待加固）

## 5. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| POPUPSTATE_Pop | 0x122600 | void(void) |
| UIPopupMsg_CreateOKFromTextData | 0xca778 | — |
| UIPopupMsg_CreateYesNoFromTextData | 0xca7d4 | — |
| SystemMenu_ButtonSaveExe | 0x14f7c4 | — |
| SystemMenu_ButtonExitExe | 0x14f7e8 | —（回主菜单确认） |
| SystemMenu_ButtonHelpExe | 0x14fec0 | — |
| SystemMenu_ButtonBackExe | 0x14fd18 | — |
| UIOption_ButtonListExe | 0xc44d8 | —（设置面板按钮） |

## 6. UI_PopupProcess 流程分派（✅ 逆向，2026-08-09）

### 机制
```
UI_SetPopupProcessInfo(id, data)(0xaecc8)：Array_Add 把 (id, data) 加入 popup 数组 [0x2f5000+0xc38] 指向
UI_PopupProcess(0xaebfc)：主循环处理 popup 数组
  → Array_GetData 读项 → id = [x0]-1（0-3 对应流程 1-4）
  → 跳转表 [0x24a190 + id]（1 字节偏移）：流程1=0x14 / 流程2=0xe / 流程3=0x0 / 流程4=0xc
  → 分支目标 = 0xaec58 + 偏移*4：
     流程4 → 0xaec94（POPUPSTATE_Clear + 弹窗出栈）
     流程1/2/3 → POPUPSTATE_Push(0x122424) → blr x0 调 popup 回调（业务逻辑在回调）
  → Array_Delete 移除已处理项
```

### 已知流程
| 流程 id | data | 语义 |
|---|---|---|
| 4 | 0 | **读档**（GAME_StartResumeGame 后主循环处理，SAVE_LoadData→LoadPlayer→LoadCharacterAll→world） |
| 1 | 0x14 | 每日奖励确认（DailyReward_ButtonOKExe 注册） |

### 关键结论
- popup 回调 = 业务逻辑（读档/每日奖励），由 popup 栈节点驱动（POPUPSTATE_Push 返回节点 +0x18 回调）
- **enter-slot 直接调 GAME_StartResumeGame 缺 UI 初始化**：正常流程经 daily_reward 面板关闭链（确认键 → UI_SetPopupProcessInfo(1,0x14) → 流程1 回调）完成 world UI/HUD 初始化；跳过该链 → world 无 UI/HUD、亮度暗、不可控制（见 save.md §9）
