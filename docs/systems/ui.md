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
- popup 栈：`g_arrPopupStack` @0x728fd8 + `g_sPopupStateList` @0x2f9f58（ui_state_probe.js 探针）
- 已知 popup state（ui_state_probe KNOWN 映射）：
  - SC_SAVESLOT=0x14c720、SC_SYSTEMMENU=0x14fb38（proc 0x14fab0/evt 0x14fe64）
  - SC_OPTION_MMENU=0x14be20（proc 0x14c07c/evt 0x14c11c）、SC_DAILY_REWARD=0x16f050
  - SC_CHARACTER_INFO id=3、SC_SYSTEMMENU id=8、SC_OPTION_MMENU id=2

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
