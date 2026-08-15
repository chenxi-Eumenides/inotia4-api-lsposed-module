# 逆向数据源（符号/VMA/结构体偏移/函数语义）

> 日期：2026-08-05 ｜ 输入：`apk/decompiled/`（jadx 反编译）+ `apk/decompiled/libgame-symbols.txt`（readelf 符号表）
> 结论先行：**游戏数据全部在 native（libgame.so，Hercules 引擎），Java 侧无数据镜像**。但 libgame.so **未 strip 符号**（6626 个导出函数 + 大量全局变量），native 数据访问完全可行。

## 1. 架构结论

| 层 | 内容 | 与游戏数据关系 |
|---|---|---|
| Java（com.com2us.*） | MainActivity / CGameManager / CEventHandler / CWrapper* | 仅引擎壳 + UI（GLSurfaceView 渲染）+ 事件队列初始化，**无任何游戏数据** |
| native（libgame.so） | Hercules 引擎 + 全部游戏逻辑 | **金币/角色/背包/地图/技能/存档全部在此** |
| 静态资源 | assets/common/game_res/*.dat.jpg | 数值表/配置/图片（另一条线，M3 解析） |

证据：
- `com.com2us.inotia4.*` 仅 6 个 Java 文件（Activity/配置/资源表）
- 全部 19 个 native 方法声明均为基础设施（引擎初始化、事件队列、网络、传感器、字体），无游戏数据 Getter
- 游戏逻辑函数以直观前缀导出：`INVEN_`、`CHAR_`、`CHARSYSTEM_`、`PARTY_`、`ITEM`、`SAVE_`、`MAP_`、`QUESTSYSTEM_`、`UISkill_` 等

## 2. 数据访问清单（按 API 需求字段）

### 2.1 金币

| 符号 | 地址 | 类型 | 说明 |
|---|---|---|---|
| `INVEN_nMoney` | 0x7134c0 | 全局 int64 | **金币值（直读）** |
| `INVEN_GetMoney` | 0x10445c | 函数 | 获取金币（dlsym 可调用） |
| `INVEN_AddMoney` / `INVEN_MinusMoney` / `INVEN_SetMoney` | — | 函数 | 金币修改（hook 可做变化通知） |

### 2.2 角色（等级/经验/HP/MP/属性/装备/技能）

角色对象入口：`PARTY_pChar`（0x728ec0，24 字节 = **3 个角色指针**）、`CHARSYSTEM_pPool`（0x307538，角色对象池）。**池容量 = 100 个对象**（0x1a2c0 字节 / 0x430 步长；`CHARSYSTEM_Initialize`(0xf32a0)/`ClearAll`(0xf3338)/`Allocate`(0xf33e4)/`Find`(0xf34dc)/`FindAsMercenarySlot`(0xf4254) 反汇编均以「池基址 + 0x1a2c0」为遍历终点，2026-08-15 反汇编确认；模块扫描上限用 `C_CHARSYSTEM_POOL_SLOTS=100`）。

| 符号 | 地址 | 说明 |
|---|---|---|
| `PARTY_GetMember(i)` | 0x11f384 | 按索引取角色指针（i=0..2，经 GOT 0x2f6450 → PARTY_pChar） |
| `PARTY_GetSize` | 0x11f3a4 | **队伍大小（API partyCount 用此）** |
| 等级 | 角色 +0x0E int8 | 结构体直读（`CHAR_GetLevel` 无） |
| 经验 | 角色 +0x318 int64 / +0x320（升级所需） | 结构体直读 |
| HP/MP | 角色 +0x1F0 / +0x1F4；上限 = `CHAR_GetAttr(char,0x1e/0x1f)` | 结构体+函数 |
| 属性 | 角色 +0x24 + attr_id×4 | 结构体直读（`CHAR_GetAttr` @0xdfd18） |
| 装备 | `CHAR_GetEquipItem(char,slot)` @0xda20c | 10 槽，物品 +0x08 类型位域 |
| 技能点 | 角色 +0x328 int8（`CHAR_GetSkillPoint` @0xd9c34） | 剩余技能点直读 |

**技能存储（✅ 2026-08-05 探索逆向完成，`CHARSYSTEM_GetSkillList` 是 stub 但结构已破）**：

| 偏移 | 类型 | 含义 | 验证函数 |
|---|---|---|---|
| +0x2A0 | ptr | **已学战斗技能链表头**：节点 [0x00]=action_id(u16)、[0x02]=level(u8)、[0x18]=next(ptr)、节点 0x1C | CHAR_FindActionForce(0xdb4d0) |
| +0x2B0 | u16 | 技能解锁位图（16 位，每 bit 一技能是否解锁） | CHAR_IsSkillOpen(0xe4d28) |
| +0x2B2 | u8[] | 打包技能等级 nibble 数组（每字节 2 技能 × 4bit） | CHAR_GetActMaxLevel(0xe9560) |
| +0x280 | ptr | 当前激活技能槽指针（节点 [0x00]=action_id） | CHAR_IsSkillOn(0xdb4fc) |
| +0x0D | s8 | 成员索引（匹配全局技能表） | UISkill_MakeSkillInfo(0xcfb3c) |
| +0x3A0 | u8 | **战斗 AI 技能开关（bit0-2，v0.4.10 修正：非"使用次数"）**——0=AI 仅普攻，非 0=AI 用技能 | CHAR_GetSkillUsage(0xe496c)/CHAR_SetSkillUsage(0xe4cc0) |

全局技能定义表：`*(0x2f3758)`（36B/条目：+0x00 member_idx、+0x01 action_id、+0x06 限制、+0x07 上限、+0x09 nibble_index）；技能动作映射 `*(0x2f49e0)`（+0x1D int16）；技能 UI 列表 `*(0x2f6748)`。
动作管理函数：`CHAR_LearnAction`(0xe2390 学习/升级)、`CHAR_FindAction`(0xdd3ac)、`CHAR_ProcessSkillBook`(0xe2488)。
`CHAR_GetName` @0xd9c54：**返回角色名称 UTF-8 字符串**（✅ 2026-08-05 实测：hero="凯恩"、佣兵="沃尔达克/西雷斯/多尔法"）。
> ⚠️ 角色 +0x0A **不是名称 text_id**（英雄 +0x0A=0、沃尔达克 +0x0A=249="死亡之弩"物品名）——名称一律用 `CHAR_GetName(obj)` 获取。

**主属性（✅ 2026-08-05 面板截图+frida 验证）**：

| 函数 | 地址 | 说明 |
|---|---|---|
| `CHAR_GetStat(char, id)` | 0xdf8d0 | 主属性（**总属性 = Base(0xdb9e4) + Main(0xdb9f0) + Bonus(0xdb9fc)**）：**0=力量 1=敏捷 2=体力 3=智力 4=精力**（与属性面板完全一致：96/139/101/54/38） |
| `CHAR_GetStatusPoint(char)` | 0xd9c44 | 剩余能力点（面板"能力点:78"） |
| `CHAR_GetStatMain(ch, i)` | 0xdb9f0 | **读已分配属性** [ch+0x256+i*2]（u16，i=0-4 力量/敏捷/体力/智力/精力）。⚠️ **v0.4.7 修正**：这是「玩家分配的属性点」分量非总属性——总属性 = CHAR_GetStat（Base+Main+Bonus）。LV2 凯恩 Main 全 0（属性全来自基础+装备） |
| `CHAR_SetStatMain(ch, i, v)` | 0xdf1c4 | **写已分配属性** [ch+0x256+i*2] + CHAR_ResetAttrFromStat(0xdf098) 重算衍生属性 + SV_MainCharacterSet |

> **StatDivide 加点链（✅ v0.4.5 逆向 + 真机验证，API stat 端点依据）**：`StatDivide_AddStat(0x148d14)` 属性+1/能力点-1（操作 UI 面板缓冲 0x307e20 区：+0x48 剩余点、+0x50 属性数组 5×8B）→ `StatDivide_OKApply(0x149000)` 提交缓冲到角色（CHAR_GetStatMain+缓冲求和→CHAR_SetStatMain→剩余点≠0 时 CHAR_SetStatusPoint→StatDivide_Init 重置）。**API 实现绕过 UI 缓冲**，直接读角色属性+校验能力点+属性+1+能力点-1（等价语义，无 UI 面板依赖）——真机验证力量 11→12/精力 15→16/能力点耗尽返回 no status point

> **AI 技能决策链（✅ v0.4.10 逆向 + 真机验证，API skill-usage 端点依据）**：`CHAR_ProcessAIOnCombat(0xe8b6c)` → `CHAR_ProcessNormalAIOnCombat(0xe497c)`：
> 1. `CHAR_GetSkillUsage(ch)` 读 [ch+0x3A0] bit0-2：0 → 仅普攻返回；非 0 → 进入技能选择
> 2. 遍历技能链表 [ch+0x2A0]（节点：+0x00 actionId u16、+0x07 **AI 等级**（0=不用该技能 AI）、+0x18 next 指针）
> 3. 每节点查技能表（0x24a000+0xcd8 按 actionId 索引）得技能档 w28 → 0 跳过
> 4. 节点 +0x07 == 0 → 跳过（该技能不参与 AI）
> 5. CHAR_GetActionState(ch, 节点) 非 0（冷却中）→ 跳过
> 6. 按技能档-1 跳转表选技能执行（0xe4aac 区 switch）
>
> **单技能 AI 等级 = 技能链表节点 +0x07**（UISkill_ButtonAIExe 0xd06ac 写 [action+0x7] = (level+1)%3 循环 0/1/2）；总开关 = [ch+0x3A0] bit0-2。API skill-usage 当前实现**总开关**（CHAR_SetSkillUsage），单技能 AI 等级写节点 +0x07 待需

> **属性重置链（✅ v0.4.7 逆向 + 真机验证，API stat-reset 端点依据）**：`CHAR_InitializeStatus(0xe68c8)` = 5 项分配属性归 0（CHAR_SetStatMain 循环）+ 能力点按 `(等级-1)×职业基础值` 还原（MEMORYTEXT_GetText_E+CAL_Calculate(0xd9968) 算基础值 → CHAR_SetStatusPoint）。游戏 UI 走 `CharacterInfo_ResetStatUIInAppProcess(0x149164)`（内购重置流程）。**API 直接调 CHAR_InitializeStatus = 免费重置**——用户确认归合法类别（v0.4.7）。真机验证：LV2 凯恩（分配点 0/能力点 3）重置后不变（无分配量可还，能力点 3=公式 (2-1)×3 还原值）

> **技能重置链（✅ v0.4.11 逆向 + 真机验证，API skill-reset 端点依据）**：`CHAR_InitializeSkill(0xe67c8)` = 遍历技能链表 [ch+0x2A0] 移除「非基础技能」（技能表 `0x2f6000+0x150` 字节×actionId → `0x2f4000+0x9e0` 技能信息表，查 byte bit1 置位=基础保留，否则 `ACTLIST_RemoveNode`(0xd79bc) 移除）+ 技能点按职业还原（`CHAR_SetSkillPoint`(0xd9c3c) 读 [ch+0xe] 职业等级）+ 主控同步 `SV_TSkillPointSet`(0x16caa0) + `PLAYER_RemoveShortcutType`(0x121764) 清快捷键 + `CHAR_ResetAttrUpdatedAll`(0xd9f0c) 重算。UI 层 `UISkill_ButtonSkillPointResetExe`(0xcece8) 含内购流程但底层函数独立可调——**与 stat-reset 同级合法（v0.4.11）**。真机验证：凯恩 actionId 80（非基础）被移除、skillPoints 还原 2、无崩溃

> ⚠️ 角色 +0x24 数组（32 个 int32）是**战斗属性**（id 0=暴击率×10、3=暴击伤害×10、4=攻击、8=魔攻、13=敏捷、17=防御、19=W.D.R×10、30/31=HP/MP 上限），**不是主属性**。主属性必须用 CHAR_GetStat。

### 2.3 背包（✅ 结构已破解，v0.2.14 实测）

**INVEN_pItem（0x7131c0）= 背包槽数组**（符号化 .bss 全局，base+VMA 安全直读）：

```
INVEN_pItem（768B）= 6 袋 × 0x80 步长
袋 i 槽数组基址 = INVEN_pItem + i×0x80
槽 j 物品指针 = 袋槽数组 + j×8（8B 指针，0 = 空槽）
每袋 16 槽 → 6×16 = 96 槽（真机实测 slotCount 总和 = 96 吻合）
物品结构：+0x08 类型位域(u16)、+0x10 数量位域(u32)
```

| 符号 | 地址 | 说明 |
|---|---|---|
| `INVEN_pItem` | 0x7131c0 | **背包槽数组基址**（6袋×0x80 每槽8B 物品指针） |
| `INVEN_GetBagSize(bag)` | 0x103250 | 袋内槽数（经 GOT 0x2f3bc0 → 袋表；袋结构+0x10 位域 bit24） |
| `INVEN_GetItemCount` | 0x104260 | 按类别统计（枚举槽数组的参考实现） |
| `INVEN_pBagSlot` | 0x7134c8 | 背包槽位（48 字节） |
| `INVEN_nBagSlotSelected` | 0x7134f8 | 当前选中槽 |
| 物品类别 | — | `UTIL_GetBitValue(item+0x08, 15, 6)` |
| 物品数量 | — | `UTIL_GetBitValue(item+0x10, 31, 25)` |
| 物品稀有度 | — | `ITEMSYSTEM_GetRarity(item)` @0x10d700 |

**物品名称联查（✅ v0.2.25 实测）**：`category = (typeFlags >> 6) & 0x3FF = ITEMDATABASE itemId`（frida 调用 ITEM_GetName 对比游戏真实名称验证）。
名称 = `ITEMDATABASE[category].text_0`（静态表，索引即 itemId；游戏名 = 词缀前缀 + 基础名，如"工匠的 皮革护手"）。

**背包移动（✅ v0.4.4 逆向 + 真机验证）**：`INVEN_MoveItem(item, count, targetBag, targetSlot)` @0x104934，语义：
- 源 item 指针 + 移动数量 + 目标 bag/slot（目标空槽 → `ITEMSYSTEM_CopyAsNewUID`(0x1083c8) 复制 + `INVEN_SaveItemDirect`(0x103bf0) 存入；目标同类 → 堆叠合并，上限 99）
- 源数量经 `UTIL_SetBitValue`(0x140564) 写回 +0x10（count 位域 31..25），**可拆分子堆叠**
- 返回 w0=1 成功 / 0 失败（`mov w1,#0x1` 成功标志，v0.4.4 frida hook 实测 retval=1）
- 前置限制：源/目标必须同 bag 类型（bag 表 +0x06 位 bit0 校验，0x104a48）；不同类别物品目标槽需为空（0x104a74 `cmp w22,w0` 类别比较）
- ⚠️ 空槽源 `cbz x0` 直接返回 0（不崩溃）；API 前置 `inventory_item_at` 判空返回 `slot empty`

**宝石镶嵌（✅ v0.4.6 逆向 + 真机验证）**：`ITEMSYSTEM_PutJewel(equipItem, jewelItem)` @0x10bcb4：
- 校验链：jewelItem 类别位（UTIL_GetBitValue +0x8,15,6）→ equipItem==null 返回 3 → 装备类别可镶嵌校验（0x2f5000+0xb60 表 bit0）→ `ITEMSYSTEM_IsJewel`(0x10b964) 宝石类别校验（非宝石返回 3）→ 装备 +0x19 bit4-6 插槽数（≤0 返回 2）→ `ITEM_AddOptionEx`(0x105ec4) 加属性 → 成功 +0x19 bit0-2 已镶数 +1
- 返回：0=成功 / 2=无孔 / 3=非宝石或空装备；**不消耗宝石物品本身**（API 手动 INVEN_RemoveItemDirect 删除防刷——真机验证宝石槽位清空）

**物品对象完整布局（✅ v0.4.62 frida 真机 dump 格斗之剑 + 反汇编交叉验证）**：

```
物品对象（连续排列，对象间隔 0x28；槽数组存 8B 指针指向对象首地址）：
+0x00  u64  物品链表 next（0x0489b35b 格式的相邻物品指针）
+0x08  u16  type 位域：bit2-5=稀有度位 bit6-15=类别（category=type>>6&0x3FF=ITEMDATABASE itemId）
+0x10  u32  count/混沌/宝石 位域（按物品类别复用）：bit0-7=混沌等级 bit8-15=混沌值率
           bit18-23=宝石属性id bit0-10=宝石数值（宝石类） bit25-31=数量（0=不可堆叠 100=装备 1-99=堆叠）
+0x18  u8   魔法倍率 magicRate（物理伤害 ×此值/100）
+0x19  u8   宝石位域 socket：bit0-3=已镶宝石数 bit4-7=总插槽数（✅ v0.4.6x 反汇编修正，4-bit）
+0x1A  u16  附魔/强化位域 enchant：bit0=混沌标志 bit2-5=强化状态标志 bit6-10=附魔等级 bit11-15=附魔ID
           （✅ v0.4.6x 反汇编修正：此前记 bit5-6/bit10-15，实测 bit6-10/bit11-15）
+0x20  ptr  词缀链表头（节点 0x18 字节，双向链表）
```

```
词缀节点（0x18 字节，双向链表；✅ v0.4.6x ITEM_AddOptionEx 0x105ec4 反汇编确认编码）：
+0x00  u16  编码：bit0-6=词缀索引（ITEMOPTINFOBASE 记录下标 0-36） bit13-15=type（0=词缀 1=宝石）
             ⚠️ 旧记录「u32 词缀 id 含分级位（0x91d80/0x40b01/0x61082）」为 frida 早期误读——实际 +0x00 是 u16，
             高 bit13-15 是 AddOption 类型参数，非分级位；词缀 id = 低 7 位（记录下标）
+0x02  s16  词缀值（API options 数组输出，真机 [9,4,6]；值 = 公式×seed系数 后随机 ∈[值/2,值]）
+0x04  u32  随机种子 seed（数值缩放系数：seed=1 不缩放，seed≠1 时值 ×(100-(seed-1)×10)%）
+0x08  ptr  前一节点
+0x10  ptr  下一节点
```

> **词缀值计算**（`ITEMSYSTEM_GetOptionValue` 0x109020）：基础值 = CAL_Calculate(ITEMOPTINFOBASE 记录 +4 text_id 公式, 能力等级)；最终 = MATH_GetRandom(基础值/2, 基础值)。**品质前缀体系（rarity 0-9）与词缀名全量表（text 1114-1150）见 docs/systems/inventory.md §2.4。**

**装备属性计算链（✅ v0.4.62 反汇编 ITEM_GetDamage 0x1099f0 / GetAbilityLevel 0x1091b4）**：
- `ITEM_GetDamage(item)`/`ITEM_GetDefense(item)` 不是直接读对象偏移，而是**静态表查值**：
  1. 读 item+0x08 type → `UTIL_GetBitValue(type, 15, 6)` 得类别
  2. 类别 × [0x2f5000+0x308](每类行数) + [0x2f4000+0xcf0](类别表基址) → `MEM_ReadUint8` 查基础攻击表
  3. item+0x1A（enchant）→ `UTIL_GetBitValue(10, 6)` 附魔ID → `ITEMSYSTEM_GetAbilityLevel(类别)` 查等级表（[0x2f5000+0x308]×类别+3 → MEM_ReadInt8）
  4. `ITEM_IsRealBroken`(0x105b78) 损坏检查（损坏则走 0x109230 分支）
- 结论：**装备基础攻击/防御/能力等级都来自静态表**（ITEMDATABASE 类别索引），物品对象只存 type/词缀/数量/附魔位域

| 函数 | VMA | 签名 | 说明 |
|---|---|---|---|
| `ITEM_GetDamage` | 0x1099f0 | int(item) | 攻击（查表+附魔等级） |
| `ITEM_GetDefense` | 0x109cc0 | int(item) | 防御（同模式） |
| `ITEM_GetAbilityLevel` | 0x1091f4 | int(item) | 能力等级（IsRealBroken 后查表） |
| `ITEMSYSTEM_GetAbilityLevel` | 0x1091b4 | int(category) | 类别→等级表查值 |
| `ITEM_IsRealBroken` | 0x105b78 | bool(item) | 损坏判定 |
| `ITEMSYSTEM_GetRarity` | 0x10d700 | int(item) | 稀有度（type bit2-5） |

### 2.4 地图 / 坐标（✅ 实时源已确认，2026-08-05 真机实测）

| 符号 | 地址 | 说明 |
|---|---|---|
| **当前地图 ID（真实）** | `*(*(base+0x2f4000+0xe80))` (u32) | **实时地图 ID** = MAPINFOBASE 记录下标（30=影子丛林1/31=影子丛林2 真机验证）；GOT 双层解引用，`G_CUR_MAP_ID_GOT_VMA`（v0.4.28 修正） |
| **~~MAP_nBaseInfo +0~~** | ~~0x713878~~ | ⚠️ **v0.4.28 前误用**：0x713878 实为瓦片矩阵起点（64×64，每字节 1 tile），前两字节 0x0808=2056 是巧合误读，**不是地图 ID**。SAVE_nMapID 0x729824 是存档字段（保存时才同步），亦非实时源 |
| **角色结构体 +0x02 / +0x04** | int16 ×2 | **实时玩家坐标**（CHAR_GetDistance 反汇编证实；MAP_nFocusX/Y、MAP_nFocusBX/BY 均非玩家实时位置） |
| `MAP_nFocusX/Y` | 0x713830 / 0x7168b4 | 存档/静态焦点（非实时，弃用） |
| `MAP_nFocusBX/BY` | 0x724d08 / 0x726e68 | 相机焦点（MAP_GetValidFocusX/Y 读取源，非玩家位置） |
| `MANISCENE_GetLayer` / `Scene_*` | — | 场景层/场景状态 |

### 2.5 队伍组成

| 符号 | 地址 | 说明 |
|---|---|---|
| `SAVE_nMainMercenarySlot` | 0x729826 | 主控角色槽（API 待加字段） |
| `SAVE_nPartyMercenarySlot` | 0x729828 | 队伍佣兵槽（3 字节 = 3 人） |
| `SAVE_nActiveMercenarySlot` / `SAVE_nHeroMercenarySlot` / `SAVE_nLiveMercenarySlot` | — | 活跃/英雄/存活槽 |
| `PARTY_AddMember` / `PARTY_Exclude` / `PARTY_Swap` | — | 队伍变更（hook 通知） |

**队伍操作边界（✅ v0.3.5-0.3.6 逆向，party include/exclude 前置校验）**：
- `CHAR_IsSpecialNPC(char)` @0xe4d90：`char +0x09(type)==2` 且查表 bit2==1 → 任务队友（如沃尔达克），**不可离队**
- `PARTY_Exclude(char)` @0x11f5c4：对**主控角色/特殊 NPC 走 `UIPopupMsg_CreateOKFromTextData` 弹窗路径**（text_id 未解析=乱码）→ API 必须前置校验；特殊路径返回 **-1**（非 0/1，truthy 陷阱）
- `MERCENARYSYSTEM_IncludeParty` @0x118e04：内部 `PARTY_GetSize<3` 校验 + PARTY_Include + 位置设置，返回 1/0（满员返回 0 → API 返回 `party full`）
- 佣兵槽 ID = 角色 **+0x352**（member[0]=0、member[1]=19、member[2]=1），API `mercenarySlot` 参数传此值（非 mercenaries 端点的大池索引）

**未上场佣兵槽（✅ 2026-08-05 探索逆向完成，v0.2.30-31 实现）**：
- 槽数组：`*(*(0x2f6010))` → **双层解引用**（0x2f6010 是 GOT 槽指向结构头，结构 +0 才是槽数组），**每槽 0x14 (20B)**
- 槽数上限：`*(*(0x2f3978))`（**双层解引用，=21** ✅ 2026-08-13 frida 实测；GOT 槽值 0xf65c4758 是指针地址，旧文档「=88」是 0x58 十六进制局部误读）
- 槽结构（MERCENARYSYSTEM_Set @0x118b94 反汇编确认）：+0x00 type(u8)、+0x01 u8、+0x02 u16、**+0x0B flags(u8: bit0=已占用 bit1=在队伍)**、+0x0C/+0x0E = 角色对象前 4 字节、+0x10 保留
- 角色↔槽关联：角色 +0x352（s8，佣兵槽索引，-1=非佣兵）；**`CHARSYSTEM_FindAsMercenarySlot(slot)` @0xf4254 按槽找角色**（遍历大池：池基址 *(*(0x2f3bb8))、步长 0x430、范围 0x1a2c0、条件 obj[0]!=0 && obj[0x352]==slot）——**未上场佣兵也必须用它**（自实现扫描找不到）
- 管理函数：`MERCENARYSYSTEM_Allocate`(0x118a50)、`MERCENARYSYSTEM_Set`(0x118b94)、`MERCENARYSYSTEM_AddCharacter`(0x118c10)、`MERCENARYSYSTEM_Release`(0x118ab4)
- 符号：`MERCENARYSYSTEM_pSlotList` @0x307750（直接指向槽数组）
- **坑**：刚进入世界时槽数据可能未初始化（垃圾 type=255/flags=255），等几秒重查

**佣兵遣散（✅ v0.4.8 逆向 + 真机验证，API discharge 端点依据）**：`MERCENARYSYSTEM_Release(mercenarySlot)` @0x118ab4：
- 语义：`CHARSYSTEM_FindAsMercenarySlot(slot)` 找角色 → `MERCENARYGROUPSKILLSYSTEM_Remove`(0x118900) 移除队伍技能 → 角色 +0x352 = -1 清关联 → UTIL_SetBitValue(角色+0x3cc) 清占用标志 → `CHAR_SetSituation`(ch,5)(0xdc310) → `MERCENARYSLOT_Initialize`(slot)(0x11896c) 重置槽 → `GAMESTATE_SetState`(0x151590) 刷新
- **⚠️ mercenary 端点 slot ≠ +0x352 槽 ID（v0.4.8 frida 实测）**：`/api/character/mercenary` 返回的 slot（如 27/32/58）是**槽数组索引**（MERCENARYSYSTEM_pSlotList 下标），而 MERCENARYSYSTEM_Release/include/exclude 的 mercenarySlot 参数是**角色 +0x352 槽 ID**（两套索引）。真机：存档 2 凯恩 +0x352=0、其余角色 +0x352=255（无效），无可用未上场佣兵 → discharge 对无效槽返回 `mercenary not found`（安全）。**mercenary 端点 slot 字段语义待修正（应暴露 +0x352 槽 ID 或统一索引）**

### 2.6 存档 / 其他

| 符号 | 地址 | 说明 |
|---|---|---|
| `SAVE_pSaveSlot` | 0x729858 | 存档槽结构（87 字节，含角色/物品数据，**离线兜底数据源**） |
| `SAVE_nVersion` / `SAVE_nBuildNumber` / `SAVE_bSaveFlag` | — | 存档版本/标志 |
| `QUESTSYSTEM_nActiveQuest` | 0x728ff8 | 当前任务 ID |
| `g_userUid` | 0x7106d0 | 用户 UID |

### 2.7 设置项数据（SYSTEMMENU 选项页，✅ 2026-08-09 静态逆向 + 真机动态验证）

> P0-1「SYSTEMMENU 选项页结构」产出。两个面板 popup：**SC_SYSTEMMENU**（世界内选项面板：保存/帮助/回主菜单）与 **SC_OPTION_MMENU**（主菜单环境设置：Sound/Light/Data Backup/Language）。

**APPINFO 设置结构体**（`APPINFOBASE_pData` = `*(0x2f5000+0xb18)`，字节字段）：

| 偏移 | 读写函数 | 字段 | 真机验证 |
|---|---|---|---|
| +0x00 | `APPINFO_Set/GetVolume` @0xd84e8/0xd84f0 | 音量（Sound 开=6 关=0） | ✅ 0↔6（Sound 按钮切换） |
| +0x01 | `APPINFO_Set/GetVibration` @0xd8560/0xd8550 | 震动 | — |
| +0x02 | `APPINFO_Set/GetSound` @0xd8538/0xd8528 | 声音开关 | 采样=2 |
| +0x03 | `APPINFO_Set/GetAutoSave` @0xd8510/0xd8500 | 自动保存 | 采样=1 |
| +0x04 | `APPINFO_Set/GetGraphicValue` @0xd8590/0xd8578 | 画质位掩码（Light Effect=bit1） | ✅ 5↔7（Light 开关） |

- `APPINFO_SetGraphicValue(x, v)`：v=1 置位 bit_x，v=0 清位 bit_x（掩码语义）
- `UIOption_ButtonListExe` @0xc44d8 按钮映射（SC_OPTION_MMENU 面板）：child 0/1 = 音量 6/0、child 2/3 = 画质 bit2 置/清、child 4 = `SAVE_MergeDatInit`+`SAVE_Merge`+`C2SHubSaveDataCheckExistFromServer`（Data Backup 保存，**Hive 云存档链，非 SAVE_Save**）、child 5 = 云存档检查、child 6/7 = 语言切换

**语言变量**（两个，勿混淆）：
- UI 语言索引 `*(0x2f9000+0xf34)`（u32）：UIOption_ButtonListExe 切换目标，0-4 循环（0=简体中文，右箭头 +1 取模 5）——✅ 真机 0→1→2→3→4→0 循环验证
- SGL 语言 ID `*(0x2f5000+0x28)` 指针指向 u32：`SGL_SetLanguage` @0x944f8 写入（渲染层），切换语言时 UI 索引先行、SGL ID 延迟/条件更新（实测切换期间保持 2）

**SYSTEMMENU 保存链**（✅ 动态 hook 验证，2026-08-09）：
- `SystemMenu_ButtonSaveExe` @0x14f7c4 → `SOUNDSYSTEM_Play` + **`SAVE_ProcessSave`(0x129830)** → `SAVE_IsOK`(0x128c14) + `KEY_ResetActive` + **`SAVE_Save`(0x129600)** → 成功弹 `UIPopupMsg_CreateOKFromTextData`("保存成功")
- 按钮回调：`SystemMenu_ButtonHelpExe` @0x14fec0（→`UIHelp_Enter`）、`SystemMenu_ButtonBackExe` @0x14fd18（关闭面板）、`SystemMenu_ButtonExitExe` @0x14f7e8（回主菜单确认弹窗 `UIPopupMsg_CreateYesNoFromTextData`）
- **推翻旧结论**：sysmenu-exploration.md §3「SAVE hook 均未命中/attach hook 不生效」是 **frida JS 数字键陷阱**（`{0xca778:...}` 键转十进制字符串，`Object.keys`+`parseInt(,16)` 地址错位），hook 从未命中正确地址；修复后 hook 完全命中

**存档槽 UI（SC_SAVESLOT @0x14c720）**：主菜单「开始游戏」→ 3 槽位面板（槽 1 有存档：LV27 忍者/悲鸣要塞1层），返回箭头 (95,80)、槽位 1/2/3 ≈ (1584,320)/(1584,685)/(1584,1040)（实机存在两种缩放，以截图 pixel-locate 为准）

## 3. 技术方案：native 数据访问

### 3.1 读取路径（模块 native 层，C/C++，✅ 真机验证）

**不用 dlopen/dlsym**（Android linker namespace 隔离会加载 libgame.so 独立副本 → 读不到游戏数据，实测全 0）。
正确方式：`/proc/self/maps` 取 libgame.so 加载基址（ELF 首 LOAD vaddr=0），符号运行时地址 = 基址 + 符号偏移。

**v0.5.15 起符号偏移动态解析**（换版本零操作）：
- 204 个有符号名符号（symbol_registry.h 单一来源）：解析器读进程内 libgame.so 的 `.dynsym`（SysV hash）按**符号名**查地址，VMA 仅兜底
- 42 个无符号名 GOT 槽（`*_GOT_VMA`）：从 `.rela.dyn` R_AARCH64_RELATIVE 按 `r_offset` 反查 `addend`（=槽指向的数据地址）
- 实现：`symbol_resolver.h/.cpp` + `bridge_init` 内 eager 批量解析；来源统计见 `g_symbol_report`
- 8 版本（20260704~0810）验证：204 符号 0 漂移、42 GOT 槽 42/42 命中
- ⚠️ **v0.5.16 真机修复**：PT_DYNAMIC 定位必须用 `load_bias + p_vaddr`，不能用 `base + p_offset`。libgame.so 第二 LOAD 段 `p_offset(0x2dea00) != p_vaddr(0x2eea00)`（差 0x10000），文件 buffer 内二者一致故离线测试全过、内存映射下 `base+p_offset` 偏离 64KB 指向野地址 → attach() 读 `dyn->d_tag` SIGSEGV（v0.5.15 未真机验证即埋下此坑）。d_ptr 本身是 vaddr，后续 DT_SYMTAB/STRTAB/HASH/RELA 定位本就该用 `load_bias + d_ptr`（代码已是），仅 dynamic 段数组起始地址错用了 p_offset。

```cpp
uintptr_t base = /* /proc/self/maps 第一个 libgame.so 映射 start */;
int64_t money = *(int64_t*)(base + 0x7134c0);          // 直读全局变量
uint32_t mapId = *(uint32_t*)(*(uintptr_t*)(base + 0x2f4000 + 0xe80));  // 实时地图 ID = MAPINFOBASE 记录下标（GOT 双层解引用，v0.4.28）
auto getMember = (void*(*)(int))(base + 0x11f384);     // 调用 Getter
void* char0 = getMember(0);
int16_t x = *(int16_t*)((uint8_t*)char0 + 0x02);       // 实时坐标
```
代码分层与常量管理详见 `architecture.md`（唯一权威）；本文件仅记录逆向数据源细节。

### 3.2 结构体逆向（✅ M4.1 完成：反汇编 Getter 函数确定偏移）

角色结构体（`PARTY_GetMember(i)` / `PARTY_pChar[0x728ec0]` 指向）：

| 偏移 | 类型 | 含义 | 来源函数 |
|---|---|---|---|
| +0x00 | int8 | **situation 情形码**（CHAR_SetSituation 0xdc310 写；**引擎碰撞 CHARSYSTEM_GetCharacterBlock 0xddaac 要求==1 才判阻挡**；尸体死亡→SetSituation(6)/Free(0)，situation!=1 不再阻挡） | CHAR_SetSituation / GetCharacterBlock |
| +0x09 | int8 | 角色类型（0=英雄 1=佣兵） | CHAR_GetName 分支 |
| +0x0A | u16 | 名称 text_id（API 用静态文本 JSON 联查） | CHAR_GetName |
| +0x0E | int8 | 等级 | CHAR_SetLevel（`ldrb/strb`） |
| +0x24 | int32×N | 属性数组（`[char + attr_id*4 + 0x24]`） | CHAR_GetAttr |
| +0x1F0 | int32 | 当前 HP | CHAR_AddLife |
| +0x1F4 | int32 | 当前 MP | CHAR_AddMana |
| +0x1F8 | ptr×10 | 装备槽数组（10 槽 × 8B） | CHAR_GetEquipItem |
| +0x278 | ptr | 移动目标指针（MoveAsPath 前置条件，null 则返回 0） | CHAR_MoveAsPath 反汇编 |
| +0x2E2 | int8 | **控制状态：0=AI 单位 / 7=玩家控制 / 135=战斗**（≠0 时 MoveAsPath 要求 +0x278 非空；AI 场景单位=0 可自由 MoveAsPath） | CHAR_MoveAsPath 反汇编 |
| +0x2F0 | ptr | PATHLIST 寻路结果链表（节点 +0x00 u16 网格x、+0x02 u16 网格y、+0x08 next） | CHAR_SearchPath |
| +0x318 | int64 | 当前经验 | CHAR_GetExperience |
| +0x320 | int64 | 升级所需经验 | CHAR_GetNextExperience |

HP 上限 = `CHAR_GetAttr(char, 0x1e)`，MP 上限 = `CHAR_GetAttr(char, 0x1f)`。

**碰撞阻挡判定（✅ v0.5.28 逆向，CHARSYSTEM_GetCharacterBlock 0xddaac）**：
- 遍历 CHARSYSTEM 池（0x1a2c0=100 对象，步长 0x430），过滤：situation(obj[0])==1、obj==目标时跳过自己
- type==2（装饰物）：读装饰物属性表（obj[0x0a] 索引，表基址 *(0x2f4058)、步长 *(0x2f5a48)）bit7——bit7==1 不阻挡、bit7==0 走矩形碰撞
- type!=2（角色/怪）：CHAR_GetAreaRect(0xdd584) + UTIL_GetIntersectionArea(0x140670) 矩形重叠判定
- **结论**：引擎用 situation 判断（非 C_STATUS 0x311、非 hp）；模块 nav_unit_blocks 此前用 type/status 过滤导致尸体（situation!=1）误判阻挡，v0.5.28 已加 situation==1 过滤对齐

**出口切图判定（✅ v0.5.28 逆向，修复出口坐标不切图）**：
- `GAMEPLAY_GoMapLinkByChar` 0x9cdc0 是 **4 参数** `(ch, tile_x, tile_y, use_dir)`，非 3 参数（use_dir 透传给 CheckMapLink）
- `GAMEPLAY_CheckMapLink` 0x9cc28：读 `matrix[ty*64+tx]` 的 **bit7**（与 API 出口扫描同源，非「两套数据源不一致」）——bit7==0 无 link；use_dir==0 → MAP_FindMapLinkNoDir（只查 x,y）；use_dir!=0 → MAP_FindMapLink（查 x,y+角色朝向 obj[6]）
- `MAP_FindMapLink` 0x112aac：exit 条目（6B：x u8, y u8, b2, b3, u16 LE）u16 bit13-15=方向，匹配 x,y + 方向（方向==4 通配）；`MAP_FindMapLinkNoDir` 0x112b2c 只匹配 x,y
- **根因**：模块 map_link_check 此前 3 参数调用，use_dir 为垃圾值 → 走 MAP_FindMapLink 按朝向匹配 → 朝向不符返回 null → 出口概率性不切图；v0.5.28 改 4 参数 use_dir=0 走 NoDir（不依赖朝向）
- 引擎主循环（GAMESTATE_PressKeyPlay 0x9d1a4）传 use_dir=1 且先 CHAR_SetDirection 设朝向（官方切图要求朝向正确）

**装备操作函数（✅ v0.3.3 逆向，equip 自动替换依据）**：
- `CHAR_CanEquipItem(char, item)` @0xe4eb4：职业掩码/等级校验（先调 `ITEM_IsRealEquip` 读 ITEMDATABASE +2 字节 bit0 判真装备）
- `CHAR_FindEquipSlot(char, item)` @0xe4fd0：计算目标装备槽
- `CHAR_GetEquipItem(char, slot)` @0xda20c：取指定槽装备
- `CHAR_EquipItem(char, item)` @0xe51c0：**目标槽已被占用时返回 0**（e5294: cbnz x0→ret 0）→ 需先卸再穿
- `CHAR_UnequipItemToInven(char, slot)` @0xe2f68：脱下装备槽→背包，返回 1/0

物品结构体（装备/背包物品指针指向，✅ 2026-08-05 探索逆向完成）：

| 偏移 | 类型 | 含义 | 来源函数 |
|---|---|---|---|
| +0x08 | u16 | 类型位域（bit2-5=稀有度位、bit6-15=类别） | ITEMSYSTEM_GetRarity/ITEM_GetDamage |
| +0x10 | u32 | 数量/混沌/宝石位域（bit0-7=混沌等级、bit8-15=混沌值率、bit18-23=宝石属性id、bit0-10=宝石数值、bit25-31=数量） | ITEM_GetChaosLevel/ITEM_GetCumulateCount/PutJewel |
| +0x18 | u8 | 魔法伤害倍率（物理伤害 × 此值/100） | ITEM_GetMagicDamage(0x109c80) |
| +0x19 | u8 | 宝石/插槽位域（**bit0-3=已镶宝石数、bit4-7=总插槽数**，4-bit） | ITEMSYSTEM_PutJewel(0x10bcb4)/ApplySocket(0x10d8a4) |
| +0x1a | u16 | 附魔/强化位域（bit0=混沌标志、bit2-5=强化状态标志、**bit6-10=附魔等级、bit11-15=附魔ID**） | ITEMSYSTEM_EnchantItem(0x10b330)/ApplyEnchantValue(0x109890) |
| +0x20 | ptr | 选项链表头（词缀：节点+0x00 低7位=索引、+0x02=s16值、+0x08=下一节点） | ITEM_AddOptionEx(0x105ec4)/GetOptionValue |

> ✅ 2026-08-12 反汇编修正：socket/enchant/混沌位域宽度此前误记 3-bit（bit0-2/bit4-6、bit5-6、bit0-6/bit8-22），实测均为 4-bit/5-bit 位域，见 `docs/systems/inventory.md` §2.4（权威）。

物品实例属性计算函数（可调用）：`ITEM_GetDamage`(0x1099f0 攻击)、`ITEM_GetDefense`(0x109cc0 防御)、`ITEM_GetMagicDamage`(0x109c80)、`ITEMSYSTEM_GetEquipLevel`(0x10976c 装备等级)。
物品静态表 `ITEMSTATICBASE`(0x301a10)：+0x03 装备等级、+0x04 基础稀有度、+0x05 需求等级、+0x06 附魔基础值、+0x07 功能位域(bit3=饰品/bit4=可插槽)、+0x08 图标帧。
附魔表 `ITEMENCHANTBASE`(0x301a20)：+0x00 卷轴ID、+0x02 类型掩码、+0x06 基础值、+0x07 上限、+0x08 溢出系数（经物品+0x1a 附魔ID 索引）。

物品身份由类型位域编码 → 类别联查 ITEMCLASSBASE（静态表）。稀有度 = `ITEMSYSTEM_GetRarity(item)`。

**物品可使用性判定（✅ v0.3.2 逆向，use-item 前置校验）**：
- `ITEMDATABASE_IsUse(itemId)` @0x1058ac：itemId = 类别 = `UTIL_GetBitValue(typeFlags, 15, 6)`；内部先特判 itemId∈{0x1a,0x1b} 返回 1，否则读 ITEMDATABASE 记录 +2 字节，值 ∈{0x16,0x17}(22/23)=恢复药水类才可消耗
- `ITEMDATA_IsUse` @0x10583c：内部同样读表记录+2 字节 `-0x16 ≤1` 判定
- ⚠️ `INVEN_ConsumeItem`(0x1047bc) **无物品类型校验**——对装备调用会触发游戏内部非预期 UI 流程（乱码弹窗）

背包结构（✅ v0.2.14 破解，见 §2.3）：`INVEN_pItem`(0x7131c0) = 6 袋 × 0x80 步长槽数组，每槽 8B 物品指针，每袋 16 槽；
袋内槽数 = `INVEN_GetBagSize(bag)`（经 GOT 0x2f3bc0 → 袋表 → 袋结构+0x10 位域）；
物品类别 = `UTIL_GetBitValue(item[+8], 15, 6)`、数量 = `UTIL_GetBitValue(item[+0x10], 31, 25)`。
> ⚠️ 旧记录"袋表 @0x2f31c8"有误：0x2f31c8 是 GOT 槽（指向 INVEN_pItem 0x7131c0）；该错误曾导致 SIGSEGV，勿再使用。

**物品删除函数（✅ v0.3.2 逆向，discard/sell 实现依据）**：
- `INVEN_RemoveItemDirect(bag, slot)` @0x103fd8：按槽删物品（x0=bag 左移 4 位 + slot → bag*16+slot 索引，ITEMPOOL_Free 释放）。⚠️ **返回值非成功标志**——成功路径 tail-call `PLAYER_UpdateShortcut`(0x120e40)，最终返回值为 UpdateShortcut 的返回值。**判定成功须调用后检查槽位是否清空**（`inventory_item_at(bag, slot) == nullptr`）
- `INVEN_RemoveItem(item)` @0x104044：**按 item 指针删**（内部 INVEN_FindItemSlot(0x103704) 按指针找槽 + RemoveItemDirect 删除，返回 1/0）。⚠️ **v0.4.3 语义修正**：此前误记为"按类别删"（反汇编证实 x0 是 item 指针，FindItemSlot 逐槽 `cmp x2,x20` 指针比较）；按类别删需先遍历背包匹配 category 再调此函数
- `INVEN_MoveItem(item, ...)` @0x104934：4 参（item+3），复杂，v0.3 暂缓

单位结构体（CHARLOCSYSTEM 池，玩家/敌人/NPC 通用，✅ 2026-08-05 探索逆向完成）：

| 偏移 | 类型 | 含义 | 来源函数 |
|---|---|---|---|
| +0x02 | int16 | 实时 X | CHAR_GetDistance |
| +0x04 | int16 | 实时 Y | CHAR_GetDistance |
| +0x09 | u8 | 角色类型（0=英雄 1=佣兵 ≥2=怪物/召唤物，无装备系统） | CHAR_GetEquipItem 校验 cmp #2 |
| +0x0A | u16 | 子类型/召唤类型（0x30/0x31=召唤怪物） | CHAR_GetAttrFromSummonMonster(0xdff00) |
| +0x0E | s8 | **等级**（✅ v0.3.14 实机：敌人 lv=1、队伍 lv27/26） | 结构体直读 |
| +0x1F0 | int32 | **当前 HP**（✅ v0.3.14 实机：敌人 hp=792、队伍 hp=10504/8352） | 结构体直读 |
| +0x1F4 | int32 | **当前 MP**（✅ v0.3.14 实机：mp=200） | 结构体直读 |
| — | — | **名称 CHAR_GetName(0xd9c54)**（✅ v0.3.14 实机：队伍"凯恩/沃尔达克/西雷斯"、地图物件"地图出口/火把/神灯"、怪物也可能返回 nullptr 须判空） | CHAR_GetName |
| +0x2C8 | u32 | 位7=非召唤物标记 | CHAR_GetSummoner(0xdb730) |
| +0x2D0 | ptr | 召唤数据链表（节点+0x00 类型码==0x07→+0x08=召唤者指针、+0x10=下一节点） | CHAR_GetSummoner |
| +0x311 | u8 | 角色状态码（0=英雄 1=城镇NPC/佣兵 2=怪物/召唤物） | CHARLOCSYSTEM_Load(0xf3084) |
| +0x312 | s8 | 显示类型 | CHAR_GetDisplayType(0xdcfd0) |
| +0x314 | s16 | 显示信息 | CHAR_GetDisplayInfo(0xde964) |
| +0x3CE | s8 | 区域类型 | CHAR_GetAreaType(0xdc0a8) |

CHARLOCSYSTEM 定位池（10B/槽，池@0x307530 计数@0x307528）：+0x00 地图ID、+0x02 X(半格)、+0x04 Y、+0x06/+0x07 像素偏移(*16)、+0x08 动作/状态ID。
**实机验证（v0.3.12，2026-08-08）**：charLoc 是**地图初始单位登记**（非实时坐标）——CHARLOCSYSTEM_Add(0xf2fbc) 调用者 0x1285d8 从数据文件 `MEM_ReadUint16/Uint8` 读取（刷怪点/初始摆放），坐标用地图数据格式（units 端点输出）；**实时坐标请用 units（CHARSYSTEM 池 +0x02/+0x04）**。

**召唤物判定**：`CHAR_GetSummoner(unit)` 返回非 NULL（+0x2C8 位7=0 且 +0x2D0 链表含类型码 0x07 节点）→ 是召唤物；或 `+0x0A==0x30/0x31`（召唤怪物 class）。

UI 状态变量（✅ v0.2.22 实测）：

| 符号 | 地址 | 说明 |
|---|---|---|
| `STATE_nState` | 0x307492 (u16) | **UI 状态机：4=主菜单流程（登录弹窗/存档选择）5=游戏中** |
| `GAMESTATE_nState` | 0x72b068 (u32) | 游戏状态 |
| `INITSTATE_nState` | 0x72b06d (u8) | 初始化状态 |

寻路（✅ v0.2.33-34 逆向）：

| 函数 | 地址 | 说明 |
|---|---|---|
| `CHAR_SearchPath(char, tx, ty, flag)` | 0xdb094 | flag=1 走 A*（构造 ASTAR → ASTAR_GeneratePath(0xd93e4) → 结果存角色 +0x2F0）；flag=0 仅 MAP_IsBlockingByPixel(0x113bcc) 阻塞检查 |

**瓦片通行矩阵（P0#3 逆向 + 实机验证，2026-08-08）**：
- 寻路链：CHAR_SearchPath(0xdb094) → MAP_IsBlockingByPixel(0x113bcc) → MAP_IsBlocking(0x113b6c)
- **通行矩阵 = GOT `*(*(base+0x2f3f48))`**（MAP_IsBlocking 反汇编 + frida 实测），索引 `y*64 + x`（1B/瓦片），**bit3=阻挡标志**
- ⚠️ **MAP_nBaseTile(0x7148a8) 与通行矩阵非同一数据**（frida 实测相差 0x1030）——0x7148a8 是**渲染基础瓦片**，寻路/阻挡用 GOT 矩阵
- 瓦片大小 16 像素（像素 ÷16 = 瓦片，MAP_IsBlockingByPixel asr #4）
- **静态性验证（v0.4.62，2026-08-12）**：同图多次读取 + 重进档 hash 恒定（0x53d32b88，mapId=31 阻挡=562/出口=9）——瓦片矩阵是**纯静态数据**（同图永久不变，P0 入静态数据的前提成立）
- **完整导出端点（v0.4.62）**：`GET /api/world/map/tiles` → `{mapId,size:64,encoding:"base64",tiles}`（4096B → 5464 字符 base64，解码与 frida 直读内存一致）——供 P0 采集工具一次性拿整图；**v0.5.24 起端点返回双层数组**（`encoding:"array"`，64×64 已解码瓦片值，bit6=阻挡 bit7=出口）

### 瓦片矩阵构建逆向（✅ 2026-08-12，P0 研究产出）

> 状态：✅ 离线 + frida 双路径均已验证，m31 matrix 一致率 99.73%（4096 字节中 11 字节 diff，全是 exit 标志位）

**MAP_Load 函数（0x1149d4）文件解析流程**（反汇编 + 真机 frida 验证）：

```
MAP_Load(mapId, flag):
  ├─ RES_LoadToPool(filename) → 文件数据指针（已过 LZMA 容器解压）
  ├─ 检查 byte 0：
  │   ├─ ==1 → 调 LZMA_Decode（内层 LZMA 解压），jump 到 0x114ad0 继续处理
  │   └─ !=1 → x0 += 1（skip 1 字节）
  └─ 读 4 字节 header：
      ├─ byte 1, 2：v0.4.62 frida 实证 width/height 在 byte 2/3（不是 disassembly 字面解读的 byte 3/4）
      ├─ byte 2 → *(0x2f4e60) = width（u32 存储）
      └─ byte 3 → *(0x2f60d0) = height（u32 存储）
  ├─ MAP_LoadBase(0, 0, width, height, *(*(0x2f60d0)))  // 遍历 base layer
  ├─ MAP_LoadLayer(0, 0, base_ptr, 0)  // 读 exit count + layer configs + sections
  ├─ MAP_LoadTile(width)  // 加载 tile graphics
  └─ MAPFEATURESYSTEM_Load + MAP_SetInformation + ...
```

**MAP_LoadBase 内层循环（0x11216c-0x112168）核心逻辑**：

```
对每个 cell (x, y), x ∈ [0, width), y ∈ [0, height):
  读 2 字节 byte1, byte2
  tile_id = (byte1 & 0x7) << 8 | byte2   // 11-bit tile ID 编码
  matrix_byte = byte1 >> 4                // 高 4 位作为 matrix 字节
  if tile_id in BLOCKING_LIST: matrix_byte |= 0x40
  matrix[y * 64 + x] = matrix_byte
```

**阻挡 tile ID 完整列表**（从 0x11210c-0x11225c cmp 指令提取）：

| 类别 | 值 |
|---|---|
| 精确值 | 0xa1, 0xa8, 0xaf, 0xb2, 0x259, 0x264, 0x267, 0x273, 0x276, 0x6f6, 0x758 |
| 范围 | 0x8d-0x8f, 0x95, 0x99-0x9e, 0xaa-0xad, 0xb5-0xb6, 0xb8-0xbb, 0x23d, 0x24a-0x24b |

**离线 vs frida 验证结果**（m31，2026-08-12 真机 192.168.3.54）：

| 指标 | offline | frida runtime | 一致？ |
|---|---|---|---|
| width × height | 40 × 30 | 40 × 30 | ✅ |
| bit3 置位 (562) | 562 | 562 | ✅ |
| bit6 置位 (20) | 20 | 20 | ✅ |
| bit7 置位 (9) | 9 | 9 | ✅ |
| nonzero (587/589) | 587 | 589 | ⚠️ 6 cells bit 5 差异 |

**Exit data 已完整逆向并解析**（2026-08-12 第二轮）：

- **关键修复**：MAP_LoadLayer 读 **5 个 u16**（1 Create + 4 AddLayer），非 4 个——修正后 exit count 定位正确
- **文件结构完整**：`byte2=width, byte3=height` → base layer（width*height*2 bytes）→ MAP_LoadLayer（5 u16 + 1 u8 section count + sections[1 u8 hdr + 1 u16 cnt + cnt*4 features]）→ exit count（1 u8）→ exits（6 bytes each）
- **exit 条目格式**（6 字节）：`x(u8) y(u8) b2(u8) b3(u8) u16(LE)`，其中 u16 bit 13-15 = 出口方向（MAP_FindMapLink 0x112b04 反汇编）
- **matrix 写入**：`matrix[y*64 + x] |= 0x80`（MAP_Load 0x114c1c `orr w3, w3, #0xffffff80`）
- **m31 验证**：9 个 exit（(0,0)(1,0)(2,0)(0,1)(0,2)(36,25)(37,25)(36,26)(37,26)）与 runtime 及 API `/api/world/map` exits 字段完全一致

**剩余 bit 5 差异**（6 cells = 0.15%，m31）：

```
[(y=25,x=36) (y=25,x=37) (y=26,x=36) (y=26,x=37)]  off=0x80 run=0xa0 (bit5+bit7)
[(y=27,x=36) (y=27,x=37)]                          off=0x00 run=0x20 (bit5 only)
```

→ bit 5 是 **map link area 区域标记**（出口周围的 2 列×3 行区域），由 MAP_SetEventAreaOn (0x112ca4，读 matrix 0x2f3f48) 或类似函数写入。对通行/寻路无影响（MAP_IsBlocking 只查 bit 3），静态数据可省略或后续再逆向。

**全量统计**（416 图）：blocking max=293 avg=11.6，exits(bit7) max=36 avg=7.4 total=3077，387 图有 exit，29 图无 exit。

**地图文件尺寸分布**（扫描 416 个 m*.dat.bin，width=byte2, height=byte3）：

- Width 范围 0-57（avg 30.5），最常见 width=25（48 图）、40（36 图）、30（33 图）
- Height 范围 0-198（avg 123.9），最常见 height=134（166 图）、128（128 图）、130（37 图）
- **无 height=0 图**（之前误读 byte4=0，实际 byte3=height）
- **206 图文件不够装 64 行** base layer（width*64*2 字节 > 文件 size，但 base layer 实际只填 width*min(height, 64) cells）
- 总存储：64×64×416 = 1.66 MB（base64 后 2.2 MB），与 backlog 估算 1.7MB 一致

**两条提取路径对比**：

| 路径 | 工作量 | 准确性 | 状态 |
|---|---|---|---|
| **A. 离线解析 base layer** | ✅ 完成（`scripts/parse/export_map_tiles.py`，2.2MB JSON） | base layer 100% 正确，缺 exit 标志 | 已 commit |
| **B. frida 全量 dump 416 图** | m31 样本已验证（11 bytes exit-only diff） | runtime matrix 完全一致 | 待用户扩展到 416 图（游戏需在 world 状态，tutorial_pause 下 CHANGEMAP 失败） |

**推荐**：
1. ✅ 离线 base layer 已完成并 commit，可直接用于阻挡 API（无 exit 也可工作）
2. ⏳ 后续若需完整 matrix（含 exit），需在 game state=world 时跑 frida 遍历 416 图；或扩展离线 parser 处理 exit data
3. 接入 native 层：v0.4.61 当前 interval=0 惰性，从静态数据读取后改从 JSON 加载

| 路径结果 | 角色 +0x2F0 | PATHLIST 链表：节点 +0x00 u16 网格x、+0x02 u16 网格y、+0x08 next；**网格×8=像素坐标**；链表=起点→终点 |

| 路径结果 | 角色 +0x2F0 | PATHLIST 链表：节点 +0x00 u16 网格x、+0x02 u16 网格y、+0x08 next；**网格×8=像素坐标**；链表=起点→终点 |

> **副作用**：CHAR_SearchPath 仅计算存储路径，**不触发角色移动**（多轮探测位置不变）。
> ⚠️ frida 直接调 ASTAR_GeneratePath 会崩溃（A* 不收敛），弃用；ASTAR 内部：+0x28=碰撞回调=*(0x2f5450)、+0x30=*(0x2f3c80)、+0x38=地图宽*2+1（地图=*(0x2f4e60)）、+0x3c=w5、+0x40=w6；参数(x0=astar, sx>>3, sy>>3, ex>>3, ey>>3, 1, 1)。

**移动执行（✅ v0.3.2 逆向，move 实现依据）**：

| 函数 | 地址 | 说明 |
|---|---|---|
| `CHAR_MoveAsPath(char)` | 0xe9db8 | 沿 +0x2F0 PATHLIST 移动。**玩家控制态（+0x2e2≠0）下要求 +0x278 目标指针非空否则返回 0**；且**只走一步不续走**（游戏主循环不自动跟进），需外部循环调用。AI 单位（+0x2e2=0）可自由调用 |
| `CHAR_Move(char, mode, delta, flag)` | 0xe9808 | 方向键移动：**mode 0-3 = 下/左/上/右**（✅ v0.4.1 真机实测：0=y+、1=x-、2=y-、3=x+），delta=8 像素/帧（**值传递**，非指针；v0.4.1 曾因 typedef 误为 int* 传 &delta 导致不动）；玩家按住方向键时游戏主循环每帧调用。mode>3 直接调用无效 |

> **玩家真实移动机制** = 方向键长按 → 主循环每帧 `CHAR_Move`。API move 实现 = `CHAR_SearchPath` 计算路径 + **临时清零 +0x2e2 控制态 + 循环调用 `CHAR_MoveAsPath` 走完 PATHLIST（上限 512 步）+ 还原控制态**（仍走游戏合法寻路链路，非 OP 传送）。

> ⚠️ **v0.4.29 更新**：API move 已改用**自研 BFS 导航**（瓦片矩阵 bit3 + 单位占用）→ FrameTaskManager 逐帧驱动 CHAR_Move，不再使用 CHAR_SearchPath + 手动循环 MoveAsPath（旧实现仅 v0.4.29 前）。

### 3.3 事件通知（可选增强）hook 变更函数（inline hook：ShadowHook/xHook 或 frida-gum 静态）：
- `INVEN_AddMoney` / `INVEN_SetMoney` → 金币变化
- `CHAR_AddExperience` / `CHAR_SetLevel` → 升级
- `PARTY_AddHPMP` / `CHAR_AddLife` / `CHAR_AddMana` → 血量/魔法变化
- `PARTY_AddMember` / `PARTY_Exclude` → 队伍变化
- `INVEN_MoveItem` / `INVEN_RemoveItem` → 背包变化

### 3.4 风险

- 结构体偏移因游戏版本而异（本次以盗版大修 20260704 为准）
- `MAP_nFocusX/Y` 是焦点而非精确玩家坐标（已确认弃用，用角色 +0x02/+0x04）
- 模块 native 层加载于游戏进程内，理论上可用 dlopen/dlsym，但 **namespace 隔离会加载独立副本读不到游戏数据**（实测全 0）——v0.5.15 起用基址 + ELF `.dynsym` 符号名解析（204 符号）+ RELATIVE 反查 GOT 槽（42 槽），VMA 仅兜底（见 §3.1）
- **popup 栈操作崩溃风险（v0.4.5 实测）**：`POPUPSTATE_Pop`(0x122600) 关闭面板在 settings 场景崩溃（pop 后新栈顶 resume 回调 → `STATE_ResumeGame → GAMESTATE_DrawPlay → MAP_DrawLayer` SIGSEGV）。popup 栈（g_arrPopupStack 0x728fd8）状态机对 pop 顺序敏感，**绕过 UI 触摸直接调 Pop 不安全**——面板关闭类端点需走游戏 Back 键事件路径或放弃（已记录 control-capability 不可调用表）
- **技能动作释放崩溃风险（两次实测）**：`CHAR_SetActionID`(0xe79ec) → `CHAR_SetAction`(0xe7630) 释放技能动作：v0.4.5 崩溃 `GAMEPLAY_DrawFocus`(0x9d3ec) 读目标 +0x4（传交互物目标，GLThread）；v0.4.11 改用 `CHAR_GetEnemyTarget`(0xe42b4) 自动取目标仍崩——`CHAR_SetAction+896` 内部 SIGSEGV（fault 0x3，HTTP 线程）。**CHAR_SetAction 对动作上下文敏感（战斗状态/动画资源），与目标判定无关**——cast 需完整逆向 CHAR_SetAction 前置条件（战斗状态机）后才能安全实现

### 3.5 游戏主循环帧率（P0-2，2026-08-08 frida 实测；2026-08-12 v0.4.57 帧同步复核）

- **`MainProcess`(0xd4984) 恒定 ~16.9fps**，不随界面/战斗状态变化（主菜单 16.9fps = 世界活跃 16.9fps，两轮 30s 采样一致）；2026-08-12 实测 20.1-20.2fps（tutorial_pause 状态）
- 主循环**无条件逐帧调用**，无休眠退避；backlog 早期记录 17.4fps 为测量窗口差异
- **含义**：events 轮询采样间隔**不受游戏状态影响**，可固定 500ms-1s（每帧 ~59ms，采样间隔远超单帧时间，不会漏事件）
- frida 探测脚本：`scripts/analyze/run_probe.py` + `/tmp/opencode/fps_probe.js`（Interceptor.attach MainProcess + 计时统计）

**帧计数 `G_FRAME_COUNT_VMA`（0x2f5648）时序实测（2026-08-12，v0.4.57 帧同步依据）**：

- **帧计数与主循环严格 1:1**：hook MainProcess 计数 vs 帧号增量 121:121（此前 game_symbols.h 注释"世界内 11.5fps"为早期测量误差，已证伪）
- **帧计数在 MainProcess 末尾递增**（反汇编 d4a20-d4a30）：`STATE_ProcessGame（数据处理+绘画）→ NOTIFIER/SOUND → 帧号+1 → CS_knlSetTimer`——即**Draw 完成之后递增**
- **帧号+1 → 下一帧 GAMESTATE_Process（状态变更）：min 28ms / avg 38.6ms / max 55ms**——采集线程看到帧号变化时，数据已完整且稳定，有 38.6ms 安全窗口采集
- **采集线程 2ms 轮询帧号**，检测变化即采集——采集点必然落在帧边界，不会撞上游戏状态变更
- 主循环调用链：`MainProcess(0xd4984) → [0x2f4000+0xa90]→* STATE_ProcessGame(0x151540) → fpProcess[0x2f3000+0x938] GAMESTATE_Process(0x151264) → fpDraw[0x2f4000+0x930] GAMESTATE_Draw(0x1512b8)`（Process→Draw 仅 0-1ms）

**品质前缀体系（✅ v0.4.62 frida 真机 rarity 0-15 全量测试）**：物品 type bit2-5（rarity 位）直接映射品级前缀：

| rarity | 前缀 | 示例 | 备注 |
|---|---|---|---|
| 0 | 生锈的 | 生锈的 短剑 | 最低品 |
| 1 | 陈旧的 | 陈旧的 短剑 | |
| 2 | （无） | 短剑 | 标准品（CreateItem 默认） |
| 3 | 太古的 | 太古的 短剑 | |
| 4 | 锐利的 | 锐利的 短剑 | |
| 5 | 打磨的 | 打磨的 短剑 | |
| 6 | 工匠的 | 工匠的 短剑 | |
| 7 | 钢铁 | 钢铁 短剑 | |
| 8 | 钛金 | 钛金 短剑 | |
| 9 | 秘银 | 秘银 短剑 | 最高品 |
| 10-14 | （无） | | |
| 15 | 越界异常 | | 忽略 |

- 前缀 = 两个单字 text 表拼接（太+古=太古、锐+利=锐利、打+磨=打磨）
- 实测：rarity=3 时 GetRarity 返回 3（API rarity 字段）、词缀 options=[9,4,6]、magicRate=80（蓝装示例「格斗之剑」）

**词缀 id → 名称（✅ v0.4.62 静态表解析）**：`ITEMOPTINFOBASE` 37 条词缀，u16[0] = text_id（1114-1150）→ text 表 `[名称单字, 属性名]`：

| 词缀索引 | text_id | 名称 | 属性 |
|---|---|---|---|
| 0 | 1114 | 力 | 量 |
| 1 | 1115 | 敏 | 捷 |
| 2 | 1116 | 体 | 力 |
| 3 | 1117 | 智 | 力 |
| 4 | 1118 | 精 | 力 |
| 5 | 1119 | 暴 | 击 |
| 6 | 1120 | 命 | 中 |
| ... | ... | ... | ... |
| 36 | 1150 | 0 | |

- 游戏显示格式 = 词缀名 + 属性名拼接（如「力量」= 力+量）
- 词缀节点 +0x00 u32 高位含词缀 id（0x91d80 格式），需 ITEMSYSTEM_MakeOption 反汇编确认完整映射

**强化（✅ 部分确认）**：`ITEMSYSTEM_ApplyEnchantValue`(0x109890)：
- item+0x1A（enchant u16）→ `UTIL_GetBitValue(10, 6)` 提取 **bit10-15 = 附魔ID**
- 强化值是**数值累加**（`add w0, w21, w19`，w19=[0x2f5000+0x2f0] 查表）
- 上限检查：enchant 与 [0x2f5000+0x2f0] 比较
- ⚠️「已强化/总可强化」位域假设**未证实**——需游戏内实际用强化卷轴观察（依赖 UI 交互，待 P2）

**掉落物数据链（⚠️ 部分确认，v0.4.62 frida 实测）**：
- 生成链：`CHARSYSTEM_Die`(0xf5418) → `CHARSYSTEM_DropItem`(0xf4d30，1768B 大函数) —— 怪死亡触发，frida 实测 3 只怪全部触发 DropItem
- `MAPITEMSYSTEM_ProcessDrop`(0x118398)：从 `*(0x2f5000+0x5d8)`（adrp 定位链表头）→ LINKEDLIST_getHead/getData 遍历 —— **实测链表为空**（哨兵节点全 0），掉落未进此链表
- `MAPITEMSYSTEM_RemoveItem`(0x117020) 反汇编：实体数组 `[实例+0x560]→[0]`，实体步长 0x20（lsl #5），实体 +0x08 = 物品 type（UTIL_GetBitValue(6,15)），计数 `[实例+0x818]` s8 —— 实例定位未完成
- `MAPITEMSYSTEM_Create`(0x118240) 被 MAPSYSTEM_Create 调用，实例 = `[MAPSYSTEM+0x20]`
- `EFFECTSYSTEM_pDropItem`(0x307590)/`EFFECTSYSTEM_AllocateDropItem`(0xf8104)/`ProcessDropItem`(0xf828c) —— 掉落效果系统
- 未确认：掉落实体场景存储位置（不在 MAPITEMSYSTEM 链表/CHARSYSTEM 池/CHARLOC 池）；坐标字段；物品指针字段 —— **backlog P1 待续**
