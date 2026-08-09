# 战斗系统逆向笔记（Combat）

> 目录：docs/systems/ ｜ 主题：战斗相关全部逆向结论与端点实现状态（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）
> 原则：本文件是战斗系统的**探索结论唯一归属**；函数签名以 control-capability 为准，端点规格以 api-reference 为准，本文档补充实现细节与逆向过程。

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/action/combat/{role}/config/auto-attack` | `CHAR_SetAutoAttack`(0xe4cf4) 写 [ch+0x3a0] bit7-10 | 早前 | ✅ 真机 |
| `/api/action/combat/{role}/switch` | `PARTY_SetActivePlayer`(0x11f584) | v0.3.2 | ✅ 真机 |
| `/api/action/combat/{role}/attack` | `CHAR_SetTarget`(0xdc754) + `CHAR_MakeDefaultAttack`(0xe2730) | v0.4.2 | ✅ 真机 |
| `/api/action/combat/{role}/stop` | `CHAR_StopCombat`(0xe7c24) | v0.4.2 | ✅ 真机 |
| `/api/action/combat/{role}/config/skill-usage` | `CHAR_SetSkillUsage`(0xe4cc0) 写 [ch+0x3a0] bit0-2 | v0.4.10 | ✅ 真机 |
| `/api/action/combat/{role}/cast` | `CHAR_GetEnemyTarget`(0xe42b4) + `CHAR_SetActionID`(0xe79ec) | v0.4.12 | ✅ 真机（第 3 次成功） |

## 2. AI 技能决策链（✅ 逆向，skill-usage 依据）

```
CHAR_ProcessAIOnCombat(ch) @0xe8b6c
  └─ CHAR_ProcessNormalAIOnCombat(ch) @0xe497c
       CHAR_GetSkillUsage(ch) → 0 则仅普攻（返回 0）
       遍历技能链表 [ch+0x2A0]（节点 +0x00 actionId u16 / +0x07 AI等级 / +0x18 next）:
         技能表档位 [0x24a000+0xcd8]（按 actionId 索引）== 0 → 跳过
         节点 [x+0x7]（AI 等级）== 0 → 跳过
         CHAR_GetActionState(ch, 节点) 非 0（冷却/状态中）→ 跳过
         按档位-1 跳转表选技能 → CHAR_StartActionEx(ch, action, 1)
```

**数据位域**（角色 +0x3A0）：
- bit0-2：`CHAR_Get/SetSkillUsage`（AI 技能总开关 0-7）
- bit7-10：`CHAR_Get/SetAutoAttack`（自动攻击开关）

**单技能 AI 等级**：技能链表节点 [x+0x7]（0=不用，1-3=等级）。UI 层 `UISkill_ButtonAIExe`(0xd06ac) 以 `(level+1)%3` 循环写入。**API 当前只实现总开关**，单技能等级需遍历链表定位 actionId 后直写（未实现，风险低可做）。

## 3. 攻击/停止链（✅ 实现）

- `CHAR_SetTarget`(0xdc754)：写 [ch+0x278]=target（目标变化时写全局状态 0x11 到 0x2f3000+0x698）
- `CHAR_MakeDefaultAttack`(0xe2730)：置普攻动作（写 [ch+0x2a8]，CHAR_FindAction(0xdd3ac) 算 actionId=5，CHAR_UpdateActionInfo(0xe2138)）
- `CHAR_StopCombat`(0xe7c24)：清 [ch+0x358] 战斗标志 → 清 [ch+0xc] bit2 → HATESYSTEM_RemoveWho(0x1024e4) → tail-call CHAR_SetActionID(ch,0,0)

## 4. cast（技能释放）——✅ v0.4.12 已实现

### 释放链（✅ 逆向）
```
CHAR_SetActionID(ch, actionId, target) @0xe79ec
  → CHAR_GetDisplayType(ch) @0xdcfd0 判断动作类型
  → 类型==4（技能）：查跳转表 0x24a000+0xce0（按 actionId 分支，特殊映射 +0x419 bit1）
  → CHAR_FindAction(ch, actionId) @0xdd3ac 找技能链表节点
  → CHAR_SetAction(ch, node, target) @0xe7630 写 [ch+0x280]（动作对象）+ 帧号/激活标志 + 朝向
```

### 关键发现（第三次成功，前两次崩溃根因）
- **`CHAR_SetActionID` 第 3 参不是 level，是目标对象指针**！技能动作（type==2）路径在 0xe79b0 读 `[target+2]/[target+4]` 坐标（UTIL_GetDirection 算朝向）——传 level（如 1）→ 读地址 3 → fault 0x3 崩溃
- **`CHAR_SetAction`(0xe7630)+896 崩溃** = 第 3 参 level 被当目标指针（x21=level → [x21+2] 读 0x3）
- 目标对象结构：+2/+4 = x/y 坐标（参考 `CHAR_Flee`(0xe7a98) 读法）
- **正确调用**：`CHAR_SetActionID(ch, actionId, target)`，target 用 `CHAR_GetEnemyTarget(ch,0,0)` 取游戏正规目标（无目标返回 null 安全跳过）

### 三次尝试对比
| 次 | 尝试 | 结果 |
|---|---|---|
| 1 (v0.4.5) | CHAR_SetTarget 传 pool 对象 + SetActionID(ch, id, level) | GAMEPLAY_DrawFocus+368 崩溃（fault 0x4）——level 当目标+目标无效 |
| 2 (v0.4.11) | GetEnemyTarget 自动取目标 + SetActionID(ch, id, level) | CHAR_SetAction+896 崩溃（fault 0x3）——level 仍被当目标 |
| 3 (v0.4.12) | **SetActionID(ch, id, target 指针)** | ✅ ok:true 无崩溃（actionId=5 已学技能；未学 44 报 skill not learned） |

### API 实现
- `data_op_cast(role, action_id)`：member → 校验技能已学（遍历 +0x2A0 链表）→ `fn_char_get_enemy_target(ch,0,0)` 取目标（null → `no target`）→ `fn_char_set_action_id(ch, action_id, target)`
- `POST /api/action/combat/{role}/cast` body `{"actionId":N}`（技能等级在链表节点内，无需传）
- 边界：未学 → `skill not learned`；无目标 → `no target`；role 无效 → `role not found`

### 待验证
- **成功施放的真实战斗效果**（当前地图无真敌人，GetEnemyTarget 返回的目标可能是交互物）；需在有怪物的地图验证技能实际释放（伤害/特效/法力消耗）

## 5. 敌人判定（部分探索）

- `CHAR_GetEnemyTarget`(0xe42b4)：`void*(ch, mode, flag)` — [ch+0x2C8] bit13 置位 → 返回 [ch+0x278]；否则 [ch+0x278] 非空返回它；空 → CHAR_FindBestTargetByAct(ch, mode) 自动找
- `CHAR_FindAttackCharacter`(0xdbdd4)：反向找"攻击目标是 ch"的角色
- units 端点 status 过滤：status==2=敌人/召唤物、status==1=城镇 NPC/佣兵（**但 status==2 含交互物如火把/宝箱**，非纯敌人——真敌人判定未完全逆向）

## 6. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| CHAR_SetAutoAttack | 0xe4cf4 | void(void*, int32_t) |
| CHAR_GetSkillUsage | 0xe496c | int(void*) |
| CHAR_SetSkillUsage | 0xe4cc0 | void(void*, int32_t) |
| CHAR_SetTarget | 0xdc754 | void(void*, void*) |
| CHAR_MakeDefaultAttack | 0xe2730 | int(void*) |
| CHAR_StopCombat | 0xe7c24 | void(void*) |
| CHAR_SetActionID | 0xe79ec | void(void*, int32_t, void*)（⚠️ 第 3 参=目标指针非 level） |
| CHAR_GetEnemyTarget | 0xe42b4 | void*(void*, int32_t, int32_t) |
| CHAR_ProcessAIOnCombat | 0xe8b6c | void(void*) |
| CHAR_ProcessNormalAIOnCombat | 0xe497c | void*(void*, int32_t, void*) |
| CHAR_FindAction | 0xdd3ac | void*(void*, int32_t) |
| CHAR_SetAction | 0xe7630 | void(void*, void*, int32_t) |
| UIPlay_ButtonSKill | 0xc6078 | — |
| CHAR_ProcessSkillBook | 0xe2488 | — |
