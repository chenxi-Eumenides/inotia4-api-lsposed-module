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

角色对象入口：`PARTY_pChar`（0x728ec0，24 字节 = **3 个角色指针**）、`CHARSYSTEM_pPool`（0x307538，角色对象池）。

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
| +0x3A0 | u8 | 技能使用次数（低 3 位） | CHAR_GetSkillUsage(0xe496c) |

全局技能定义表：`*(0x2f3758)`（36B/条目：+0x00 member_idx、+0x01 action_id、+0x06 限制、+0x07 上限、+0x09 nibble_index）；技能动作映射 `*(0x2f49e0)`（+0x1D int16）；技能 UI 列表 `*(0x2f6748)`。
动作管理函数：`CHAR_LearnAction`(0xe2390 学习/升级)、`CHAR_FindAction`(0xdd3ac)、`CHAR_ProcessSkillBook`(0xe2488)。
`CHAR_GetName` @0xd9c54：**返回角色名称 UTF-8 字符串**（✅ 2026-08-05 实测：hero="凯恩"、佣兵="沃尔达克/西雷斯/多尔法"）。
> ⚠️ 角色 +0x0A **不是名称 text_id**（英雄 +0x0A=0、沃尔达克 +0x0A=249="死亡之弩"物品名）——名称一律用 `CHAR_GetName(obj)` 获取。

**主属性（✅ 2026-08-05 面板截图+frida 验证）**：

| 函数 | 地址 | 说明 |
|---|---|---|
| `CHAR_GetStat(char, id)` | 0xdf8d0 | 主属性：**0=力量 1=敏捷 2=体力 3=智力 4=精力**（与属性面板完全一致：96/139/101/54/38） |
| `CHAR_GetStatusPoint(char)` | 0xd9c44 | 剩余能力点（面板"能力点:78"） |

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

### 2.4 地图 / 坐标（✅ 实时源已确认，2026-08-05 真机实测）

| 符号 | 地址 | 说明 |
|---|---|---|
| **MAP_nBaseInfo +0** | 0x713878 (u16) | **实时地图 ID**（切地图实测变动；SAVE_nMapID 0x729824 是存档字段，保存时才同步） |
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
- 槽数上限：`*(0x2f3978)`（s8，=88）
- 槽结构（MERCENARYSYSTEM_Set @0x118b94 反汇编确认）：+0x00 type(u8)、+0x01 u8、+0x02 u16、**+0x0B flags(u8: bit0=已占用 bit1=在队伍)**、+0x0C/+0x0E = 角色对象前 4 字节、+0x10 保留
- 角色↔槽关联：角色 +0x352（s8，佣兵槽索引，-1=非佣兵）；**`CHARSYSTEM_FindAsMercenarySlot(slot)` @0xf4254 按槽找角色**（遍历大池：池基址 *(*(0x2f3bb8))、步长 0x430、范围 0x1a2c0、条件 obj[0]!=0 && obj[0x352]==slot）——**未上场佣兵也必须用它**（自实现扫描找不到）
- 管理函数：`MERCENARYSYSTEM_Allocate`(0x118a50)、`MERCENARYSYSTEM_Set`(0x118b94)、`MERCENARYSYSTEM_AddCharacter`(0x118c10)、`MERCENARYSYSTEM_Release`(0x118ab4)
- 符号：`MERCENARYSYSTEM_pSlotList` @0x307750（直接指向槽数组）
- **坑**：刚进入世界时槽数据可能未初始化（垃圾 type=255/flags=255），等几秒重查

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
正确方式：`/proc/self/maps` 取 libgame.so 加载基址（ELF 首 LOAD vaddr=0），符号运行时地址 = 基址 + 符号表 VMA。

```cpp
uintptr_t base = /* /proc/self/maps 第一个 libgame.so 映射 start */;
int64_t money = *(int64_t*)(base + 0x7134c0);          // 直读全局变量
uint16_t mapId = *(uint16_t*)(base + 0x713878);        // 实时地图 ID（MAP_nBaseInfo+0）
auto getMember = (void*(*)(int))(base + 0x11f384);     // 调用 Getter
void* char0 = getMember(0);
int16_t x = *(int16_t*)((uint8_t*)char0 + 0x02);       // 实时坐标
```
代码分层与常量管理详见 `architecture.md`（唯一权威）；本文件仅记录逆向数据源细节。

### 3.2 结构体逆向（✅ M4.1 完成：反汇编 Getter 函数确定偏移）

角色结构体（`PARTY_GetMember(i)` / `PARTY_pChar[0x728ec0]` 指向）：

| 偏移 | 类型 | 含义 | 来源函数 |
|---|---|---|---|
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

**装备操作函数（✅ v0.3.3 逆向，equip 自动替换依据）**：
- `CHAR_CanEquipItem(char, item)` @0xe4eb4：职业掩码/等级校验（先调 `ITEM_IsRealEquip` 读 ITEMDATABASE +2 字节 bit0 判真装备）
- `CHAR_FindEquipSlot(char, item)` @0xe4fd0：计算目标装备槽
- `CHAR_GetEquipItem(char, slot)` @0xda20c：取指定槽装备
- `CHAR_EquipItem(char, item)` @0xe51c0：**目标槽已被占用时返回 0**（e5294: cbnz x0→ret 0）→ 需先卸再穿
- `CHAR_UnequipItemToInven(char, slot)` @0xe2f68：脱下装备槽→背包，返回 1/0

物品结构体（装备/背包物品指针指向，✅ 2026-08-05 探索逆向完成）：

| 偏移 | 类型 | 含义 | 来源函数 |
|---|---|---|---|
| +0x08 | u16 | 类型位域（bit2-5=稀有度、bit5=能力等级、bit6-15=类别） | ITEMSYSTEM_GetRarity/ITEM_GetDamage |
| +0x10 | u32 | 数量/混沌位域（bit0-6=混沌等级、bit8-22=混沌值率、bit25-31=数量） | ITEM_GetChaosLevel/ITEM_GetCumulateCount |
| +0x18 | u8 | 魔法伤害倍率（物理伤害 × 此值/100） | ITEM_GetMagicDamage(0x109c80) |
| +0x19 | u8 | 宝石/插槽位域（bit0-2=已镶宝石数、bit4-6=插槽等级） | ITEMSYSTEM_PutJewel(0x10bcb4)/ApplySocket(0x10d8a4) |
| +0x1a | u16 | 混沌/附魔位域（bit0=有混沌、bit5-6=附魔等级、bit10-15=附魔效果ID） | ITEMSYSTEM_EnchantItem(0x10b330) |
| +0x20 | ptr | 选项链表头（词缀：节点+0x02=s16值、+0x08=下一节点） | ITEM_GetOptionCount/GetOptionValue |

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
- `INVEN_RemoveItem(category)` @0x104044：按类别删第一个物品（内部调 RemoveItemDirect）
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
| 路径结果 | 角色 +0x2F0 | PATHLIST 链表：节点 +0x00 u16 网格x、+0x02 u16 网格y、+0x08 next；**网格×8=像素坐标**；链表=起点→终点 |

> **副作用**：CHAR_SearchPath 仅计算存储路径，**不触发角色移动**（多轮探测位置不变）。
> ⚠️ frida 直接调 ASTAR_GeneratePath 会崩溃（A* 不收敛），弃用；ASTAR 内部：+0x28=碰撞回调=*(0x2f5450)、+0x30=*(0x2f3c80)、+0x38=地图宽*2+1（地图=*(0x2f4e60)）、+0x3c=w5、+0x40=w6；参数(x0=astar, sx>>3, sy>>3, ex>>3, ey>>3, 1, 1)。

**移动执行（✅ v0.3.2 逆向，move 实现依据）**：

| 函数 | 地址 | 说明 |
|---|---|---|
| `CHAR_MoveAsPath(char)` | 0xe9db8 | 沿 +0x2F0 PATHLIST 移动。**玩家控制态（+0x2e2≠0）下要求 +0x278 目标指针非空否则返回 0**；且**只走一步不续走**（游戏主循环不自动跟进），需外部循环调用。AI 单位（+0x2e2=0）可自由调用 |
| `CHAR_Move(char, mode, delta, flag)` | 0xe9808 | 方向键移动：mode 0-3 = 上/下/右/左方向，delta=8 像素/帧；玩家按住方向键时游戏主循环每帧调用。mode>3 直接调用无效 |

> **玩家真实移动机制** = 方向键长按 → 主循环每帧 `CHAR_Move`。API move 实现 = `CHAR_SearchPath` 计算路径 + **临时清零 +0x2e2 控制态 + 循环调用 `CHAR_MoveAsPath` 走完 PATHLIST（上限 512 步）+ 还原控制态**（仍走游戏合法寻路链路，非 OP 传送）。

### 3.3 事件通知（可选增强）hook 变更函数（inline hook：ShadowHook/xHook 或 frida-gum 静态）：
- `INVEN_AddMoney` / `INVEN_SetMoney` → 金币变化
- `CHAR_AddExperience` / `CHAR_SetLevel` → 升级
- `PARTY_AddHPMP` / `CHAR_AddLife` / `CHAR_AddMana` → 血量/魔法变化
- `PARTY_AddMember` / `PARTY_Exclude` → 队伍变化
- `INVEN_MoveItem` / `INVEN_RemoveItem` → 背包变化

### 3.4 风险

- 结构体偏移因游戏版本而异（本次以盗版大修 v5.0 为准）
- `MAP_nFocusX/Y` 是焦点而非精确玩家坐标（已确认弃用，用角色 +0x02/+0x04）
- 模块 native 层加载于游戏进程内，理论上可用 dlopen/dlsym，但 **namespace 隔离会加载独立副本读不到游戏数据**（实测全 0）——必须用 base+VMA 直读

### 3.5 游戏主循环帧率（P0-2，2026-08-08 frida 实测）

- **`MainProcess`(0xd4984) 恒定 ~16.9fps**，不随界面/战斗状态变化（主菜单 16.9fps = 世界活跃 16.9fps，两轮 30s 采样一致）
- 主循环**无条件逐帧调用**，无休眠退避；backlog 早期记录 17.4fps 为测量窗口差异
- **含义**：events 轮询采样间隔**不受游戏状态影响**，可固定 500ms-1s（每帧 ~59ms，采样间隔远超单帧时间，不会漏事件）
- frida 探测脚本：`scripts/analyze/run_probe.py` + `/tmp/opencode/fps_probe.js`（Interceptor.attach MainProcess + 计时统计）
