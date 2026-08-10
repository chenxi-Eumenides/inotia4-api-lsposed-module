# 角色成长系统逆向笔记（Character）

> 目录：docs/systems/ ｜ 主题：角色属性/技能/加点/重置全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/character/skill` | `CHAR_LearnAction`(0xe2390) 学习/升级技能 | 早前 | ✅ 真机 |
| `/api/action/character/{role}/stat` | 属性+1/能力点-1（StatDivide 语义） | v0.4.5 | ✅ 真机 |
| `/api/action/character/{role}/stat-reset` | `CHAR_InitializeStatus`(0xe68c8) | v0.4.7 | ✅ 真机 |
| `/api/action/character/{role}/skill-reset` | `CHAR_InitializeSkill`(0xe67c8) | v0.4.11 | ✅ 真机 |
| `/api/op/character/{role}/level` | `CHAR_SetLevel`(0xe05a0) 完整升级结算 | v0.4.23 | ✅ 真机 |
| `/api/op/character/{role}/experience` | `CHAR_SetExperience`(0xd9b5c) 直写经验 | v0.4.23 | ✅ 真机（不触发升级） |

## 2. 属性结构（✅ 完整逆向）

**总属性 = 基础 + 已分配 + 加成**（CHAR_GetStat 三源求和）：
```
CHAR_GetStat(ch, i) @0xdf8d0 = CHAR_GetStatBase(0xdb9e4) + CHAR_GetStatMain(0xdb9f0) + CHAR_GetStatBonus(0xdb9fc)
```
- i=0-4：力量/敏捷/体力/智力/精力（u16）
- `CHAR_GetStatMain`(0xdb9f0) 读 [ch+0x256+i*2] = **已分配属性点**（v0.4.7 修正：非总属性！）
- `CHAR_SetStatMain`(0xdf1c4) 写 [ch+0x256+i*2] + CHAR_ResetAttrFromStat(0xdf098) 重算衍生 + SV_MainCharacterSet
- `CHAR_GetStatusPoint`(0xd9c44) 读 [ch+0x32a] 剩余能力点
- `CHAR_SetStatusPoint`(0xd9c4c) 写 [ch+0x32a]
- ⚠️ 角色 +0x24 数组（32 int32）是**战斗属性**（暴击/攻防等），非主属性

**StatDivide 加点链**（UI 面板语义）：
```
StatDivide_AddStat(statIndex) @0x148d14：面板缓冲 [0x307e20+0x48] 剩余点-1、[0x307e20+0x50+i*8] 属性+1
StatDivide_OKApply @0x149000：CHAR_GetStatMain+缓冲求和 → CHAR_SetStatMain → 剩余点≠0 时 CHAR_SetStatusPoint → StatDivide_Init 重置缓冲
```
**API stat 端点绕过 UI 缓冲**：直接 读角色能力点 → 校验>0 → CHAR_GetStatMain+1 → CHAR_SetStatMain → CHAR_SetStatusPoint-1（等价语义，无面板依赖）

## 2.5 等级与经验系统（✅ v0.4.23 完整逆向）

**角色字段布局**：
```
[ch+0xe]     C_LEVEL     等级 u8（0x14e0cc 读当前角色等级 = [0x2f6a68] 双重解引用 +0xe）
[ch+0x318]   C_EXP       经验 u64（CHAR_GetExperience 0xd9b54 读 / CHAR_SetExperience 0xd9b5c 写）
[ch+0x320]   C_NEXT_EXP  下一级所需经验 u64（CHAR_GetNextExperience 0xd9b68 读 / CHAR_SetNextExperience 0xd9c28 写）
[ch+0x328]   C_SKILL_POINTS  技能点 u8（CHAR_GetSkillPoint 0xd9c34）
[ch+0x32a]   C_STATUS_POINTS 能力点 u16（CHAR_GetStatusPoint 0xd9c44）
```

**CHAR_SetLevel(ch, level) @0xe05a0 完整升级结算链**：
```
1. 写 [ch+0xe] = level
2. CHAR_SetNextExperience(0xd9c28)  重算下一级经验 [ch+0x320]
3. CHAR_InitializeFromLevel(0xdf2c0) 按等级初始化属性
4. 若升级（新等级 > 原等级）：按表驱动加能力点/技能点
     [0x2f3000+0xe70] 表字节 → [0x2f5000+0x5a0] 表 → MEMORYTEXT_GetText_E(0x1186cc)
     → CAL_Calculate(0xd9968) 分别算能力点增量(w24)与技能点增量(w21)
     → CHAR_SetStatusPoint(旧+w24) + CHAR_SetSkillPoint(旧+w21) + SV_TStatPointSet/SV_TSkillPointSet 同步
5. 回满血蓝：C_HP(0x1f0) = CHAR_GetAttr(0x1e)，C_MP(0x1f4) = CHAR_GetAttr(0x1f)
```
⚠️ **降级限制**：`b.le` 分支——新等级 < 当前等级直接返回 0（不设置）。原版只允许升级/同级。
⚠️ **set-experience 不触发升级**：CHAR_SetExperience 只写 [ch+0x318]，升级结算由游戏内部经验获得事件（打怪）驱动，直写经验不会升级（真机实证）。

**升级点数是表驱动累加**（非按等级公式直接算）：每次升级调用时按「本次升了多少级」的表值增量叠加到当前能力点/技能点上。

**骰子随机依赖等级**（关联 docs/systems/inventory.md §2.5.3）：
- STATUSDICE_Roll 随机块 (0xfd26c) 读 byte_e = 当前角色等级（0x14e0cc，与 CHAR_SetLevel 写的 [ch+0xe] 同一字段）
- 判定 `byte_e/2 == 0`（等级 0-1）跳过随机；等级 2+ 执行 `MATH_GetRandom(0, byte_e/2)` 单向加到基础表值，clamp 127
- v0.4.23 真机实证：set-level 1→2 后骰子力量 50↔51 随机生效（byte_e 随等级字段同步更新）

## 3. 属性重置链（✅ v0.4.7）

```
CHAR_InitializeStatus(ch) @0xe68c8：
  5 项 CHAR_SetStatMain(ch, i, 0)     # 分配属性归 0
  能力点 = (等级-1) × 职业基础值       # MEMORYTEXT_GetText_E + CAL_Calculate(0xd9968)
  CHAR_SetStatusPoint(ch, 能力点)     # 还原能力点
  SV_MainCharacterSet
```
- 游戏 UI：CharacterInfo_ResetStatUIInAppProcess(0x149164)（含内购重置流程）
- **API 直接调 = 免费重置——用户确认归合法类别（v0.4.7）**

## 4. 技能重置链（✅ v0.4.11）

```
CHAR_InitializeSkill(ch) @0xe67c8：
  遍历技能链表 [ch+0x2A0]：
    技能表字节 [0x2f6000+0x150 × actionId] → [0x2f4000+0x9e0] 查保留标记（bit1）
    非基础 → ACTLIST_RemoveNode(0xd79bc) 移除
  技能点按职业还原：CHAR_SetSkillPoint(0xd9c3c) 读 [ch+0xe]
  SV_TSkillPointSet(0x16caa0) + PLAYER_RemoveShortcutType(0x121764) 清快捷键
  CHAR_ResetAttrUpdatedAll(0xd9f0c) 重算
```
- 游戏 UI：UISkill_ButtonSkillPointResetExe(0xcece8)（含内购流程）
- **底层函数独立可调——与 stat-reset 同级合法（v0.4.11）**，真机验证凯恩 actionId 80 移除+技能点还原 2

## 5. 技能链表结构

```
[ch+0x2A0] C_SKILL_LIST：链表头
节点：+0x00 actionId u16 / +0x02 level / +0x18 next 指针
[ch+0x2B0] C_SKILL_BMP：技能位图
[ch+0x280] C_ACTIVE_SKILL：当前激活动作
[ch+0x328] C_SKILL_POINTS：技能点
```
- `CHAR_FindAction`(0xdd3ac) 遍历链表按 actionId 找节点
- `CHAR_LearnAction`(0xe2390) 学习/升级（消耗技能点）

## 6. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| CHAR_GetStat | 0xdf8d0 | int(void*, int32_t) |
| CHAR_GetStatBase | 0xdb9e4 | int(void*, int32_t) |
| CHAR_GetStatMain | 0xdb9f0 | int(void*, int32_t) |
| CHAR_GetStatBonus | 0xdb9fc | int(void*, int32_t) |
| CHAR_SetStatMain | 0xdf1c4 | void(void*, int32_t, int32_t) |
| CHAR_GetStatusPoint | 0xd9c44 | int(void*) |
| CHAR_SetStatusPoint | 0xd9c4c | void(void*, int32_t) |
| CHAR_InitializeStatus | 0xe68c8 | void(void*) |
| CHAR_InitializeSkill | 0xe67c8 | void(void*) |
| CHAR_LearnAction | 0xe2390 | void*(void*, int32_t, int32_t) |
| CHAR_FindAction | 0xdd3ac | void*(void*, int32_t) |
| CHAR_SetSkillPoint | 0xd9c3c | void(void*, int32_t) |
| CHAR_GetExperience | 0xd9b54 | int64(void*) |
| CHAR_SetExperience | 0xd9b5c | void(void*, int32_t) |
| CHAR_GetNextExperience | 0xd9b68 | int64(void*) |
| CHAR_SetNextExperience | 0xd9c28 | void(void*, int32_t) |
| CHAR_SetLevel | 0xe05a0 | int(void*, int32_t) |
| CHAR_InitializeFromLevel | 0xdf2c0 | void(void*, int32_t) |
| CAL_Calculate | 0xd9968 | int(void*, void*, int32_t) |
| ACTLIST_RemoveNode | 0xd79bc | void(void*) |
| CHAR_ResetAttrFromStat | 0xdf098 | void(void*) |
| StatDivide_AddStat | 0x148d14 | void(int32_t) |
| StatDivide_OKApply | 0x149000 | void(void) |
| StatDivide_Init | 0x1488dc | void(void) |
