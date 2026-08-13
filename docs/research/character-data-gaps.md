# character 域数据缺口研究（backlog R1-R5 / S1-S4 / D1-D3）

> 日期：2026-08-13 ｜ 来源：`docs/backlog.md` §character 域数据缺口（12 项）逐项研究
> 方法：文档 + 静态数据 JSON（`apk/static-data/json/`）+ `libgame.so` 反汇编（`.tmp/libgame_disasm.txt`，arm64）+ 模块源码（`module/`）+ 符号表（`apk/decompiled/libgame-symbols.txt`）
> 状态标记：✅ 已确认（静态证据闭环）／⚠️ 部分确认（需实机补验证）／❓ 待实机
> 成果归入：运行时逆向结论 → `docs/data-sources.md`；静态表字段语义 → `docs/reference/static-data.md`；API 行为 → `docs/api-reference.md`；backlog 条目按结果闭环

---

## R1 stats 属性名 22 项未逆向

### 现状（确认）

- `member_json()`（`module/app/src/main/cpp/game_read.cpp:46-54`）对 32 项属性**结构体直读** `[ch + C_ATTR(0x24) + a*4]`（a=0..31），输出 `"<id>":<value>` 裸数字，无属性名；仅 `max_hp`/`max_mp` 走 `fn_get_attr`（CHAR_GetAttr 0xdfd18，绑定于 `game_access.cpp:203`，只用了 0x1e/0x1f 两个 id）
- 已确认 9 项（api-reference §2.1）：0=暴击率×10、3=暴击伤害×10、4=攻击、8=魔攻、13=敏捷、17=防御、19=武器伤害减免率×10、30=HP上限、31=MP上限

### 新发现（静态证据）

**CHAR_GetAttr (0xdfd18) 完整反汇编**（`CHAR_IsAttrUpdated` 缓存 + `CHAR_UpdateAttr` 惰性刷新）：
- id 11/12：`sub w0, w19, #0xb; cmp w0, #0x1; b.ls` + clamp `cmp #0x2ee (750)` → **11/12 是 ×10 百分比类属性，上限 75.0%**（✅ 实机确认 11=魔法抵抗，面板 M.RES 35193；12 仍未定性）
- id 20 (0x14)：特殊分支——读装备 slot 6（`CHAR_GetEquipItem(ch,6)`）→ item+0x08 bit6-15 类别 → 查表 → 乘系数 → 返回。**id 20 是武器攻击类属性**（与主手武器类型挂钩，对应 §6.1 普攻类型公式）
- id 28 (0x1c)：`CHAR_UpdateAttr` 特殊分支——等级表（`[0x2f3000+0xe70]` 字节 + `[0x2f5000+0x5a0]` 表，每级 9 字节记录）读 u16 → 公式文本 → `CAL_Calculate` → 写 **[ch+0x94]**。与 id 30/31 同类（等级表驱动上限），但 0x94 ≠ HP/MP 上限（0x9c/0xa0），✅ 实机确认 28=等级驱动属性 =(960+36×等级)/10（黑魔导，见实机验证章节）
- id 30 (0x1e)：**[ch+0x9c] = HP 上限**（读后若当前 HP [ch+0x1f0] > 上限则钳制）✅
- id 31 (0x1f)：**[ch+0xa0] = MP 上限**（读后若当前 MP [ch+0x1f4] > 上限则钳制）✅
- id 113 (0x71)：**总攻击 = max(CHAR_GetNormalDamage 0xe0bd0, CHAR_GetNormalMagicDamage 0xe0c54)**，写入 [ch+0x1e8]（非 32 项数组内，扩展 id）
- id 1 / id 50：读 [ch+0x24+id*4] 的普通路径（id 50 越过数组尾部，ch+0x24+200=ch+0xE0）

**CHAR_UpdateAttr (0xdfb30) 加成链**（属性名语义的上游）：
```
[ch+0x24+id*4] = CHARSYSTEM_GetDefaultAttributeValue(ch, id)   // 默认值
→ CHAR_UpdateAttrFromEquip(ch, id, slot) ×10                   // 10 槽装备加成
→ CHAR_UpdateAttrFromStat(ch, id)                              // 主属性点加成
→ CHAR_UpdateAttrFromEquipOpt(ch, id, slot) ×10                // 装备词缀加成
→ CHAR_UpdateAttrFromSkill(ch, id)                             // 技能加成
→ CHAR_UpdateAttrFromMercenaryGroupSkill(ch, id)               // 佣兵群体技能加成
→ CHAR_UpdateAttrFromBuff(ch, id)                              // Buff 加成
→ id 11/12 clamp 750
```

**CHAR_UpdateAttrFromStat (0xdf99c) 的映射表**：遍历「主属性→attr」映射表（22 条级，**代码内建，非静态 *BASE 表**——ATTRINITBASE 记录 4B、STATUSASSIGNBASE 记录 2B，均 < 反汇编需要的 6B）：
- 记录结构：+0 主属性 id（u8，&0x7f ≤ 4 时走 `CHAR_GetStat` 读主属性值，否则 +2 引用文本参数）/ +1 目标 attr id（与当前计算 attr 匹配）/ +3 公式文本 u16（`MEMORYTEXT_GetText_E`→`CAL_Calculate`）/ +5 职业条件 u8（`CHAR_CheckCondition`）
- 记录数读 `[0x2f4000+0x8d0]`、记录大小读 `[0x2f4000+0xb80]`（u8）、数据读 `[0x2f6000+0xa38]`（GOT 双层）——三个 GOT 槽地址均未入 `game_symbols.h`

**STATUSINFOBASE = 主属性信息表（✅ 确认）**：6 条 × 6B = 3 个 u16 text_id 三元组：
| 记录 | text_id 三元组 | 内容（zh-Hans） |
|---|---|---|
| 0 | 12/13/14 | 力量/力量/「增加物理攻击力。装备双手武器」 |
| 1 | 15/16/17 | 敏捷/敏捷/「增加物理攻击力、命中及回避几率。」 |
| 2 | 18/19/20 | 体力/体力/「增加HP最大值和防御力。」 |
| 3 | 21/22/23 | 智力/智力/「增加魔法攻击力。装备水晶球（」 |
| 4 | 24/25/26 | 精力/精力/「增加魔法攻击力和命中几率。」 |
| 5 | 27/28/29 | （空×3，占位） |

**text 表 12-26 = 5 主属性名+加成描述**（STATUSINFOBASE 与 CHAR_GetStat 0-4 主属性一一对应）。

**ITEMOPTINFOBASE 词缀名 ≠ stats 数组编码**（api-reference 警告成立）：词缀 37 条记录名 = 1114-1150（text），编码 = §7.5 附魔属性编码（0-16 基础/17-29 元素/30-35 抗性），与 stats 数组索引（0=暴击率…31=MP上限）不同体系。

### 结论

- ✅ 实机已确认 15 项（11/14/15/18/20/28 等，见实机验证章节）；1,2,5,6,7,9,10,12,16,21,22-27 仍待高等级/词缀样本
- ✅ 实机结论：11=魔法抵抗、14=命中率基数、15=命中率百分比、18=物理减伤系数、20=副手武器攻击、28=等级驱动属性
- 实现路径：`member_json` stats 输出加名称映射表（✅ attr 名已实测补全，见实机验证章节）；`fn_get_attr` 已可用（`F_GET_ATTR_VMA=0xdfd18`）

---

## R2 skill max_level 权威读取路径

### 现状（确认）

- `build_skills_json()`（`game_read.cpp:530-565`）只读技能链表节点 `action_id`(u16 @+0x00)/`level`(u8 @+0x02)，**无 max_level 输出**；写侧 `data_op_learn_action`（`game_ops_value.cpp:143-148`）直接 `fn_learn_action`（CHAR_LearnAction 0xe2390）**无上限校验**

### 新发现（✅ CHAR_GetActMaxLevel 0xe9560 完整反汇编）

```
CHAR_GetActMaxLevel(ch, action_id)：
1. 记录大小 = [0x2f6000+0x150] 字节（技能信息表记录大小，u8）
2. 表1 = [0x2f4000+0x9e0] → ldr → 数据（技能信息表）
3. 读 int16 @ 表1 + action_id×记录大小 + 0x1D     ★ 权威基础值
4. w19 < 0（符号位）→ 返回 0
5. SV_MainCharacterGet(ch) == 0（非主角）→ 重试
6. 表2 大小 = [0x2f6000+0xe68]（u8）；表2 = [0x2f3000+0x758] → 数据
7. 读 u8 @ 表2 + (w19×表2大小 + 9)              ★ 映射：基础 max_level → 角色偏移
8. 读 u8 @ [ch + 表2值 + 0x2B2]                  ★ 角色 +0x2B2 字节数组
9. 返回 UTIL_GetBitValue(byte, 4, 1)             ★ bit4（1 位）= 技能书升级标志
```

- **权威路径 = 技能信息表 `[0x2f4000+0x9e0]` 记录 +0x1D int16**（api-reference R2 候选 ① 被反汇编证实）；`CHAR_GetActMaxLevel` 的返回值实际是 **[ch+偏移+0x2B2] 的 bit4（0/1）**——技能书使用与否的标志，而非完整 max_level
- api-reference R2 候选 ②「全局技能表 `*(0x2f3758)` +0x07」**修正**：0x2f3758 = [0x2f3000+0x758] = 上述**映射表**（把基础 max_level 映到角色偏移），非直接上限来源
- api-reference R2 候选 ③「+0x2B2 nibble 数组」**修正**：+0x2B2 是角色字节数组，bit4 为技能书升级标志（1 bit，非 nibble）
- **完整 max_level 语义**（✅ 实机验证，见实机验证章节 R2）：表1 +0x1D → 表2 偏移 → **[ch+0x2B2] bit1-4（4 位）** = 最终 max_level（常规 4/技能书 8，frida 写 bit1-4=4 实测切换；bit1-4=8 黑魔导 action50 实测）

### 结论

- ✅ 读取链已完全逆向并实机验证：4→8 变化 = [ch+0x2B2] bit1-4（见实机验证章节 R2）
- 实现路径：`build_skills_json` 增加 max_level 输出（native 直读表1 或调用 fn 封装的 CHAR_GetActMaxLevel）；写侧 `data_op_learn_action` 按 max_level 校验

---

## R3 set_skill_usage 单技能档位编码

### 新发现（✅ CHAR_SetSkillUsage 0xe4cc0 / CHAR_SetAutoAttack 0xe4cf4 / UTIL 反汇编）

**UTIL_SetBitValue (0x140564) 签名确认**：`(value, end_bit, start_bit, new_value)`（反汇编：`sub w1, w1, w2; add w1, w1, #1` = 取 bit[start..end]，与 UTIL_GetBitValue (0x140528) 的 (value, end_bit, start_bit) 对应；UTIL_GetBitValue(item, 15, 6) = item bit6-15 与 inventory.md 物品类别位域一致 ✅）

- **CHAR_SetSkillUsage(ch, v)**：`UTIL_SetBitValue([ch+0x3a0], 3, 0, v)` → 写 **[ch+0x3a0] bit0-3（4 位）**——⚠️ **修正 api-reference R3「bit0-2 总开关」为 bit0-3**
- **CHAR_SetAutoAttack(ch, v)**：`UTIL_SetBitValue([ch+0x3a0], 7, 4, v)` → 写 **[ch+0x3a0] bit4-7（4 位）**——⚠️ **修正 api-reference L528「写 bit7-10」为 bit4-7**（0x3a0 是单字节，无 bit7 以上）
- **[ch+0x3a0] 字节位域**：bit0-3 = 技能自动使用开关（总开关，4 位值）、bit4-7 = 自动反击开关（4 位值）

### 结论

- ✅ 单技能档位结论：技能链表节点 +0x07=1 **恒为激活标志**（frida 实测全部节点），native 无单技能档位；只有全局位域 [ch+0x3a0]（bit0-3 技能使用/bit4-7 自动反击）
- ✅ 三档映射结论：**native 无单技能档位**，mode 三档为 API 层设计值，底层仅全局开关（[ch+0x3a0] bit0-3）；api-reference set_skill_usage/set_auto_attack 位域描述已修正
- ⚠️ 文档修正项：api-reference set_skill_usage/set_auto_attack 的位域描述需更正

---

## R4 merc 两套索引统一

### 现状（✅ 代码确认）

- 读端点 `build_mercenaries_json()`（`game_read.cpp:567-607`）：`slot` 输出 = **槽数组下标 i**（`G_MERC_SLOTLIST_GOT_VMA = 0x2f6000+0x10` 双层解引用，20B/槽；过滤 `flags&0x01` 空槽、`M_TYPE>2` 跳过），再 `find_char_by_merc_slot(i)` 关联角色
- 写端点 `include/exclude/discharge/withdraw`（`game_ops_action.cpp:919-955`）：API 的 `mercenary_slot` 参数**原样**传给 `find_char_by_merc_slot(slot)`（`game_read.cpp:733-735` = `fn_find_merc_slot` = CHARSYSTEM_FindAsMercenarySlot 0xf4254）
- **native 层无两套索引转换逻辑**；`C_MERC_SLOT = 0x352`（game_symbols.h:27）有常量定义但读端点未使用
- data-sources §2.5：CHARSYSTEM_FindAsMercenarySlot(slot) 遍历角色大池（基址 `*(*(0x2f3bb8))`、步长 0x430、范围 0x1a2c0、条件 `obj[0]!=0 && obj[+0x352]==slot`）

### 新发现（⚠️ 槽数矛盾）

| 来源 | 槽数 | 说明 |
|---|---|---|
| data-sources L197 / backlog L109 | `*(0x2f3978)` = **88** | 单层解引用 |
| `game_symbols.h:148` G_MERC_MAX_GOT_VMA | `*(*(0x2f3978))` = **21**（注释：3 队伍+18 仓库） | **双层**解引用（GOT 槽） |
| 用户实测游戏 UI（backlog L110） | 佣兵仓库 **2页×9=18** | +3 队 = 21 ✅ 与 game_symbols.h 吻合 |

→ **0x2f3978 是 GOT 槽（双层解引用），data-sources 的 88 是少解一层得到的指针值**；真实槽数 = 21（3 队伍槽 + 18 仓库槽）。⚠️ 需实机确认（frida 读 `*(*(0x2f3978))`）。

### 结论

- 两套索引 = **槽数组下标（读端点 slot）** vs **角色 +0x352 槽 ID（写参数 mercenary_slot）**，桥接函数 = CHARSYSTEM_FindAsMercenarySlot（按 +0x352 匹配角色）
- ✅ 槽数实测 21（`*(*(0x2f3978))`）；两套索引经 CHARSYSTEM_FindAsMercenarySlot(0xf4254) 匹配（见实机验证章节 R4）；槽记录 → 角色对象指针偏移有待槽数据样本
- 实现路径（api-reference §2.2 方案）：API 统一为槽数组索引一套编号，写操作内部完成「槽数组下标 → +0x352」转换（遍历槽数组找 +0x352==目标 的槽，或经 CHARSYSTEM_FindAsMercenarySlot 反向）；`mercenary_slot` 参数改名 `slot` 与读端点对齐
- 文档修正：data-sources L197 槽数 88 → 21（双层解引用）；backlog L109/L110 与 api-reference §2.2 对齐（D3）

---

## R5 装备槽位表实机验证

### 新发现（✅ CHAR_FindEquipSlot 0xe4fd0 完整反汇编）

```
CHAR_FindEquipSlot(ch, item)：
1. CHAR_CanEquipItem(ch, item) 为 0 → 返回（不可装备）
2. 类别 = UTIL_GetBitValue(item+0x08 type, 15, 6)   // bit6-15
3. ITEMCLASSBASE 记录 = pData[0x2f4000+0xcf0] + 类别×(记录大小 [0x2f5000+0x308])
4. 读 ITEMCLASSBASE 记录 +2（u8）→ 槽位表索引                      ★ 中间索引
5. 槽位表 = pData[0x2f5000+0xb60]，记录大小 = [0x2f3000+0x418]
6. 读 槽位表 记录 +4（u8）→ 返回最终槽位（0-9）                     ★ 最终槽位
```

**CHAR_CanEquipItem (0xe4eb4) 校验链**：
```
ITEM_IsRealEquip(item)（0x105ab8）→ 非装备返回 0
ITEMCLASSBASE 记录 +7 bit4 标志（=1 则不可装备，tbnz 返回 0）
CHAR_CanChangeEquip(ch)（0xe4df4）→ 0 则不可
等级校验：[ch+0xe] ≥ ITEM_GetEquipLevel(item)（0x1097ac）
```

**ITEMCLASSBASE.json**（36 条 × 31B，记录索引 = item 类别）：+2 值分布 = 35/19/5/69/1/0（中间索引）；+7 bit4 标志（rec13/20/21 = 1，其余 0）

- ⚠️ **修正 api-reference L375**：「ITEMCLASSBASE 记录 +4 字节静态表」→ 实际为 **ITEMCLASSBASE 记录 +2 → 槽位表（[0x2f5000+0xb60]）+4**（双层表）
- native 侧：`data_op_equip`（`game_ops_action.cpp:585-606`）用 `fn_find_equip_slot`（F_FIND_EQUIP_SLOT_VMA=0xe4fd0）+ 10 槽硬编码遍历，无槽位表

### 结论

- ✅ 槽位双层表结构已实机验证：基础法杖→主手槽5、漆黑之皮甲→身体槽3，与 equipment 数组一致（见实机验证章节 R5）
- 实现路径：如需暴露类别→槽位映射，native 可查 ITEMCLASSBASE 记录 +2 → 槽位表 +4（把 [0x2f5000+0xb60]/[0x2f3000+0x418] 两个 GOT 槽登记入 game_symbols.h）

---

## S1 ITEMOPTINFOBASE.json 未打包（✅ 确认）

- `package_assets.py`（`scripts/parse/package_assets.py:13-23`）INCLUDE_TABLES 共 **28 张**，**不含 ITEMOPTINFOBASE**（也缺 TEXTDATABASE、ITEMSTATICOPTBASE）
- `StaticData.buildOptionNames()`（`StaticData.kt:79-98`）读 `tables/ITEMOPTINFOBASE.json` → `read()` 因 assets 缺失返回 null → emptyMap() → `optionName()` 恒空 ✅ 与 api-reference S1 描述一致
- `apk/static-data/json/tables/ITEMOPTINFOBASE.json` **存在**（37 条 × 12B），未进 APK assets
- **词缀名内容全量确认**（text 1114-1150）：索引 0-35 = §7.5 附魔属性编码完全一致（0-16 = 力量/敏捷/体力/智力/精力/暴击率/命中率/暴击伤害抵抗率/魔法抵抗率/回避率/盾牌格挡率/武器格挡率/MP增加/MP恢复/暴击抵抗率/暴击伤害增加率/HP吸收；17-29 = 火/风/寒气/神圣/黑暗/毒/重力摆/冰霜/治愈气息/狂战士/瞬间恢复/魔力专家/减少敌意值；30-35 = 眩晕/睡眠/失明/恐惧/减速/沉默抗性）；索引 36 = 哨兵记录
- **修复动作**：`package_assets.py` INCLUDE_TABLES 加入 `"ITEMOPTINFOBASE"` → 重跑（`uv run python scripts/parse/package_assets.py`）→ 重新构建 → 真机复验 option_names
- 体积影响：ITEMOPTINFOBASE.json 极小（37 条），无体积顾虑

---

## S2 className 职业名联查缺失（✅ 确认 + 实现依据）

- `CHARCLASSBASE.json` 已在 assets（INCLUDE_TABLES 含）✅
- `StaticData.kt` 无 `className()`（现有：itemName/rarityPrefix/optionName/enchantName）
- **CHARCLASSBASE 结构实测**（6 条 × 20B）：
  | 偏移 | u16 | 内容（zh-Hans 实测） |
  |---|---|---|
  | +0x00 | 0/2/4/6/8/10 | **职业名 text_id = class_idx×2**：黑暗骑士/忍者/黑魔导/祭司/暗影猎手/狂战士 ✅ |
  | +0x02 | 1/3/5/7/9/11 | 职业描述（紧邻职业名的奇数 text_id） |
  | +0x04 | 2317/3849/1797/1803/3847/2829 | 杂项文本（影子猎人侦察兵/封印地下城2-2/技能描述…），**非职业名** |
  | +0x0A | 1280/1290/1305/1300/1295/1285 | 各职业默认技能名（血之复仇/刀剑风暴/圣徒之牺牲/恢复术/钢筋铁骨/狂暴突进）✅ |
- **实现**：`className(classIdx)` = read CHARCLASSBASE → records[classIdx].u16[0] → text/zh-Hans.json；参照 `buildOptionNames()` 模式；party 复合/`{id}` 端点注入 id_name
- ⚠️ 注意 `buildItemNames()`（L65-77）用 records **数组下标**作 key（backlog L27 审计 H1 关联）：若 ITEMDATABASE 记录索引 ≠ itemId 会错位——className 实现前先核对 CHARCLASSBASE 记录索引 = class_idx（实测 u16[0]=class_idx×2 与索引一致 ✅）

---

## S3 skillName / skillMaxLevel 联查缺失（⚠️ 假设修正）

- SKILLDESCBASE / MAXLEVELBASE / SKILLTRAINBASE / SKILLTRAINPOINTBASE 均在 assets ✅；`StaticData.kt` 无 skillName()/skillMaxLevel()
- **⚠️ 修正 api-reference S3 假设「SKILLDESCBASE [0]→action_id 匹配」**：SKILLDESCBASE（114 条 × 24B）u16[0] = 20..142 递增（rec82/83 乱序）**非 action_id**
- **技能名文本段定位**：text **1242-1329 约 88 个技能名**（诅咒之呐喊/盾牌强击/火焰风暴/…/血之复仇/狂暴突进/刀剑风暴/钢筋铁骨/恢复术/圣徒之牺牲/…/狙击，后段 1330+ 为空）——6 职业默认技能名在段内位置：黑骑士=1280、狂战士=1285、忍者=1290、暗影猎手=1295、祭司=1300、黑魔导=1305（与 CHARCLASSBASE +0x0A 一致 ✅）
- ✅ 映射规则已定（实机验证章节 S3）：**技能信息表**（非 SKILLDESCBASE）recN↔action N 直接索引，技能名=rec+0 u16 text_id（=1220+rec，凯恩 action50=痛苦之击 text[1270] 实测）；SKILLDESCBASE 为描述表（u16[0]=20..142 效果 ID）
- **MAXLEVELBASE（48 条 × 4B）语义未确定**（仍待逆向）；**R2 已确认权威 max_level 路径 = 技能信息表 [0x2f4000+0x9e0] +0x1D + 角色 [ch+0x2B2] bit1-4（运行时，实测），skillMaxLevel() 应与之对齐**
- **实现**：skillName() = 技能信息表 rec+0 text_id 联查（映射已定，见实机验证章节 S3）；skillMaxLevel() 与 R2 结论对齐（native 输出或运行时表读取）

---

## S4 佣兵名联查缺失（✅ 确认 + 实现依据）

- `MERCENARYINFOBASE.json` 已在 assets ✅；`StaticData.kt` 无 mercName()
- **MERCENARYINFOBASE 结构实测**（47 条 × 8B）：
  | 偏移 | u16 | 内容 |
  |---|---|---|
  | +0x00 | 18/19/20/21… | 佣兵特性文本（体力/体力/增加HP最大值…/智力…） |
  | +0x02 | 0/256/512… | 未知（text 假阳性） |
  | +0x04 | **35752+idx** | **佣兵名 text_id（连续段）** ✅ |
  | +0x06 | 65535 | 无效占位 |
- **佣兵名文本段全量确认**（text 35752-35798 = 47 个）：35752-35757 = 6 职业佣兵（带 BOM 的黑暗骑士/忍者/暗影猎手/黑魔导/祭司/狂战士）、35758-35763 = 6 职业佣兵（二组）、35764-35798 = 35 个人名佣兵（费罗赛普妮/沃尔达克/峡谷怪人/凯萨尔/克雷尔/罗扎林/阿梅里尔/多巴啦/黎卡/莫尔加纳/露西/法米诺/马雷斯/勒兹/雷古拉斯/菲洛特/艾希文/拉卡诺斯/卢马赫/贾比尔/奥罗贝/埃尼斯/赛罗法纳/多雷克德/卡斯特/多尔法/格莱尼酷斯/加拉德/西雷斯/扎姆尼恩/肯金/卡茵/里内亚/费诺亚/鑫迪）
- **实现**：`mercName(mercId)` = read MERCENARYINFOBASE → records[mercId].u16[2] → text；⚠️ 前置：确认 MERCENARYINFOBASE 记录索引 ↔ 槽结构 type（槽记录 +0x00 type u8）的对应（MERCENARYSYSTEM_AddCharacter 反汇编待补）→ 修正 name=null 槽成因（与 mercenary inParty 条目对齐，backlog L132）

---

## D1 static-data.md §7.2 职业名字段错误（✅ 确认）

- **错误确认**：`docs/reference/static-data.md` §7.2 L159 记「CHARCLASSBASE +0x04 = class_display_name（TEXT 83%，6 唯一 = 6 职业名）」——+0x04 的 6 个值（2317/3849/1797/1803/3847/2829）实测为杂项文本（影子猎人侦察兵/封印地下城2-2/技能描述等），**TEXT 命中是假阳性**，语义非职业名
- **正确结论**：职业名在 **+0x00**（u16[0] = class_idx×2 = 0/2/4/6/8/10 → 黑暗骑士/忍者/黑魔导/祭司/暗影猎手/狂战士）；+0x02 = 职业描述
- **修正动作**：static-data.md §7.2 CHARCLASSBASE 行：`+0x04 class_display_name` → `+0x00 class_name（text_id=class_idx×2）`、`+0x02 class_desc`，并补充 +0x0A base_skill_text 已有、+0x04 标记为「杂项文本（语义未定）」

---

## D2 backlog L63 恒空矛盾描述（✅ 确认）

- **矛盾成立**：backlog L63 声称「✅ v0.4.64 已修复 buildOptionNames 恒空」；但 `package_assets.py` INCLUDE_TABLES 缺 ITEMOPTINFOBASE → `StaticData.buildOptionNames()` 的 `read("tables/ITEMOPTINFOBASE.json")` 返回 null → emptyMap() → **optionName() 仍恒空**
- L63 描述中的「词缀名 1114-1150 全量确认」指**静态数据侧**（apk/static-data/json/tables/ITEMOPTINFOBASE.json 已解析出词缀名），非运行时 assets——修复方向 = 执行 S1（打包 ITEMOPTINFOBASE.json）后闭环
- **修正动作**：执行 S1 → 真机复验 option_names 非空 → 更新 backlog L63 描述（区分「静态解析完成」与「运行时可用」）

---

## D3 backlog merc 两套索引条目对齐（✅ 结论 = R4）

- backlog L109（88 槽数组）/L110（18 佣兵仓库）现象统一解释：
  - **两套索引** = 槽数组下标（读端点 slot）vs 角色 +0x352 槽 ID（写参数 mercenary_slot），桥接 = CHARSYSTEM_FindAsMercenarySlot
  - **槽数**：真实值 = 21（3 队伍 + 18 仓库，game_symbols.h:148），data-sources 的 88 = 少解一层 GOT 的指针值；用户实测 18 佣兵仓库与 21 吻合（3+18）
- 对齐动作：data-sources L197 修正槽数 88 → 21（注明 GOT 双层解引用）；backlog L109/L110 标注归并到 R4；api-reference §2.2 两套索引说明保持权威

---

## 汇总表

| # | 结论 | 状态 | 关键证据 | 归入文档 |
|---|---|---|---|---|
| R1 | attr 名称 15 项实机确认（0/3/4/8/11/13/14/15/17/18/19/20/28/30/31 + 113）；11=魔法抵抗、14=命中基数、15=命中率、18=物理减伤、28=等级驱动；公式表 19 条实测；面板映射 10 项 | ✅ | 映射表 dump + formula-e 联查 + CharacterInfo_InfoDraw 反汇编 | api-reference §2.5 + data-sources |
| R2 | max_level 权威路径闭环：表1 +0x1D → 表2 偏移 → [ch+0x2B2] bit1-4（4 位）= 实际 max_level（常规4/技能书8，实测切换） | ✅ | 反汇编 + frida 写 bit1-4 实测 | data-sources + api-reference |
| R3 | 位域确认：bit0-3 技能使用/bit4-7 自动反击；节点 +0x07=1 恒为激活标志，**native 无单技能档位** | ✅ | 反汇编 + frida 写 0x35 实测 | api-reference（修正）+ control-capability |
| R4 | 两套索引无转换；槽数 21 实测（*(*(0x2f3978))）；桥接=FindAsMercenarySlot(0xf4254)；data-sources 88 误读已修正 | ✅ | frida 实测 + 反汇编 | api-reference + data-sources |
| R5 | 槽位双层表实测：法杖→主手5、皮甲→身体3；与 equipment 数组一致 | ✅ | frida hook CHAR_FindEquipSlot 实测 | data-sources + api-reference |
| S1 | ITEMOPTINFOBASE.json 存在但未打包 → optionName 恒空；词缀名 37 条=§7.5 编码 | ✅ | package_assets.py + StaticData.kt + ITEMOPTINFOBASE.json | backlog 闭环（打包修复） |
| S2 | CHARCLASSBASE.json 在 assets；u16[0]=class_idx×2 职业名验证通过；无 className() | ✅ | CHARCLASSBASE.json 实测 | api-reference（实现）+ static-data |
| S3 | 映射规则定案：技能信息表 recN↔action N，技能名=rec+0 text_id（=1220+rec）；凯恩 action50=痛苦之击实测 | ✅ | frida dump 技能信息表 + text 联查 | api-reference + data-sources |
| S4 | MERCENARYINFOBASE +0x04=佣兵名（35752+idx）；47 佣兵名全量确认；无 mercName() | ✅ | MERCENARYINFOBASE.json + text 35752-35798 | api-reference（实现） |
| D1 | static-data.md §7.2 +0x04=class_display_name 错误，实际 +0x00=职业名 | ✅ | CHARCLASSBASE.json 实测 | static-data.md（修正） |
| D2 | L63 恒空矛盾成立（静态解析 ≠ 运行时可用） | ✅ | package_assets.py + StaticData.kt | backlog（修正描述） |
| D3 | 两套索引 = R4；槽数 88→21（GOT 双层） | ✅ | game_symbols.h:148 + data-sources L197 | data-sources + backlog |

## 附：本研究新增 GOT 槽/表地址清单（待登记 game_symbols.h）

| 地址 | 语义 | 来源 |
|---|---|---|
| `0x2f4000+0x8d0` | 主属性→attr 映射表记录数（u16） | CHAR_UpdateAttrFromStat |
| `0x2f4000+0xb80` | 主属性→attr 映射表记录大小（u8） | CHAR_UpdateAttrFromStat |
| `0x2f6000+0xa38` | 主属性→attr 映射表数据（GOT 双层） | CHAR_UpdateAttrFromStat |
| `0x2f4000+0x9e0` | 技能信息表数据（记录 +0x1D = 基础 max_level int16） | CHAR_GetActMaxLevel |
| `0x2f6000+0x150` | 技能信息表记录大小（u8） | CHAR_GetActMaxLevel |
| `0x2f3000+0x758` | max_level → 角色偏移映射表数据 | CHAR_GetActMaxLevel |
| `0x2f6000+0xe68` | max_level 映射表记录大小（u8） | CHAR_GetActMaxLevel |
| `0x2f4000+0xcf0` | ITEMCLASSBASE 数据（记录 +2 = 槽位表索引、+7 bit4 = 不可装备标志） | CHAR_FindEquipSlot / CHAR_CanEquipItem |
| `0x2f5000+0x308` | ITEMCLASSBASE 记录大小（u8） | CHAR_FindEquipSlot / CHAR_CanEquipItem |
| `0x2f5000+0xb60` | 装备槽位表数据（记录 +4 = 最终槽位） | CHAR_FindEquipSlot |
| `0x2f3000+0x418` | 装备槽位表记录大小（u8） | CHAR_FindEquipSlot |
| 角色 `+0x94` | id 28 属性存储（等级表驱动，语义待定） | CHAR_UpdateAttr |
| 角色 `+0x2B2` | max_level 技能书标志 bit 数组（bit4） | CHAR_GetActMaxLevel |
| 角色 `+0x3a0` | 技能使用 bit0-3 / 自动反击 bit4-7 | CHAR_SetSkillUsage / CHAR_SetAutoAttack |

## 实机验证阶段（2026-08-13，真机2 192.168.3.54，frida 探针 attach 实测）

> 新档流程：`POST /api/system/save/create {slot:0,class_idx:2}`（黑魔导凯恩）→ 剧情 story → frida 调 `Event_ButtonOKExe(0x9c4ac)` 逐句推进 → world（初始营地 map_id=0）。存档需 `POST /api/system/save/save` 落盘。

### R1 属性名（✅ 完全闭环）

**黑魔导 LV1 实测 attr[0-31]**：
`70,0,0,1000,12,0,0,0,24,0,0,0,0,7,80,72,8,9,0,30,0,1000,0,0,0,0,0,0,99,8,1432,200`

**CHAR_UpdateAttrFromStat 映射表（19 条 × 6B，frida dump 运行时表）**：
`+0=主属性(0力1敏2体3智4精) +1=attr +2=参数 +3=公式text_id(u16) +5=条件`。公式文本联查 `formula-e.json`：

| 主属性 | attr | 公式（text_id） | 结论 |
|---|---|---|---|
| 力量 | 4 攻击 | a×700/600/500÷1000（职业条件 4/2/3/5/6/18/1 各异） | 力量→攻击 |
| 敏捷 | 4 攻击 | a×500÷1000 | 敏捷→攻击 |
| 敏捷 | **15** | a×4 | 命中率（敏捷×4） |
| 敏捷 | **13** | a×1 | 总敏捷 |
| 体力 | **30** | a×80 | HP上限（80血/点） |
| 体力 | **17** | a×1 | 防御（1防/点） |
| 智力 | 8 魔攻 | a×600/700/1000÷1000（条件 5/6/16） | 智力→魔攻 |
| 精力 | 8 魔攻 | a×600/600/1000÷1000 | 精力→魔攻 |
| 精力 | 15 | a×4 | 命中率（精力×4） |

实测验证：attr15=72=(敏捷7+精力11)×4 ✓；attr13=7=总敏捷 ✓；attr17=9=体力 ✓；attr30=1432=640+72×(等级+10) ✓；attr31=200=MP上限默认 ✓。

**角色面板映射（CharacterInfo_InfoDraw 0x1494fc 反汇编 + 跳转表 + hook 文本序列）**：

| 面板项 | 文本 | 取值 |
|---|---|---|
| DMG | 35185 | CHAR_GetNormalDamage(0xe0bd0) |
| M.DMG | 35186 | CHAR_GetNormalMagicDamage(0xe0c54) |
| CRT | 35189 | CHAR_GetAttr(0) |
| H.RATE | 35190 | CHAR_GetHitRate1000(0xe0e64) |
| C.DMG | 35191 | CHAR_GetAttr(3) |
| DEF | 35187 | CHAR_GetAttr(17) |
| P.RES | 35192 | CHAR_GetDisplayDamageReduceRate(0xe0ce4) = attr17×(attr18+1000)/10000 |
| M.RES | 35193 | **CHAR_GetAttr(11) ← attr11=魔法抵抗** |
| EVD | 35194 | CHAR_GetNormalEvasionRate1000(0xe0db8) = attr13+attr12+等级 |
| W.D.R | 35195 | CHAR_GetAttr(19) |

面板文本段 35160+：35160等级/35161种类/35162职业/35163攻击力/35164防御力/35182EXP/35183HP/35184MP/35185-35199 如上。

**CHAR_GetHitRate1000(0xe0e64) 公式**：命中×1000 = attr14 + 100 + (attr15+20)×1000÷(attr15+目标敏捷+25)；无目标用 level+10 → **attr14=命中基数、attr15=命中率百分比**。

**CHAR_GetAttr 完整分支（0xdfd18）**：已更新→直读 `[ch+0x24+id*4]`；id 20(0x14)=副手武器攻击（读 slot6 装备→类别→[ch+0x74]）；id 28(0x1c)=等级表驱动公式（`[0x2f3000+0xe70]` 索引 u8×9 → `[0x2f5000+0x5a0]` 表数据 u16 公式text → CAL_Calculate）→ 黑魔导公式 text[9]='960a36*+10/'=(960+36×等级)/10，LV1=99；id 30(0x1e)=HP上限 `[ch+0x9c]`（公式 text[1]='640 72a*+'=640+72×(等级+10)，LV1=1432）；id 31(0x1f)=MP上限 `[ch+0xa0]`；id 113(0x71)=max(物攻,魔攻)→`[ch+0x1e8]`；id 11/12 clamp 750（75.0%）。

**默认属性表（CHARSYSTEM_GetDefaultAttributeValue，22 条 × 4B：+0 attr_id/+1 职业位掩码/+2 默认值 int16）**：attr0=30、attr3=1000、attr16=8(classmask 5/2)、attr19=30(5)、attr21=1000(7)、attr28=24(5)、attr29=8(7)、attr31=200(5)、attr32=1000(5)、扩展 id 45/91/93/94/95=1000。

**最终 attr 名称清单**：
`0=暴击率×10` `3=暴击伤害×10` `4=攻击` `8=魔攻` `11=魔法抵抗` `13=敏捷(总)` `14=命中率基数` `15=命中率百分比` `17=防御` `18=物理减伤系数` `19=武器格挡率×10` `20=副手武器攻击` `28=等级驱动属性(960+36×lvl)/10` `29=?(默认8)` `30=HP上限` `31=MP上限` `113=总攻击`
未确认（LV1 全 0）：1/2/5/6/7/9/10/12/16/21/22-27——其中 21 默认 1000（×10=100%？格挡/抵抗类）、16 默认 8。

### R2 max_level 权威路径（✅ 完全闭环）

- **技能信息表 `[0x2f4000+0x9e0]`（recsize `[0x2f6000+0x150]`=32B）recN↔action N 直接索引**：`+0=技能名text_id(=1220+rec)`、`+0x1D=int16 等级参数`（负=无；rec0-19 全 -1）
- CHAR_GetActMaxLevel(0xe9560)：表1[action]+0x1D → 表2`[0x2f3000+0x758]`(recsize `[0x2f6000+0xe68]`=11)记录+9=角色偏移 → 读 `[ch+偏移+0x2B2]` **bit1-4（4 位，UTIL_GetBitValue(byte,4,1)，end/start 顺序）** = 最终 max_level
- 黑魔导凯恩 action50：表1 rec50+0x1D=15 → 表2[15]+9=0 → [ch+0x2B2]=0x10 → bit1-4=8 → CHAR_GetActMaxLevel(50)=8 ✅
- **4 vs 8 实测**：写 [ch+0x2B2] bit1-4=4 → 0x08（写入成功，探针线程随后 crash 但游戏存活）。**bit1-4=实际 max_level（常规 4 / 技能书 8）**，技能书经 CHAR_ProcessSkillBook 提升

### R3 技能使用位域（✅ 闭环）

- `[ch+0x3a0]`：bit0-3=技能使用、bit4-7=自动反击（实测写 0x35 正常）
- 技能节点（C_SKILL_LIST=0x2A0，0x18 步长）：+0 action_id、+2 level、+3=4 恒定、+4=技能ID、+7=**1 恒定**（全部节点 = 激活标志）
- **native 无单技能档位**——只有全局位域 `[ch+0x3a0]`，节点 +7=1 恒为激活。凯恩技能链表：action=0,1,2,3,50,10 全 level1

### R4 佣兵槽（✅ 闭环，data-sources 88 为误读）

- **槽数 = 21**：`*(*(0x2f3978))`=21（GOT 槽值 0xf65c4758 是指针地址；88 是 0x58 十六进制局部误读）
- 槽数组 `*(*(0x2f6010))` 双层解引用，20B/槽：+0 type、+0x0B flags（bit0=占用 bit1=在队伍）
- 角色 +0x352=s8 槽索引（凯恩=0）；party 数组 [0x728ec0] 3 指针
- 新档仅 slot0 占用（flags=0xDB type=0）；`/api/character/mercenary/list` 返回 `{"slots":[0]}`

### R5 装备槽位（✅ 闭环）

- CHAR_FindEquipSlot(0xe4fd0) 前置 CHAR_CanEquipItem(0xe4eb4)：item type bit6-15 类别 → ITEMCLASSBASE(数据 `[0x2f4000+0xcf0]`、大小 `[0x2f5000+0x308]`=23B 运行时)记录+2 索引 → 槽位表(数据 `[0x2f5000+0xb60]`、大小 `[0x2f3000+0x418]`)+4=槽位
- **实测**：基础法杖(type=30088)→findEquipSlot=5（主手）；漆黑之皮甲(type=54188)→findEquipSlot=3（身体）；与 `/api/character/party/0` equipment 数组一致（10 项，槽3=身体/槽5=主手）
- ITEMDATABASE.json 索引=物品 category（rec470='基础法杖'）

### S1 ITEMOPTINFOBASE 打包修复（✅ 已实施并复验）

- `scripts/parse/package_assets.py` INCLUDE_TABLES 末尾加 `"ITEMOPTINFOBASE"`（28→29 张）→ 重跑打包 → `module/assets/static-data/tables/ITEMOPTINFOBASE.json` 出现
- build.gradle.kts versionCode 106→107、versionName 0.5.0→0.5.1 → 构建 `output/inotia4-export-module-v0.5.1.apk` → 部署 → 重启
- **复验通过**：`op/inventory/add category=846/816/790` 造带词缀装备 → `/api/item/inventory` 输出 option_names 非空：`["敏捷","体力","瞬间恢复","武器格挡率"]`、`["智力","精力","暴击率","回避率"]`、`["力量","盾牌格挡率","暴击率","体力"]`
- **词缀编码实测**：0=力量/1=敏捷/2=体力/3=智力/4=精力/5=暴击率/9=回避率/10=盾牌格挡率/11=武器格挡率/27=瞬间恢复（与 §7.5 编码一致）

### S2/S3/S4（✅ 实现依据闭环）

- **S2**：CHARCLASSBASE +0x00=职业名 text_id=class_idx×2（黑魔导=4 已验证）
- **S3**：技能名 = 技能信息表 rec+0 u16 = text_id = 1220+rec_index（凯恩 action50 → text[1270]='痛苦之击' 已验证）；max_level 权威路径见 R2（**非 SKILLDESCBASE**，backlog 假设修正）
- **S4**：MERCENARYINFOBASE +0x04=佣兵名 text_id（35752+idx）

### D1/D2/D3（✅ 已闭环）

- D1：static-data.md §7.2 修正 +0x00=职业名（实机无变化，静态证据充分）
- D2：L63 恒空矛盾由 S1 修复消除
- D3：槽数 21 实测确认（88 为误读），data-sources 需修正

## 实机新增 GOT 槽/表地址

| 地址 | 语义 |
|---|---|
| `0x2f3000+0xe70` / `0x2f5000+0x5a0` | 等级表驱动属性公式表（索引 u8×9 → 数据 u16 公式 text） |
| `0x2f3000+0xc38` / `0x2f5000+0xa18` / `0x2f6000+0xe38` | 默认属性表（记录数/大小/数据） |
| `0x2f5000+0x5b0` / `0x2f3000+0xb08` | 装备词缀表（数据/记录大小；记录 +2 类型 +3 目标 attr） |

### N1 补充：attr 采样（2026-08-13 二轮，LV10 黑魔导 + 词缀装备）

**采样序列**（v0.5.4，frida attr_dump 探针）：
1. LV1 基线 → 2. set-level 10 → 3. grow stat 力量+1 → 4. 造/穿 790 漆黑之剑（词缀 力量2/盾牌格挡率20/暴击率20/体力1）

**LV10 + 790 剑 only**：`main_stats=8,7,10,15,11`（力量 5+1+2、体力 9+1）
`attr=90,0,0,1000,31,0,0,0,45,0,0,0,0,7,30,72,8,10,0,30,0,1000,0,0,0,0,0,0,132,8,2160,200`

**结论**：
- **词缀暴击率(5) → attr0 直加（值 20 → +20，与值完全相等）** ✅（无锐利前缀时干净验证；「锐利的」前缀 = 额外 +100 暴击率×10 = +10.0%）
- **词缀力量/体力（0/2）→ 主属性 stat 数组**（力量 +2、体力 +1 → attr13/17/30 全链重算）；武器格挡率(11)→attr19 直加（前述）
- **attr14（命中基数）完全来自武器**：黑魔导杖 80 / 漆黑之剑 30 / 无武器 0——与盾牌格挡率词缀关系未分离
- attr28=132=(960+36×10)/10、attr30=2160=640+72×20 ✅ 等级驱动公式二次验证
- attr1,2,5,6,7,9,10,12,16,21,22-27 在「LV10 + 词缀装备 + 主属性加点」下**仍全 0** → 与等级/装备/主属性无关（条件触发类：Buff/技能/特定职业）
- **attr12 = 回避率**（强证据：面板 EVD 公式 = attr13+attr12+等级，attr12 clamp 750=75.0% 与回避率语义吻合）——词缀回避率(9) 预期 → attr12（816 法杖因等级限制未穿成，未实机）

**⚠️ 新发现 bug：set-level ≥ 15 导致游戏崩溃**（SIGSEGV HTTP-worker；LV10 正常，LV15/20 均崩）——疑 CHAR_InitializeFromLevel 边界逻辑；**op attr 直写主属性后 save failed**（内存与存档结构不一致）——采样状态勿落盘。

## N3/N4 补充（2026-08-13 第二波，frida 运行时表 dump + 反汇编）

### N3 MAXLEVELBASE 语义（✅ 结构定案，引用点待定）
- **符号**：MAXLEVELBASE_pData @0x301620(8B)、nRecordSize @0x301628(=4)、nRecordCount @0x30162a(=48)——.bss 直接符号地址（base+0x301620）
- **结构（frida 运行时 dump 与静态一致）**：48 = **6 职业 × 8 档**。+0 u16 = 职业索引(低字节：0,1,4,2,3,5 顺序=职业 0-5) | 档位(高字节：0,1,2,8,3,4,5,6)；+2 u16 = 装备名 text_id
- **档位序列 0,1,2,8,3,4,5,6**（8 在 3 前）：档 0-6 + 8。装备名：档0=322(职业0)/285(1,2)/286(3,4)/322(5)、档1=363 全职业、档2=453 全职业、档8=417 全职业、档3=268(0,5)/250(1,2)/232(3,4)、档4=381 全职业、档5=92(0)/74(1)/194(2)/156(3)/175(4)/121(5)、档6=399(仅职业0)/-1(其余)
- **语义假设**：职业 × 技能等级档 → 奖励/初始装备（每档 1 件）。引用点未定位（GOT 间接引用），待 CHAR_InitializeFromLevel/升级链确认
- 注：max_level 权威路径与 MAXLEVELBASE **无关**（R2 已定：技能信息表 + [ch+0x2B2] bit1-4）

### N4 MERCENARYINFOBASE ↔ 佣兵索引（✅ 完全闭环）
- **槽 type 与 MERCENARYINFOBASE 索引无关**：槽 +0x00 type = 角色 type（0=英雄/1=佣兵/2=特殊NPC）——MERCENARYSYSTEM_Set(0x118b94)/AddCharacter(0x118c10)/SetLocation(0x118d80) 三处反汇编证实：槽 +0x00=type、+0x02=char[+0x0A] name_id、+0x01=char[+0x0D]、+0x04/+0x06/+0x08=x/y/z
- **佣兵身份 = name_id（char+0x0A）**，经 CHAR_GetName(0xd9c54)：type=0 英雄名表（数据 GOT[0x2f6000+0x538]，**name_id×130B/条**，+0=名字 text）、type=1 佣兵名表（GOT[0x2f6000+0x598]）、type=2 特殊NPC
- **MERCENARYINFOBASE = 静态佣兵模板表（47×8B，符号 @0x301590/0x301598/0x30159a，运行时与静态一致）**：+0=特性/初始装备 text_id、+2=职业索引(高字节)|变体(低字节，rec0-5=职业0-5变体0、rec6-11=变体1、rec12-19=人名佣兵变体2/3)、+4=佣兵名 text_id（35752+idx）、+6=特性参数（职业佣兵 -1；人名佣兵 33/0/8/18/46/58/70/84）
- **name=null 槽成因**：槽占用（flags bit0）但角色池 find_char_by_merc_slot（按 +0x352）匹配失败（垃圾槽/角色池扫描范围）——非缺联查函数；模块 mercenaries 端点 name 已用 CHAR_GetName（正确路径）
- **CHAR_SetActMaxLevel(0xe9614) 补证 R2 写入侧**：表1[action]+0x1D → 表2 偏移 → UTIL_SetBitValue([ch+0x2B2+偏移], 4, 1, new_val) → 读回验证——技能书提升用的就是它
