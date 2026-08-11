# UI 状态系统逆向笔记（UI）

> 目录：docs/systems/ ｜ 主题：界面状态/弹窗/面板切换逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/ui/dialog/ok` | `UIPopupMsg` 确认按钮动作链 | 早前 | ✅ 真机 |
| `/api/action/ui/dialog/cancel` | 弹窗取消（Free 路径） | 早前 | ✅ 真机 |
| `/api/action/ui/panel/open` | `data_op_panel_open`（扫描 g_sPopupStateList 找 state id → `UI_SetPopupProcessInfo(1,state_id)` → 主循环流程1 Push） | v0.4.32 | ✅ 真机（9 面板白名单） |
| `/api/action/ui/panel/close` | `data_op_panel_close`（栈顶 enter 匹配 PANELS → `UI_SetPopupProcessInfo(3,0)` → 主循环流程3 Pop + HUD 开关恢复） | v0.4.32 | ✅ 真机 |

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

## 3. 面板关闭/打开（✅ v0.4.32-0.4.33 解决）

### 崩溃历史（⛔ v0.4.5 实测）
```
POPUPSTATE_Pop @0x122600 = mov w0,#1 + tail-call POPUPSTATE_PopInternal(1)
PopInternal：ArrayStack_Pop(0x2f3000+0x590) → 销毁回调(+0x28) → 新栈顶 → 遍历栈节点触发回调
崩溃：POPUPSTATE_PopInternal+132 → STATE_ResumeGame → GAMESTATE_DrawPlay → MAP_DrawLayer+1396 SIGSEGV
```
**v0.4.5 崩溃根因**：HTTP 线程直接调 POPUPSTATE_Pop 是同步操作，绕过 popup 数组队列，破坏弹窗栈状态机时序。

### 官方安全链（✅ 已逆向）
- **关闭面板 = `UI_SetPopupProcessInfo(3, 0)`**（0xaecc8）→ 主循环 `UI_PopupProcess` 处理流程3 → **POPUPSTATE_Pop 异步出栈**。官方 `*_ButtonBackExe`（SystemMenu_ButtonBackExe @0x14fd18 / CharacterInfo_ButtonBackExe @0x14922c 等）均复现此链：SOUNDSYSTEM_Play(0) + 流程3 + HUD 开关恢复 `[0x2f6000+0xc48]=1`。
- **打开面板 = `UI_SetPopupProcessInfo(1, state_id)`** → 主循环流程1 → **POPUPSTATE_Push 异步入栈**（+ SetClearDrawFlag）。state_id 从 g_sPopupStateList（GOT 0x2f3000+0x4f0，27 条×64B）按 enter 指针（+0x10）匹配面板 VMA 扫描得到。

### v0.4.34 真机白名单（✅ 全部实测）
| 面板 | open/close | 面板 | open/close |
|---|---|---|---|
| character_info | ✅ | settings | ✅ |
| inventory | ✅ | skills | ✅ |
| mercenary | ✅ | quests | ✅ |
| choice/world_map/wipeout | ⛔ 语义不符（v0.4.34 移除） | options | ⛔ 需上下文 |
| craft/shop | ⛔ 需上下文 | input_count | ⛔ 需上下文 |
| save_slot/character_select/daily_reward/npc 系列/shortcut/in_app | ⛔ 需上下文 | | |

**移除语义（v0.4.34，用户指示）**：
- `choice`：游戏内由事件/剧情驱动的选择框，API 打开语义不符
- `world_map`：由游戏内事件（如保存点）驱动的世界地图，API 打开语义不符
- `wipeout`：角色死亡时游戏自动打开，非用户可操作面板（不禁止 close——死亡面板可经 panel/close 关闭）

**崩溃记录（SIGSEGV，tombstone 已验证，白名单排除依据）**：
- `options`（SC_OPTION_MMENU）→ `Scene_Process_POPUP_SC_OPTION_MMENU` → `Scene_Draw` → `GAMELOADER_DrawBackGround` → `GRPX_DrawPart`：**主菜单/GAMELOADER 场景专属**，world 下直接 Push 崩溃
- `craft`（SC_MIX）/`shop`（SC_STORE）→ `UIMix_Draw`/`UIStore_Draw` → `CHAR_GetName+16` 空指针解引用：需 NPC 交互对象 `[0x2f6000+0xc20]→[x0]` 就绪（游戏内由 NPC 打开）
- `input_count`（SC_INPUT_ITEMCOUNT）→ `UIInputItemCount_IsOn` → `ControlObject_GetActive` 空控件：需 inventory 物品数量输入上下文

**结论**：只有不依赖外部上下文（NPC 对象/物品选择/GAMELOADER 场景）的独立面板可 API 直接 Push；其余面板返回 `panel requires in-game context`，由游戏内交互打开。

### 3.1 wipeout 死亡面板对话感知（✅ v0.4.35）

wipeout 面板本身不可 API 打开（死亡时自动出现），但**统一对话 API 可感知并操作它**（v0.4.35 实现，栈顶 enter==0x1506d8 判定）：

| 端点 | 行为 | 真机验证 |
|---|---|---|
| `GET /api/info/dialog/content` | 栈顶是 wipeout → `{"type":"wipeout","options":[{"id":"revive","label":"复活"},{"id":"special_revive","label":"特殊复活"},{"id":"game_over","label":"游戏结束"}]}` | ✅ hp=0 触发死亡后返回 |
| `POST /api/action/dialog/select` action=game_over | 调 `Wipeout_ButtonGameOverExe`(0x1502ac) → 弹 "是否要回到主菜单？" YesNo 弹窗 → select ok 回主菜单 | ✅ 全链路 |
| `POST /api/action/dialog/select` action=revive | 调 `Wipeout_ButtonReviveExe`(0x1505a8) → 网络链（离线弹 "连接出错。请稍后重试。" TextData 0x4e） | ✅ 离线弹窗 |
| `POST /api/action/dialog/select` action=special_revive | 调 `Wipeout_ButtonSpecialReviveExe`(0x150640) → 同网络链 | ✅ 离线弹窗 |

**测试方法**：`POST /api/op/character/0/hp` body `{"hp":0}` → 角色死亡 → wipeout 自动打开 → get-content/select 可用。复活后 hp 回满但面板不自动关（API 直改不走游戏复活流程），需 `POST /api/action/ui/panel/close` 关闭。

**新符号**：F_WIPEOUT_BUTTON_REVIVE_VMA=0x1505a8、F_WIPEOUT_BUTTON_SPECIAL_REVIVE_VMA=0x150640、F_WIPEOUT_BUTTON_GAMEOVER_VMA=0x1502ac（均 `int ()` 无参按钮）。

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
| UI_SetPopupProcessInfo | 0xaecc8 | int(int32_t id, int32_t data) |
| POPUPSTATE_Push | 0x122424 | void(int32_t state_id) |
| POPUPSTATE_Pop | 0x122600 | void(void) |
| POPUPSTATE_Clear | 0x122698 | void(void) |
| POPUPSTATE_Exist | 0x1223f8 | int(void) |
| KEY_SetCode | 0x10f7f4 | void(int32_t code) |
| CharacterInfo_ButtonBackExe | 0x14922c | — |
| Wipeout_ButtonReviveExe | 0x1505a8 | int(void)（网络复活链） |
| Wipeout_ButtonSpecialReviveExe | 0x150640 | int(void)（特殊复活链） |
| Wipeout_ButtonGameOverExe | 0x1502ac | int(void)（弹 YesNo 回主菜单确认） |
| PARTY_IsWipeout | 0x120060 | int(void)（3 成员状态全 0/3 → 死亡判定） |

## 6. UI_PopupProcess 流程分派（✅ 逆向，2026-08-09 修正）

### 机制
```
UI_SetPopupProcessInfo(id, data)(0xaecc8)：Array_Add 把 (id, data) 加入 popup 数组 [0x2f5000+0xc38] 指向
UI_PopupProcess(0xaebfc)：主循环处理 popup 数组
  → Array_GetData 读项 → id = [x0]-1（0-3 对应流程 1-4）
  → 跳转表 [0x24a190 + id]（1 字节偏移）：流程1=0x14 / 流程2=0xe / 流程3=0x0 / 流程4=0xc
  → 分支目标 = 0xaec64 + 偏移*4：
     流程3（0x00）→ 0xaec64 = POPUPSTATE_Pop（关闭面板）
     流程2（0x0e）→ 0xaec9c = POPUPSTATE_Push(data) + blr 回调
     流程4（0x0c）→ 0xaec94 = POPUPSTATE_Clear（清空弹窗栈，读档用）
     流程1（0x14）→ 0xaecb4 = POPUPSTATE_Push(data) + SetClearDrawFlag（打开面板）
  → Array_Delete 移除已处理项
```

### 已知流程
| 流程 id | data | 语义 |
|---|---|---|
| 4 | 0 | **读档**（GAME_StartResumeGame 后主循环处理，SAVE_LoadData→LoadPlayer→LoadCharacterAll→world） |
| 1 | 0x14 | 每日奖励确认（DailyReward_ButtonOKExe 注册） |
| 3 | 0 | **关闭面板**（panel/close 端点） |
| 1 | state_id | **打开面板**（panel/open 端点） |

### 关键结论
- popup 回调 = 业务逻辑（读档/每日奖励），由 popup 栈节点驱动（POPUPSTATE_Push 返回节点 +0x18 回调）
- **enter-slot 直接调 GAME_StartResumeGame 缺 UI 初始化**：正常流程经 daily_reward 面板关闭链（确认键 → UI_SetPopupProcessInfo(1,0x14) → 流程1 回调）完成 world UI/HUD 初始化；跳过该链 → world 无 UI/HUD、亮度暗、不可控制（见 save.md §9）

## 7. 沉浸模式（✅ v0.4.36 真机验证）

> **补丁实现见 docs/systems/patch.md §3（游戏补丁统一文档，与 API 主线分离）**——含 hook 目标、API 30± 分支、获焦回调选型原因、验证。

游戏原生无沉浸模式：主题虽为 `NoTitleBar.Fullscreen`（状态栏已隐藏），但**导航栏/手势条始终显示**。由模块 hook 补丁解决（`patch/ImmersiveMode.kt`），与 popup 面板系统无交互，故本节仅留引用。
