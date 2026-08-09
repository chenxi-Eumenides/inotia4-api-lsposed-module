# 存档系统逆向笔记（Save）

> 目录：docs/systems/ ｜ 主题：存档/读档链逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 端点状态

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `POST /api/action/save/save` | `SAVE_Save`(0x129600) 无参静默保存 | v0.4.16 | ✅ 真机 |
| `POST /api/action/save/load` | ⛔ 卡点（仅主菜单/选档界面，GAMELOADER 状态限制，P3 暂缓） | 占位 | not implemented |

## 2. SAVE_Save 完整签名（✅ v0.4.16 逆向修正）

**`SAVE_Save` 是无参函数 `int(void)`**——之前的"[x0+0x8c0] 上下文参数"是**误判**（0x2f3000+0x8c0 是全局存档上下文指针，非参数）。

```
SAVE_Save():
  SV_GoldGet() + SV_TStatPointGet(0x16c960) + SV_TSkillPointGet(0x16caf8) 校验（任一不过 → CS_knlExit 返回 0）
  KEY_ResetActive(0x10f354)
  写 [0x2f3000+0x878] 保存槽标志位（UTIL_SetBitValue）
  APPINFO_Save(0xd8084)（保存设置）
  SAVE_SaveInformation(0x1270ec) → SAVE_SetBlockInfo
  SAVE_SavePlayer(0x128e38) → SAVE_SetBlockInfo
  SAVE_SaveCharacterAll(0x129480) → SAVE_SetBlockInfo
  SAVE_SaveInventory(0x127d8c) → SAVE_SetBlockInfo
  SAVE_SaveQuest(0x128000) → SAVE_SetBlockInfo
  SAVE_SaveEvent(0x1282d0) → SAVE_SetBlockInfo
  SAVE_SaveETC(0x1286f8) → ...
  SAVE_SaveData(0x1290c0)（最终写盘）
```

## 3. 保存链（UI 流程 vs 直接调用）

```
UI 流程（P0-1 动态验证）：SystemMenu_ButtonSaveExe(0x14f7c4) → SAVE_ProcessSave(0x129830)
  → SAVE_IsOK(0x128c14) → KEY_ResetActive → SAVE_Save → 弹 UIPopupMsg_CreateOK("保存成功")

API 直接调用（v0.4.16）：SAVE_Save() 无参——静默保存无弹窗，内部校验通过即全量序列化
```

## 4. 存档数据结构

- `SAVE_pSaveSlot` @0x729858：存档槽结构（87 字节，含角色/物品数据，**离线兜底数据源**）
- 存档上下文：`[0x2f3000+0x8c0]`（全局指针，SAVE_Save 内部读取）
- 存档槽 UI：SC_SAVESLOT @0x14c720，3 槽位面板

## 5. 真机验证（v0.4.16）

- **hook 确认**：SAVE_Save → SAVE_SaveInformation → SAVE_SaveCharacterAll → SAVE_SaveInventory → SAVE_SaveData 全链命中（真实序列化）
- **存档生效验证**：丢弃再生药水（消耗背包物品）→ save/save → 回主菜单重进 → **再生药水保持丢弃**（存档写入生效）、金币 81 不变
- 用户规则（m1488）：save 测试可消耗资源并保存，但**消耗背包物品**（可再获得）而非金币/能力点

## 6. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| SAVE_Save | 0x129600 | int(void)（无参，完整保存） |
| SAVE_ProcessSave | 0x129830 | —（UI 流程，弹窗） |
| SAVE_SaveData | 0x1290c0 | —（写盘） |
| SAVE_SaveDataAsKey | 0x129050 | — |
| SAVE_SaveInformation | 0x1270ec | — |
| SAVE_SaveCharacterAll | 0x129480 | — |
| SAVE_SaveInventory | 0x127d8c | — |
| SAVE_SaveQuest | 0x128000 | — |
| SAVE_SaveEvent | 0x1282d0 | — |
| SAVE_SaveETC | 0x1286f8 | — |
| UIPlay_CallSave | 0xc604c | —（36B UI 入口） |
| SAVE_IsOK | 0x128c14 | — |
| APPINFO_Save | 0xd8084 | —（保存设置） |
