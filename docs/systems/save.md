# 存档系统逆向笔记（Save）

> 目录：docs/systems/ ｜ 主题：存档/读档链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/save/save` | ⛔ 卡点（SAVE_Save 上下文复杂） | v0.4.0 占位 | 返回 not implemented |
| `/api/action/save/load` | ⛔ 卡点（仅主菜单/选档） | v0.4.0 占位 | 返回 not implemented |

## 2. 保存链（✅ 动态验证，P0-1 产出）

```
SystemMenu_ButtonSaveExe @0x14f7c4（保存按钮回调）
  → SOUNDSYSTEM_Play + SAVE_ProcessSave @0x129830
    → SAVE_IsOK(0x128c14) → KEY_ResetActive → SAVE_Save @0x129600
      → 细分：SAVE_SaveItem(0x1274f0) / SAVE_SaveCharacterAll(0x129480) / SAVE_SaveEvent(0x1282d0) / SAVE_SaveETC(0x1286f8) / SAVE_SaveInformation(0x1270ec)
      → SAVE_SaveData(0x1290c0) / SAVE_SaveDataAsKey(0x129050)
    → 成功弹 UIPopupMsg_CreateOKFromTextData("保存成功")
```

**已确认不可直接调用**（control-capability）：
- `SAVE_Save`(0x129600)：依赖存档上下文参数（[x0+0x8c0]），签名未完全确认
- `SAVE_ProcessSave`(0x129830)：UI 流程（弹窗+KEY 状态），依赖游戏状态机
- `SAVE_SaveData`(0x1290c0)：签名 `(w0,x1,x2)→SAVE_SaveDataAsKey`，上下文复杂

## 3. 存档数据结构

- `SAVE_pSaveSlot` @0x729858：存档槽结构（87 字节，含角色/物品数据，**离线兜底数据源**）
- `SAVE_nVersion` / `SAVE_nBuildNumber` / `SAVE_bSaveFlag`：存档版本/标志
- 存档槽 UI：SC_SAVESLOT @0x14c720，3 槽位面板

## 4. 待探索方向

1. SAVE_Save 完整签名/上下文（[x0+0x8c0] 是什么对象）
2. 或探索 UIPlay_CallSave 触发路径（0xc604c，36B UI 触发）——已验证 SystemMenu_ButtonSaveExe 路径
3. load 仅主菜单/选档界面的状态限制

## 5. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| SAVE_Save | 0x129600 | —（依赖 [x0+0x8c0]） |
| SAVE_ProcessSave | 0x129830 | —（UI 流程） |
| SAVE_SaveData | 0x1290c0 | — |
| SAVE_SaveDataAsKey | 0x129050 | — |
| UIPlay_CallSave | 0xc604c | —（36B UI 入口） |
| SAVE_SaveItem | 0x1274f0 | — |
| SAVE_SaveCharacterAll | 0x129480 | — |
| SAVE_IsOK | 0x128c14 | — |
