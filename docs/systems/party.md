# 队伍佣兵系统逆向笔记（Party/Mercenary）

> 目录：docs/systems/ ｜ 主题：队伍组成/佣兵管理（入队/离队/遣散/装备取出）全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/party/include` | `MERCENARYSYSTEM_IncludeParty`(0x118e04) | 早前 | ✅ 真机 |
| `/api/action/party/exclude` | `MERCENARYSYSTEM_ExcludeParty`(0x118d0c) | 早前 | ✅ 真机 |
| `/api/action/party/discharge` | `MERCENARYSYSTEM_Release`(0x118ab4) | v0.4.8 | ✅ 真机 |
| `/api/action/party/withdraw` | `CHAR_UnequipItemToInven`(0xe2f68) 对佣兵角色 | v0.4.9 | ✅ 真机 |

## 2. 队伍结构（✅ 已破解）

- 出战槽：3 人（SAVE_nPartyMercenarySlot 0x729828），`PARTY_GetSize` 人数
- 佣兵槽 ID = 角色 **+0x352**（member[0]=0、member[1]=19、member[2]=1）——**API mercenarySlot 参数用此值**
- 未上场佣兵槽数组：`*(*(0x2f6010))` 双层解引用，每槽 0x14，上限 `*(0x2f3978)`
- 角色↔槽关联：`CHARSYSTEM_FindAsMercenarySlot(slot)`(0xf4254) 按槽找角色（遍历角色池：基址 *(*(0x2f3bb8))、步长 0x430、范围 0x1a2c0、条件 obj[0]!=0 && obj[0x352]==slot）
- 符号：MERCENARYSYSTEM_pSlotList @0x307750（直接指向槽数组）
- ⚠️ 刚进 world 槽数据可能未初始化（type=255/flags=255），等几秒重查

**⚠️ 两套 slot 索引陷阱**（v0.4.8 frida 实测）：
- `/api/info/mercenary` 端点的 slot（27/32/58...）是**槽数组索引**（MERCENARYSYSTEM_pSlotList 下标）
- include/exclude/discharge 的参数 slot 是**角色 +0x352 槽 ID**（member[0]=0、member[1]=19...）
- 存档 2 凯恩 +0x352=0、其余 +0x352=255（无效）——discharge 无效槽返回 mercenary not found（安全）

## 3. 入队/离队链（✅ 早前）

```
MERCENARYSYSTEM_IncludeParty(ch) @0x118e04：内部 PARTY_GetSize<3 校验 + PARTY_Include + 位置设置，返回 1/0
MERCENARYSYSTEM_ExcludeParty(ch) @0x118d0c：PARTY_Exclude + 状态设置
```
- 边界校验：主控→`cannot exclude leader`；任务NPC→`cannot exclude quest npc`（CHAR_IsSpecialNPC 0xe4d90 识别：char +0x09(type)==2 且查表 bit2==1）
- PARTY_Exclude(char) @0x11f5c4 对主控/特殊 NPC 走 UIPopupMsg 弹窗路径返回 **-1**（truthy 陷阱，API 前置拦截）

## 4. 遣散链（✅ v0.4.8）

```
MERCENARYSYSTEM_Release(mercenarySlot) @0x118ab4：
  CHARSYSTEM_FindAsMercenarySlot 找角色
  MERCENARYGROUPSKILLSYSTEM_Remove(0x118900)
  角色 +0x352=-1 + 清 +0x3cc 标志
  CHAR_SetSituation(ch, 5)(0xdc310)
  MERCENARYSLOT_Initialize(0x11896c) + GAMESTATE_SetState(0x151590)
```

## 5. 装备取出（✅ v0.4.9）

- 佣兵装备走通用 CHAR 装备槽（+0x1F8 10 槽×8B），复用 `CHAR_UnequipItemToInven`(0xe2f68)
- `data_op_withdraw(mercenary_slot, equip_slot)`：find_char_by_merc_slot → 校验 equip_slot 0-9 → fn_unequip 脱下

## 6. 队伍操作边界（v0.3.5-0.3.6 逆向）

- 佣兵槽 ID = 角色 +0x352（member[0]=0、member[1]=19、member[2]=1），API `mercenarySlot` 参数传此值（非 mercenaries 端点的大池索引）
- `CHAR_IsSpecialNPC(char)` @0xe4d90：char +0x09(type)==2 且查表 bit2==1 → 任务队友（如沃尔达克），**不可离队**

## 7. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| MERCENARYSYSTEM_IncludeParty | 0x118e04 | int(void*) |
| MERCENARYSYSTEM_ExcludeParty | 0x118d0c | int(void*) |
| MERCENARYSYSTEM_Release | 0x118ab4 | void(int32_t) |
| CHARSYSTEM_FindAsMercenarySlot | 0xf4254 | void*(int32_t) |
| CHAR_IsSpecialNPC | 0xe4d90 | int(void*) |
| PARTY_Exclude | 0x11f5c4 | int(void*) |
| PARTY_Swap | 0x11ff5c | void(int32_t, int32_t) |
| PARTY_SetActivePlayer | 0x11f584 | int(int32_t) |
| MERCENARYSYSTEM_Allocate | 0x118a50 | void(int32_t) |
| MERCENARYSYSTEM_Set | 0x118b94 | void(int32_t, void*) |
| MERCENARYSYSTEM_AddCharacter | 0x118c10 | void(int32_t, void*) |
