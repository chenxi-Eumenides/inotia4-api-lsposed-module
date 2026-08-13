# 背包物品系统逆向笔记（Inventory）

> 目录：docs/systems/ ｜ 主题：背包结构/物品操作（使用/丢弃/装备/出售/移动/镶嵌）全部逆向结论（唯一归属）
> 关联：docs/control-capability.md（函数签名总表）、docs/api-reference.md §0.4/§3.1（端点规格）

## 1. 已实现端点

| 端点 | 函数链 | 版本 | 验证 |
|---|---|---|---|
| `/api/item/inventory/use-item` | **4 路分派**（v0.4.20-22）：IsDice→掷骰预览（不应用，返回变化量）/ IsSealed→ReleaseSealed+Consume / IsItemBox→OpenItemBox+Consume / 其余→CHAR_UseItemEx(0xeb670) | v0.4.20 | ✅ 真机（药水 50→1504 满血、CD 反馈） |
| `/api/item/inventory/dice-accept` | 接受掷骰结果：`STATUSDICE_Apply` 前两步（循环 `CHAR_SetStatBase` + 清 flag），返回 base/applied/delta | v0.4.22 | 待真机 |
| `/api/item/inventory/dice-reject` | 拒绝掷骰结果：仅清 flag（bit0），不应用、不消耗（骰子已在掷时消耗） | v0.4.22 | 待真机 |
| `/api/item/inventory/discard` | `INVEN_RemoveItemDirect`(0x103fd8) | 早前 | ✅ 真机 |
| `/api/item/inventory/{role}/equip` | `CHAR_CanEquipItem`(0xe4eb4) + `CHAR_EquipItem`(0xe51c0) | 早前 | ✅ 真机 |
| `/api/item/inventory/{role}/unequip` | `CHAR_UnequipItemToInven`(0xe2f68) | 早前 | ✅ 真机 |
| `/api/item/inventory/sell` | `ITEM_GetPrice`(0x109f50) + `INVEN_RemoveItemDirect` + `INVEN_AddMoney`(0x1044e4) | v0.4.3 | ✅ 真机 |
| `/api/item/inventory/move` | `INVEN_MoveItem`(0x104934) | v0.4.4 | ✅ 真机 |
| `/api/item/inventory/{role}/jewel` | `ITEMSYSTEM_PutJewel`(0x10bcb4) | v0.4.6 | ✅ 真机 |

## 2. 背包结构（✅ 已破解）

```
INVEN_pItem (0x7131c0) = 6 袋 × 0x80 步长，每袋 16 槽 × 8B 物品指针
物品指针 = INVEN_pItem + bag*0x80 + slot*8（0 = 空槽）
物品结构：+0x08 类型位域(u16) / +0x10 数量位域(u32) / +0x19 宝石位域
```
- 物品类别：`UTIL_GetBitValue(item+0x08, 15, 6)` = `(typeFlags >> 6) & 0x3FF`
- 物品数量：`UTIL_GetBitValue(item+0x10, 31, 25)`
- 物品稀有度：`ITEMSYSTEM_GetRarity`(0x10d700)
- 名称联查：category = (typeFlags >> 6) & 0x3FF = ITEMDATABASE itemId → 静态表 text_0

## 2.4 物品对象完整数据结构（品质/词缀/附魔/强化/宝石孔，✅ 2026-08-12 反汇编确认）

> 本节为物品对象的**权威位域定义**（全字段反汇编实证，覆盖此前 §2 零散记录）。来源函数见每行标注。

### 2.4.1 物品对象位域总表

```
物品对象（槽数组存 8B 指针指向对象首地址，对象间隔 0x28）：
+0x08  u16  type 位域：bit2-5=稀有度位、bit6-15=类别（category = type>>6 & 0x3FF = ITEMDATABASE itemId）
+0x10  u32  count/混沌/宝石 位域（按物品类别复用不同 bit）：
           bit0-7   混沌等级    （ITEM_GetChaosLevel 0x105c5c: GetBitValue(7,0)）
           bit8-15  混沌值率    （ITEM_GetChaosValueRate 0x105c38: GetBitValue(15,8)，100=无混沌）
           bit18-23 宝石属性 id（ITEMSYSTEM_PutJewel 0x10bcb4 读 gem: GetBitValue(23,18)，=词缀索引）
           bit0-10  宝石数值    （PutJewel 读 gem: GetBitValue(10,0)，11 bit）
           bit25-31 数量/累计数 （ITEM_GetCumulateCount 0x106094 ✅v0.5.12 完整反汇编：可堆叠类读 GetBitValue(31,25) 实际数量；**不可堆叠类（装备）直接返回 1**——API count 已按此输出，旧版裸读位域装备错显 100）
+0x18  u8    magicRate 魔法伤害倍率（物理伤害 ×此值/100）
+0x19  u8    socket 位域：
           bit0-3  已镶宝石数   （PutJewel: GetBitValue(3,0)；写回 +1 用 SetBitValue(3,0)）
           bit4-7  总插槽数     （ApplySocket 0x10d8a4: SetBitValue(7,4)；PutJewel 校验 GetBitValue(7,4)）
+0x1A  u16   附魔/强化 位域：
           bit0     混沌标志    （GetChaosLevel/GetChaosValueRate: tbnz bit0）
           bit2-5   强化状态标志（EnchantItem 0x10b330 特级/普通卷轴路径检查，语义待 P3 确认）
           bit6-10  附魔/强化等级（ApplyEnchantValue 0x109890 / EnchantItem 写回: GetBitValue(10,6)，5 bit）
           bit11-15 附魔ID      （ApplyEnchantValue: GetBitValue(15,11)，5 bit）
+0x20  ptr    词缀链表头（节点见 2.4.3）
```

> ⚠️ **修正旧记录**：此前 §5/§2 记 socket「bit0-2 已镶/bit4-6 插槽」为 3-bit 位域 → 反汇编证实为 **4-bit**（bit0-3/bit4-7）；此前记 enchant「bit5-6 附魔等级/bit10-15 附魔ID」→ 实测 **bit6-10 等级/bit11-15 ID**。

### 2.4.2 品质（稀有度）链路

- 稀有度位 = type bit2-5（4 bit，0-15）→ `ITEMSYSTEM_GetRarity`(0x10d700) 查表返回最终档
- **品级前缀** = `ITEMGRADEBASE`（15 条 × 13B，text 表已全量解析）：

| rarity | +0x00 前缀 | +0x02 增强前缀 | 示例 |
|---|---|---|---|
| 0 | 1084 生锈的 | 1085 生锈的 | 生锈的 短剑 |
| 1 | 1086 陈旧的 | 1087 陈旧的 | 陈旧的 短剑 |
| 2 | 1088 （空） | 1089 （空） | 短剑（标准品，CreateItem 默认） |
| 3 | 1090 太古的 | 1091 太古的 | 太古的 短剑 |
| 4 | 1092 锐利的 | 1093 优质的 | 锐利的 短剑 |
| 5 | 1094 打磨的 | 1095 坚固的 | 打磨的 短剑 |
| 6 | 1096 工匠的 | 1097 工匠的 | 工匠的 短剑 |
| 7 | 1098 钢铁 | 1099 高级 | 钢铁 短剑 |
| 8 | 1100 钛金 | 1101 耀眼的 | 钛金 短剑 |
| 9 | 1102 秘银 | 1103 神秘的 | 秘银 短剑 |
| 10-14 | 1104-1113 （空） | | |

- 5 档稀有度表 `ITEMRARITYGRADEBASE`（5 条 × 15B）：**白/绿/蓝/黄/紫**（用户 2026-08-12 确认游戏品质仅此 5 档；GetRarity 返回值 0-4 直接映射档位，v0.5.12 API 注入 `rarity_tier`，字段语义待 P3）
- 品质→孔数（wiki 资料，待逆向确认）：白4/绿3/蓝2/黄0/紫0

### 2.4.3 词缀（options）体系

**词缀节点**（0x18B 双向链表，`ITEM_AddOptionEx` 0x105ec4 构造）：

```
+0x00  u16  编码：bit0-6=词缀索引（ITEMOPTINFOBASE 记录下标 0-36）、bit13-15=type（普通词缀=0、宝石词缀=1）
+0x02  s16  词缀数值（ITEMSYSTEM_GetOptionValue 0x109020 计算，API options 数组输出此项）
+0x04  u32  随机种子 seed（数值缩放系数，见下）
+0x08  ptr  前一节点
+0x10  ptr  下一节点
```

> ✅ **v0.4.64 已修复**：game_read.cpp append_item_attrs 输出 `(id & 0x7F)` 作 option_ids（词缀索引）+ `O_VALUE` 数值作 options；`StaticData.buildOptionNames` 改用 zh-Hans 文本数组解析，词缀名注入成功（真机：短剑 [力量,敏捷,体力]、真实之链 [体力,暴击伤害增加率]）。

**ITEMOPTINFOBASE**（37 条 × 12B，词缀定义表）：

| 偏移 | 类型 | 语义 | 依据 |
|---|---|---|---|
| +0x00 | u16 | 词缀名 text_id（1114-1150，全量名称见下） | StaticData 联查 |
| +0x02 | u16 | 属性类型编码（主属性 0x00/0x100/0x200/0x300/0x400 = 力/敏/体/智/精；其余 0xX01 高字节=内部属性 id） | 分布分析 |
| +0x04 | u16 | 数值公式 text_id（MEMORYTEXT_GetText_E → CAL_Calculate 计算） | GetOptionValue 0x109020 |
| +0x06 | u8 | 等级要求（≥ 装备能力等级才可生成） | MakeOptionEx cmp flag |
| +0x07 | u8 | 标志（bit1=1 排除候选） | MakeOptionEx tbnz bit1 |
| +0x08 | u32 | 稀有度位掩码（1<<rarity，tst 判定） | MakeOptionEx tst w26 |

词缀名全量（text 表直接完整名称，非单字拼接）：

```
1114 力量 1115 敏捷 1116 体力 1117 智力 1118 精力 1119 暴击率 1120 命中率
1121 暴击伤害抵抗率 1122 魔法抵抗率 1123 回避率 1124 盾牌格挡率 1125 武器格挡率
1126 MP增加 1127 MP恢复 1128 暴击抵抗率 1129 暴击伤害增加率 1130 HP吸收
1131 火 1132 风 1133 寒气 1134 神圣 1135 黑暗 1136 毒 1137 重力摆 1138 冰霜
1139 治愈气息 1140 狂战士 1141 瞬间恢复 1142 魔力专家 1143 减少敌意值
1144 眩晕抗性 1145 睡眠抗性 1146 失明抗性 1147 恐惧抗性 1148 减速抗性 1149 沉默抗性
1150 0
```

**词缀生成链**（`ITEMSYSTEM_MakeOption` 0x10dbe4 → `MakeOptionEx` 0x10928c）：
1. 物品类别查 ITEMDATABASE 记录 +6 字节 bit1：非可生成词缀类 → 返回 1
2. 稀有度位查表得该档词缀**数量**（OPTINFOBASE 表第 rarity 条 +2 字节）；饰品（ITEMDATABASE_IsAccessory）+1
3. MakeOptionEx：遍历 ITEMOPTINFOBASE 全部记录筛候选（+7 bit1=0、+6≤能力等级、+8 位掩码含 rarity 位、+4 公式值>0）→ 随机选 → 去重 → 每词缀 `GetOptionValue(索引, 能力等级, seed, item)` 算值 → `ITEM_AddOptionEx(item, 0, 索引, 值)` 建节点

**词缀值计算**（GetOptionValue）：基础值 = CAL_Calculate(公式 text_id 文本, 能力等级)；seed 缩放 `×(100-(seed-1)×10)%`（seed=1 不缩放）；最终随机 ∈ **[基础值/2, 基础值]**（MATH_GetRandom(w0=值/2, w1=值)）。

**静态词条表** `ITEMSTATICOPTBASE`（1409 条 × 5B）：`[item_id(u16), option_编码(u16+u8)]` 物品固定词条映射（如短剑→力量/敏捷/体力 3 词条，条目值 0xCE00-0xCE0F/0xDD01-0xDD16 段），✅ v0.5.12 确认编码低字节=词缀索引 0-35（见 2.4.6）。

**附魔数据位置**（✅ 2026-08-14 确认，⑨ 结论；xlsx `apk/附魔属性对照表.xlsx` 已解析入 /tmp/opencode/enchant_table.json）：
- 附魔属性名 = ITEMOPTINFOBASE 词缀索引 0-35（力量0/敏捷1/体力2/.../沉默抗性35，text 1114-1150 全量见上）
- 加分规则 = xlsx B6:E41 属性加分表（C 列单次加分，D/E 列 1级/105级数值）
- 附魔结果 = xlsx G/H 普通附魔表（score 0-168，13 词条循环）与 J/K 完美附魔表（score 0-167，6 抗性词条循环）；机制：镶嵌 4 宝石后按词条+宝石属性加分 → 总分查表取普通/完美附魔（普通大概率/完美小概率）
- ⚠️ L 列博主实测数据不保证正确，仅作参考不据此实现

### 2.4.4 附魔 / 强化体系

**位域**（+0x1A）：bit11-15=附魔ID、bit6-10=附魔等级、bit0=混沌标志、bit2-5=强化状态标志。

**强化计算**（`ITEMSYSTEM_ApplyEnchantValue`(0x109890)：返回 `原值 + 加成`）：
- 附魔ID = GetBitValue(+0x1A, 15, 11)；≤0 或 > 上限表 `[0x2f3000+0x2f0]` 直接返回原值
- 附魔等级 = GetBitValue(+0x1A, 10, 6)
- 查 ITEMENCHANTBASE 记录(ID-1)：+6=基础值、+7=上限、+8=溢出系数
- 加成 = 基础值×等级；等级超上限 → 修正 `基础值×等级 + (等级-上限)×(系数+1)`

**ITEMENCHANTBASE**（32 条 × 9B，附魔/强化参数表）：

| 偏移 | 类型 | 语义 |
|---|---|---|
| +0x00 | u16 | 附魔类型/卷轴类别组（16-25 强化卷轴、946/947） |
| +0x02 | u16 | 档位（1/14/496 或 0xF200/1024/2048 两组） |
| +0x04 | u16 | 0 或 1 或 0x3e |
| +0x06 | u8 | **基础值** |
| +0x07 | u8 | **等级上限** |
| +0x08 | u8 | **溢出系数** |

**强化卷轴执行**（`ITEMSYSTEM_EnchantItem` 0x10b330，✅ v0.5.12 API 已实现 `POST /api/item/inventory/{role}/enchant`）：
- 前置：ITEMDATABASE 记录 +2 字节查表 bit0/bit2（可强化标志）+ `ITEMSYSTEM_IsEnchantScroll`(0x10b2f0 = IsWeaponEnchantScroll||IsDefenseEnchantScroll)
- 卷轴类别判定：**0x14(20)/0x19(25) = 混沌武器/防具卷轴走特级路径**，其余普通路径；两条路径对 +0x1A bit2-5 与当前等级检查不同（特级要求等级≠0 且 bit2-5==0；普通要求 bit2-5>0）
- 成功概率 = 公式(CAL_Calculate) vs MATH_GetRandom(1, 998)
- 写入：`SetBitValue(+0x1A, 10, 6, 新等级)` + `strh` 写回
- **EnchantItem 不消耗卷轴**，消耗在调用方 UIEquip_ApplyStuff(0xb8df8) 成功分支 `INVEN_ConsumeItem`(0x1047bc)——API 复刻该分支（成功才消耗，真机验证卷轴 3→2）
- 强化卷轴类别：16-19 低级~顶级武器、20 混沌武器、21-24 低级~顶级防具、25 混沌防具

### 2.4.5 宝石孔体系

**位域**（+0x19）：bit0-3=已镶数、bit4-7=总孔数。

**宝石类别** = **[28, 32]**（`ITEMSYSTEM_IsJewel` 0x10b964：`w0-0x1c ≤ 4`）：28 低级宝石/29 中级/30 高级/31 顶级/32 混沌宝石。

**宝石物品 +0x10 位域**：bit18-23=宝石属性 id（词缀索引）、bit0-10=宝石数值。

**镶嵌链**（`ITEMSYSTEM_PutJewel` 0x10bcb4，✅ v0.4.6 API 已实现）：
1. 装备类别 → ITEMDATABASE +2 字节 → `[0x2f3000+0x418]` 表 bit0（可镶嵌标志），不通过返回 3
2. `ITEMSYSTEM_IsJewel` 宝石类别校验（非宝石返回 3）
3. 总孔数 = GetBitValue(+0x19, 7, 4)；已镶数 = GetBitValue(+0x19, 3, 0)；**已镶 ≥ 总孔 → 返回 2（无孔）**
4. 宝石属性 id = GetBitValue(gem+0x10, 23, 18)、宝石数值 = GetBitValue(gem+0x10, 10, 0)
5. `ITEM_AddOptionEx(equip, 1, 属性id, 数值)` 加宝石词缀（type=1）
6. 成功 → 已镶数 +1（SetBitValue(+0x19, 3, 0)）
- ⚠️ 不消耗宝石物品本身——API 手动 INVEN_RemoveItemDirect 删除防刷（v0.4.6 验证）

**打孔链**（`ITEMSYSTEM_ApplySocket` 0x10d8a4，盗版大修合成器可重复开孔）：
- 概率 = 公式(CAL_Calculate) vs MATH_GetRandom(0,99)；失败不写
- 孔数 = 随机落在累计阈值 [35, 30, 20, 15]（1孔35%/2孔30%/3孔20%/4孔15%）的区间 → 写 `SetBitValue(+0x19, 7, 4, 孔数)`
- 熔炉版 `ITEMSYSTEM_ApplySocketForMixure` 0x10da64 同语义

### 2.4.6 静态词条映射（ITEMSTATICOPTBASE）

1409 条 × 5B：`+0 u16 item_id`（ITEMDATABASE 索引，如 75=短剑、76=匕首）+ `+2 u16/+4 u8` 词条编码。✅ **v0.5.12 编码结构确认**：`+2 u16` 低字节 = **词缀索引 0-35**（与 ITEMOPTINFOBASE 记录下标对齐：0xCE00/0xCE01/0xCE02=力量/敏捷/体力），高字节 + byte4 = 数值分组（CE 高字节+50、DD 高字节+35）；item 120 存在重复低字节条目。短剑(75) 固定 3 词条（力量/敏捷/体力），物品静态词条 + 掉落随机词缀共同构成装备属性。API v0.5.12 注入 `static_options`（去重聚合）。

## 2.5 使用物品分派体系（✅ v0.4.20 反汇编完整梳理）

> 权威分派依据 = `UIEquip_SetDescMenu`(0xb8504) 背包右键菜单按钮判定链（disasm 行 43391-43678）。
> 按钮判定从上到下 if/else，命中即显示对应按钮；按钮执行函数不再重复判定类别。

### 2.5.1 UI 按钮判定链（权威类别归属）

| 按钮 | 判定函数 | 类别范围 |
|---|---|---|
| 装备 | `ITEM_IsRealEquip`(0x105ab8) + `CHAR_CanEquipItem`(0xe4eb4) | 真装备且当前角色可穿 |
| 使用（快捷） | `ITEMSYSTEM_IsShortcutUse`(0x10cd04) | ITEMDATABASE 记录 off7 bit0 或 off6 bit5，或 IsReviveScroll |
| 佣兵卡 | `ITEMSYSTEM_IsMercenarySeal`(0x10be70) | [0x2a,0x32] ∪ [0x3a0,0x3a5] |
| 掷骰 | `ITEMSYSTEM_IsDice`(0x10be60) | **[0x34,0x38]** = 52~56 |
| 使用（确认后） | `ITEMSYSTEM_IsUseAfterConfirm`(0x10cca4) | {0x33, 0x39d, 0x39e} ∪ {0xf 超药水} |
| 使用 | ITEMDATABASE off7 bit1 或 id==0x3e | — |
| 使用（解包） | `ITEMSYSTEM_IsPackItem`(0x10cdb0) | PackItem 表遍历匹配 |
| 解封 | `ITEMSYSTEM_IsSealed`(0x10be50) | **[0x3a6,0x3ab]** = 934~939 |
| **开箱** | `ITEMSYSTEM_IsItemBox`(0x10cda0) | **[0x3ef,0x3f1]** = 1007~1009 |

⚠️ 开箱判定是 `IsItemBox`（0x3ef-0x3f1），**不是 IsPackItem**（那是"打包物"走 ProcessUnpack 解包）。

### 2.5.2 CHAR_UseItemEx(0xeb670) 覆盖的类别（走"使用"按钮路径）

签名 `int (void* ch, void* item, int flag)`，类别提取 = `UTIL_GetBitValue(item+8, 0xf, 0x6)`。返回 1=成功（内部已调 INVEN_ConsumeItem）、0=失败（CD/状态不符，不消耗）。

| 类别 | 物品 | 分派 |
|---|---|---|
| 5-8 | 恢复药水 | ITEMCLASSBASE off6 bit5 → `FindInRecover` + CAL_Calculate 公式 + `PARTY_AddHPMP`(AsRate) |
| 9 | 魔法药丸 | 快速回血路径 0xeb8d0 |
| 26/27 (0x1a/0x1b) | 复活卷轴 | `CHAR_ProcessReviveScroll`(0xde3d8) |
| 51 (0x33) | 技能书 | `CHAR_ProcessSkillBook`(0xe2488) |
| 62 (0x3e) | 佣兵封印卡 | `CHARSYSTEM_Produce`(0xf3880) + `CHAR_AppearFromCharacter` |
| 925 (0x39d) | 技能重置 | `CHAR_InitializeSkill`(0xe67c8) |
| 926 (0x39e) | 属性重置 | `CHAR_InitializeStatus`(0xe68c8) |
| 15 (0xf) | 超级药水 | `ITEMSYSTEM_IsSuperPotion` → `CHAR_ProcessSuperPotion`(0xdcc88) |
| 默认 | 配方书 | ITEMCLASSBASE off7 bit1 → `CHAR_ProcessRecipe`(0xdcde4) |
| 默认 | 增益类 | ITEMCLASSBASE off7 bit0 → `FindInBuff` + `CHAR_CreateBuff` |
| 默认 | 打包物 | `ITEMSYSTEM_IsPackItem` → `ITEMSYSTEM_ProcessUnpack`(0x10ce50) |

### 2.5.3 独立路径（不在 CHAR_UseItemEx 内，API 前置分派）

| 类别 | 判定 | 执行链 | API 状态 |
|---|---|---|---|
| 0x34-0x38 骰子 | IsDice | 掷骰=`STATUSDICE_Roll`(0x138338)+消耗；接受=`STATUSDICE_Apply` 前两步；拒绝=清 flag | ✅ v0.4.22 两段式（掷→预览变化量→accept/reject） |
| 0x3a6-0x3ab 解封 | IsSealed | `ITEMSYSTEM_ReleaseSealed`(0x10af4c) 成功→手动 `INVEN_ConsumeItem` | ✅ v0.4.20 |
| 0x3ef-0x3f1 开箱 | IsItemBox | `ITEMSYSTEM_OpenItemBox`(0x10e970) 成功→手动 `INVEN_ConsumeItem` | ✅ v0.4.20 |

**骰子两段式状态机（v0.4.22，反汇编实证）**：
- 掷骰 `use-item`：前置检查 flag（`[0x2f3000+0x7b8]` GOT 槽解引用，bit0=1 已有未确认结果→拒绝）→ 读掷前 base（`CHAR_GetStatBase`）→ `STATUSDICE_Roll`(0x138338) 写 pending（`[0x2f5000+0x740]` GOT 槽解引用 int8[5]）→ `INVEN_ConsumeItem` 消耗 → 置 flag bit0=1。返回 `{base[5],pending[5],delta[5]}`，**不应用**。
- 接受 `dice-accept`：检查 flag→应用 pending 到 leader 基础属性（复刻 `STATUSDICE_Apply`(0x1382b8) 前两步）→清 flag。返回 `{base,applied,delta}`。
- 拒绝 `dice-reject`：检查 flag→仅清 flag。骰子不退回（与原版一致：`UIRollDice_ButtonRollExe`(0xcc970) 掷即消耗，`UIRollDice_ButtonCancelExe`(0xcca94) 只关面板）。
- 原版 flag 引用面：仅骰子面板 UI（Roll 置位 / Create+Apply 复位 / Draw 显示 pending / Apply+Cancel 按钮判定）+ STATUSDICE_Apply 复位——**不影响游戏世界操作**；未确认时可正常游戏，但 API 层禁止再掷（防覆盖 pending）。

**掷骰随机语义（v0.4.22 反汇编 + frida 实证）**：
- 随机块在 `0xfd26c`（EVTSYSTEM_Process 共享代码块，编译器 code folding）。基础值 `w28 = w1*w2`（表驱动：w1=5 力量系数、w2=类别倍率，如中级骰子 力量 5×10=50 / 敏捷 5×8=40 / 精力 5×4=20）。
- 判定：`bl 0x14e0cc` 读 `byte_e` = 当前角色等级（`[0x2f6a68]` 双重解引用 +0xe，即 `ch+0xe`）→ `udiv w1, byte_e, #2` → **`byte_e/2 == 0`（等级 0-1）跳过随机**，结果为纯表值（固定）。
- 范围：否则 `MATH_GetRandom(0, byte_e/2)` → 随机整数 ∈ **[0, byte_e/2]**（含端点），**加到**基础值上（单向正浮动，非正负区间）→ `cmp w28, #0x7f` clamp 上限 127。
- 实测（level 1 凯恩，frida）：`byte_e=1 → 1/2=0` → 跳过随机 → 每次掷骰结果恒为表值（中级 [50,40,50,40,20]）；API 掷骰与直接调原生 Roll 结果一致（非自实现）。
- **level 2 实证（frida 改 ch+0xe=2 掷骰）**：`byte_e=2 → GetRandom(0,1)` 被调用，返回 0/1 各约 50%，加到基础值上——力量 50↔51、敏捷 40↔41、精力 20↔21。证明 level 2 起随机即生效，无"2级固定"。
- 等级越高浮动越大：level 4 → `[0,2]`（+0/+1/+2），level 10 → `[0,5]`。

### 2.5.4 恢复表结构（ITEMRECOVERBASE，药水回血数据源）

- 运行时表：stride=`[0x2f5000+0x890]` 字节(9)、表数据=`[0x2f6000+0x850]` **双重解引用**；记录按**顺序索引**（cat5→0, cat6→1...cat14→5），非类别值索引
- 记录 9B：byte0-1=类别(u16)、byte2=标志、byte3-4=文本ID(u16)、**byte5-6=回血下限公式ID(u16)**、byte7-8=回血上限公式ID(u16)
- 公式：formula-e[127]='2400' / [128]='3200'（cat5 恢复药水小）；恢复量 = MATH_GetRandom(下限公式值, 上限公式值)
- 回血前置标志：ITEMCLASSBASE off6 bit5（回血类）+ byte2 bit1（可消耗）；`[ch+0x2c8] & 0x10004`（死亡状态）拒绝
- ⚠️ 踩坑：OpenItemBox 对**任何类别**都返回 1（表记录数==0 时也返回随机+1）——分派时必须先用 IsItemBox 判定，否则药水会被误拦（v0.4.20 真机实证）

## 3. 出售链（✅ v0.4.3，防刷钱设计）

```
ITEM_GetPrice(item) @0x109f50：价格 = 静态表 ITEMDATABASE（item+8 类别 → 价格字段）+ ITEM_GetAbilityLevel 加成
INVEN_RemoveItemDirect(bag, slot) @0x103fd8：按槽删除
INVEN_AddMoney(money) @0x1044e4：加金币（内部溢出检查）
```
**价格由静态表决定，非调用方传入**（防任意定价刷钱）——2026-08-08 审查修正归合法类别。返回 `{"ok":true,"price":N}`

## 4. 移动/堆叠链（✅ v0.4.4）

```
INVEN_MoveItem(item, count, targetBag, targetSlot) @0x104934：
  目标空槽 → ITEMSYSTEM_CopyAsNewUID(0x1083c8) 复制 + INVEN_SaveItemDirect(0x103bf0) 存入
  目标同类 → 堆叠合并（上限 99）
  源数量 UTIL_SetBitValue 写回 +0x10
  返回 w0=1 成功/0 失败；前置限制同 bag 类型 + 类别比较
```
⚠️ INVEN_RemoveItem(0x104044) 语义为 `int(void* item)`（**item 指针**非类别）——v0.4.3 修正旧记录

## 5. 镶嵌链（✅ v0.4.6）

```
ITEMSYSTEM_PutJewel(equipItem, jewelItem) @0x10bcb4：
  装备类别可镶嵌校验（0x2f5000+0xb60 表 bit0）
  ITEMSYSTEM_IsJewel(0x10b964) 宝石校验（非宝石→3）
  装备 +0x19 bit4-6 插槽数 ≤0 → 2
  ITEM_AddOptionEx(0x105ec4) 加属性 → +0x19 bit0-2 已镶数 +1
  返回 0=成功 / 2=无孔 / 3=非宝石或空装备
```
**⚠️ 不消耗宝石物品本身**——API 手动 `INVEN_RemoveItemDirect(bag,slot)` 删除防刷（v0.4.6 真机验证宝石槽清空）
- 装备插槽访问：角色 +0x1F8（C_EQUIP，10 槽×8B 指针）
- 装备宝石位域 +0x19：**bit0-3=已镶宝石数、bit4-7=总插槽数**（2026-08-12 反汇编修正：此前记 bit0-2/bit4-6 为 3-bit，实测 4-bit，见 §2.4.5）
- 宝石类别 = [28,32]（`ITEMSYSTEM_IsJewel`），宝石物品 +0x10：bit18-23=属性 id、bit0-10=数值（见 §2.4.5）

## 6. 相关符号表

| 函数 | VMA | 签名 |
|---|---|---|
| ITEMDATABASE_IsUse | 0x1058ac | int(int32_t) |
| INVEN_ConsumeItem | 0x1047bc | void(void*) |
| CHAR_UseItemEx | 0xeb670 | int(void*, void*, int) |
| ITEMSYSTEM_OpenItemBox | 0x10e970 | int(int32_t) |
| ITEMSYSTEM_ReleaseSealed | 0x10af4c | int(int32_t) |
| ITEMSYSTEM_IsDice | 0x10be60 | int(int32_t) |
| ITEMSYSTEM_IsSealed | 0x10be50 | int(int32_t) |
| ITEMSYSTEM_IsItemBox | 0x10cda0 | int(int32_t) |
| ITEMSYSTEM_IsPackItem | 0x10cdb0 | int(int32_t) |
| ITEMSYSTEM_IsShortcutUse | 0x10cd04 | int(int32_t) |
| ITEMSYSTEM_IsUseAfterConfirm | 0x10cca4 | int(int32_t) |
| ITEMSYSTEM_IsMercenarySeal | 0x10be70 | int(int32_t) |
| ITEMSYSTEM_IsReviveScroll | 0x10ccf4 | int(int32_t) |
| ITEMSYSTEM_FindInRecover | 0x10aa8c | int(int32_t) |
| ITEMSYSTEM_ProcessUnpack | 0x10ce50 | int(int32_t) |
| CHAR_ProcessRecipe | 0xdcde4 | int(int32_t) |
| CHAR_ProcessReviveScroll | 0xde3d8 | int(void*, int32_t) |
| CHAR_ProcessSkillBook | 0xe2488 | int(void*, void*) |
| INVEN_RemoveItemDirect | 0x103fd8 | int(int32_t, int32_t) |
| INVEN_RemoveItem | 0x104044 | int(void*) |
| INVEN_MoveItem | 0x104934 | int(void*, int32_t, int32_t, int32_t) |
| INVEN_SaveItemDirect | 0x103bf0 | int(void*, int32_t, int32_t) |
| ITEMSYSTEM_CopyAsNewUID | 0x1083c8 | void*(void*) |
| ITEM_GetPrice | 0x109f50 | int(void*) |
| INVEN_AddMoney | 0x1044e4 | int(int64_t) |
| ITEMSYSTEM_PutJewel | 0x10bcb4 | int(void*, void*) |
| ITEMSYSTEM_IsJewel | 0x10b964 | int(int32_t) |
| ITEM_AddOptionEx | 0x105ec4 | int(void*, ...) |
| CHAR_EquipItem | 0xe51c0 | int(void*, void*) |
| CHAR_CanEquipItem | 0xe4eb4 | int(void*, void*) |
| CHAR_UnequipItemToInven | 0xe2f68 | int(void*, int32_t) |
