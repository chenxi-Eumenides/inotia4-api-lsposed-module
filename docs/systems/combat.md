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
| `/api/action/combat/{role}/cast` | ⛔ 卡点（见 §4） | — | 两次崩溃 |

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

## 4. cast（技能释放）——⛔ 卡点详析

### 释放链（✅ 逆向）
```
CHAR_SetActionID(ch, actionId, level) @0xe79ec
  → CHAR_GetDisplayType(ch) @0xdcfd0 判断动作类型
  → CHAR_FindAction(ch, actionId) @0xdd3ac 找技能链表节点
  → CHAR_SetAction(ch, node, level) @0xe7630 写 [ch+0x280]（动作对象）+ 帧号/激活标志
```

### 两次崩溃记录（v0.4.5 / v0.4.11）
| 次 | 尝试 | 崩溃点 | 特征 |
|---|---|---|---|
| 1 | CHAR_SetTarget 传 pool 对象 + SetActionID | GAMEPLAY_DrawFocus+368 读目标 +0x4（fault 0x4，GLThread） | 误判为"无敌人目标" |
| 2 | CHAR_GetEnemyTarget 自动取目标 + SetActionID | **CHAR_SetAction+896 内部**（fault 0x3，HTTP 线程） | 与目标无关，动作链本身 |

### 关键结论
- **崩溃根因不是目标指针**（两次崩溃点不同，第二次在 CHAR_SetAction 内部，未到渲染）
- `CHAR_SetAction`(0xe7630)+896 处读 [x+3] 空指针——**对调用上下文敏感**，可能依赖：
  1. 战斗状态（STATE_nState / 角色战斗标志）
  2. 动作资源加载（skillInfo 动画/特效资源）
  3. 目标/位置上下文（攻击动作需要目标，技能动作需要施放位置）
- **待研究方向**：
  1. 反汇编 CHAR_SetAction(0xe7630) 完整 +896 处，确认崩溃读的是什么字段/对象
  2. 对比游戏正常施放路径（UISkill_SkillMainExe / UIPlay_ButtonSKill 触摸触发链）的前置状态
  3. 确认是否需先进入战斗状态（CHAR_SetCombatState 之类）或先设置施放位置
- 参考：`UIPlay_ButtonSKill`(0xc6078)（技能快捷键回调）、`UISkill_SkillMainExe`（技能主执行）、`CHAR_ProcessSkillBook`(0xe2488)（技能书路径）

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
| CHAR_SetActionID | 0xe79ec | void(void*, int32_t, int32_t) |
| CHAR_GetEnemyTarget | 0xe42b4 | void*(void*, int32_t, int32_t) |
| CHAR_ProcessAIOnCombat | 0xe8b6c | void(void*) |
| CHAR_ProcessNormalAIOnCombat | 0xe497c | void*(void*, int32_t, void*) |
| CHAR_FindAction | 0xdd3ac | void*(void*, int32_t) |
| CHAR_SetAction | 0xe7630 | void(void*, void*, int32_t) |
| UIPlay_ButtonSKill | 0xc6078 | — |
| CHAR_ProcessSkillBook | 0xe2488 | — |
