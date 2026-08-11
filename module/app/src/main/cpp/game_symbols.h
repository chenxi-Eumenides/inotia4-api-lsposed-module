#pragma once

#include <cstdint>

// ============================================================
// 游戏符号与结构体布局定义（单一来源）
// 由 scripts/analyze/check_symbols.py 解析校验；改动后需同步运行该校验。
// 结构体偏移来源：docs/notes/hook-points.md §3.2（M4.1 反汇编逆向）
// VMA 来源：libgame-symbols.txt（readelf 符号表）
// ============================================================

// ---- 角色结构体偏移 ----
constexpr size_t C_TYPE = 0x09;      // int8 角色类型 (0=英雄 1=佣兵)
constexpr size_t C_NAME_ID = 0x0A;   // u16 名称相关 ID（非 text_id；角色名称须用 CHAR_GetName 获取）
constexpr size_t C_LEVEL = 0x0E;     // int8 等级
constexpr size_t C_ATTR = 0x24;      // int32 属性数组 [char + attr_id*4 + 0x24]
constexpr size_t C_HP = 0x1F0;       // int32 当前 HP
constexpr size_t C_MP = 0x1F4;       // int32 当前 MP
constexpr size_t C_EQUIP = 0x1F8;    // 装备槽数组 (10 槽 × 8B 指针)
constexpr size_t C_EXP = 0x318;      // int64 当前经验
constexpr size_t C_NEXT_EXP = 0x320; // int64 升级所需经验
constexpr size_t C_STATUS = 0x311;   // u8 状态码 (0=队伍 1=城镇NPC/佣兵 2=怪物/召唤物, frida 实测)
constexpr size_t C_SKILL_LIST = 0x2A0;    // 已学战斗技能链表头（节点见 S_* 偏移）
constexpr size_t C_SKILL_BMP = 0x2B0;     // u16 技能解锁位图
constexpr size_t C_ACTIVE_SKILL = 0x280;  // 当前激活技能节点指针
constexpr size_t C_SKILL_POINTS = 0x328;  // int8 剩余技能点
constexpr size_t C_MERC_SLOT = 0x352;     // s8 佣兵槽索引（-1=非佣兵, frida 实测）
constexpr size_t C_PATH_LIST = 0x2F0;     // 寻路结果 PATHLIST 链表头（节点 +0x00 u16 网格x/+0x02 u16 网格y/+0x08 next）
constexpr size_t C_CTRL_STATE = 0x2E2;    // u8 控制状态（0=AI可自由寻路 7=玩家控制 135=战斗态, frida 实测）
constexpr size_t C_MOVE_TARGET = 0x278;   // 移动目标指针（MoveAsPath 在控制态下要求非空）
constexpr size_t C_EQUIP_SLOTS = 10;
constexpr size_t C_POS_X = 0x02;     // int16 实时 X（CHAR_GetDistance 反汇编证实）
constexpr size_t C_POS_Y = 0x04;     // int16 实时 Y
constexpr size_t C_OBJ_SIZE = 0x430; // 角色对象步长（CHARSYSTEM 池相邻对象间隔, frida 实测）

// ---- CHARLOC 位置登记结构（CHARLOC_Copy/Add 反汇编确认，10B/条）----
constexpr size_t CHARLOC_SIZE = 0x0A; // 位置条目步长（Add 中 idx*8 + idx*2 = idx*10）
constexpr size_t LOC_TYPE = 0x00;     // u8 单位类型
constexpr size_t LOC_POS_X = 0x02;    // u16 实时 X
constexpr size_t LOC_POS_Y = 0x04;    // u16 实时 Y

// ---- 技能节点结构偏移（角色 C_SKILL_LIST 链表，frida 实测 2026-08-05）----
constexpr size_t S_ACTION_ID = 0x00; // u16 技能 action_id
constexpr size_t S_LEVEL = 0x02;     // u8 技能等级
constexpr size_t S_NEXT = 0x18;      // 下一节点指针

// ---- 佣兵槽结构偏移（20B/槽，MERCENARYSYSTEM_Set 反汇编确认）----
constexpr size_t M_TYPE = 0x00;   // u8 类型
constexpr size_t M_FLAGS = 0x0B;  // u8 flags (bit0=已占用 bit1=在队伍)
constexpr size_t M_SLOT_SIZE = 0x14;

// ---- 物品结构体偏移 ----
constexpr size_t I_TYPE = 0x08;  // u16 类型位域 (bit2-5=稀有度, bit6-15=类别)
constexpr size_t I_COUNT = 0x10; // u32 数量位域 (bit25-31：0=不可堆叠、100=装备、1~99=可堆叠数量，上限99)
constexpr size_t I_MAGIC_RATE = 0x18; // u8 魔法伤害倍率（物理伤害×此值/100）
constexpr size_t I_SOCKET = 0x19;     // u8 宝石/插槽位域 (bit0-2=已镶宝石数 bit4-6=插槽等级)
constexpr size_t I_ENCHANT = 0x1A;    // u16 混沌/附魔位域 (bit0=有混沌 bit5-6=附魔等级 bit10-15=附魔ID)
constexpr size_t I_OPTION_LIST = 0x20; // 词缀链表头（节点见 O_* 偏移）

// ---- 词缀节点结构偏移（物品 I_OPTION_LIST 链表）----
constexpr size_t O_VALUE = 0x02; // s16 词缀值
constexpr size_t O_NEXT = 0x08;  // 下一节点指针

// ---- HP/MP 上限的属性 id ----
constexpr int ATTR_MAX_HP = 0x1e;
constexpr int ATTR_MAX_MP = 0x1f;

// ---- 全局变量 VMA ----
constexpr uintptr_t G_MONEY_VMA = 0x7134c0;        // int64 金币
constexpr uintptr_t G_MAP_ID_VMA = 0x713878;       // MAP_nBaseInfo +0 (u16) 实时地图 ID（切地图实测变动）
constexpr uintptr_t G_PARTY_VMA = 0x728ec0;        // 3 个角色指针
constexpr uintptr_t G_ACTIVE_QUEST_VMA = 0x728ff8; // u16 当前任务
constexpr uintptr_t G_INVEN_VMA = 0x7131c0;        // INVEN_pItem 背包槽数组（6袋×0x80，每槽8B 物品指针）
constexpr uintptr_t G_BAG_TABLE_VMA = 0x2f3bc0;    // GOT 槽：*(0x2f3bc0) = 袋表指针（INVEN_GetBagSize 反汇编）
constexpr uintptr_t G_MAIN_MERC_SLOT_VMA = 0x729826; // SAVE_nMainMercenarySlot (u8) 当前控制角色槽
constexpr uintptr_t G_CHAR_POOL_VMA = 0x307538;      // CHARSYSTEM_pPool 角色对象池（指向英雄对象，0x430/对象）
constexpr uintptr_t G_DEALSYSTEM_SALE_LIST_VMA = 0x2f3000 + 0x490;  // GOT 槽：*(此地址) = 商店商品表基址（48 槽 × 16B，步长 0x10；+0 位域 bit0=空、+8 商品对象指针）
constexpr uintptr_t G_CHARLOC_POOL_VMA = 0x307530;   // CHARLOCSYSTEM_pPool 位置登记池（CHARLOC_Copy 反汇编：10B/条）
constexpr uintptr_t G_CHARLOC_COUNT_VMA = 0x307528;  // CHARLOCSYSTEM_nCount (u16) 位置登记条数
constexpr uintptr_t G_PREV_STATE_VMA = 0x307490;      // STATE_nPrevState (u16) 上一个 UI 状态（readelf 符号表）
constexpr uintptr_t G_STATE_VMA = 0x307492;          // STATE_nState (u16) UI 状态机（4=主菜单流程 5=游戏中, frida 实测）
constexpr uintptr_t G_GAMESTATE_VMA = 0x72b068;      // GAMESTATE_nState (u32) 游戏状态
constexpr uintptr_t G_FRAME_COUNT_VMA = 0x2f5648;  // 帧计数 GOT 槽：*(此地址)=u64 计数指针（frida 实证：世界内 2 秒 +23 ≈ 11.5fps；FPS 系统 0x3075f0 未启用恒 0）
constexpr uintptr_t G_INITSTATE_VMA = 0x72b06d;      // INITSTATE_nState (u8) 初始化状态
constexpr uintptr_t G_POPUP_ON_VMA = 0x3070e8;       // UIPopupMsg_bOn (u8) 弹窗/对话框是否激活（readelf 符号表）
constexpr uintptr_t G_POPUP_TEXT_VMA = 0x3070b8;     // UIPopupMsg_pText (8B) 弹窗打开时指向当前文本（v0.3.10 真机验证）
constexpr uintptr_t G_POPUP_FPOK_VMA = 0x3070e0;     // UIPopupMsg_fpOK (8B) 确定回调（非空=有确认按钮）
constexpr uintptr_t G_POPUP_FPCANCEL_VMA = 0x3070d8; // UIPopupMsg_fpCancel (8B) 取消回调（非空=有取消按钮）
constexpr uintptr_t G_POPUP_TYPE_VMA = 0x712518;     // 弹窗类型 (i32)（debug 端点）
constexpr uintptr_t G_POPUP_DISPTYPE_VMA = 0x712510; // 弹窗显示类型 (i32)（debug 端点）
constexpr uintptr_t G_MAINMENU_DRAW_VMA = 0x72a0f8;  // UIMainMenu_bDrawFull (u8) 主菜单是否完整绘制（readelf 符号表）
constexpr uintptr_t G_POPUP_STACK_VMA = 0x728fd8;    // g_arrPopupStack (32B) UI 弹窗栈（readelf 符号表）
constexpr uintptr_t G_MERC_SLOTLIST_GOT_VMA = 0x2f6010; // 佣兵槽数组指针（需双层解引用 *(*(base+0x2f6010))，20B/槽）
constexpr uintptr_t G_PLAYER_NEAR_NPC_VMA = 0x728fb8;   // PLAYER_pNearNPC（写者 PLAYER_DoCheckNearNPC 0x120d14）
constexpr uintptr_t G_NPCTASKLIST_INDEX_VMA = 0x307820; // NPCTASKLIST_nIndex (u8) 当前任务索引
constexpr uintptr_t G_NPCTASKLIST_COUNT_VMA = 0x307821; // NPCTASKLIST_nCount (u8) 任务数
constexpr uintptr_t G_NPCTASKLIST_PDATA_VMA = 0x307818; // NPCTASKLIST_pData（8B → 32×16B 槽数组：+0 u8 type、+2 u16 id）
constexpr uintptr_t G_NPCTASKLIST_DESCTEXT_VMA = 0x307810; // NPCTASKLIST_pDescText（对话描述文本）
constexpr uintptr_t G_UICHOICE_ITEMTEXT_VMA = 0x711c60; // UICHOICE_pItemText（6×8B 指针数组选项文本）
constexpr uintptr_t G_UICHOICE_COUNT_VMA = 0x302d70;    // UICHOICE_nItemCount (u8 选项数 ≤6)
constexpr uintptr_t G_UICHOICE_FOCUS_VMA = 0x302d80;    // UICHOICE_nFocusIndex (u8 焦点索引)
constexpr uintptr_t G_NPCSEL_ID_VMA = 0x728e8e;         // nSelectedID (u16 选中任务 ID)
constexpr uintptr_t G_NPCSEL_TYPE_VMA = 0x728e90;       // nSelectedType (u8 选中任务类型)
constexpr uintptr_t G_MERC_MAX_VMA = 0x2f3978;       // 佣兵槽上限 (s8)
constexpr uintptr_t G_TILE_GOT_VMA = 0x2f3f48;       // MAP 通行矩阵 GOT（双层解引用 *(*(base+0x2f3f48))，MAP_IsBlocking 反汇编确认；frida 实测与 MAP_nBaseTile 0x7148a8 非同一数据——0x7148a8 为渲染基础瓦片）
constexpr size_t TILE_ROW_STRIDE = 64;               // 瓦片行字节步长（MAP_IsBlocking 中 y*64+x 索引）
constexpr uint8_t TILE_BLOCK_BIT = 0x08;             // 阻挡标志位（ubfx bit3）

// ---- 骰子（STATUSDICE）状态 ----
// 两处均为 GOT 槽：先解引用取指针，再按位操作（STATUSDICE_Roll/Apply/UI 按钮反汇编确认）。
constexpr uintptr_t G_STATUSDICE_PENDING_GOT_VMA = 0x2f5740;  // GOT 槽：*(此地址) = pending int8[5] 数组指针（STATUSDICE_Roll 写入/Apply 读取，5 项基础属性掷骰结果）
constexpr uintptr_t G_STATUSDICE_FLAG_GOT_VMA = 0x2f37b8;     // GOT 槽：*(此地址) = 确认标志 u8 指针，bit0=1 有未确认掷骰结果（ButtonRollExe 置位、Create/Apply 复位）

// ---- 函数 VMA ----
constexpr uintptr_t F_GET_MONEY_VMA = 0x10445c;      // int64 ()
constexpr uintptr_t F_GET_MEMBER_VMA = 0x11f384;     // void* (int)
constexpr uintptr_t F_GET_PARTY_SIZE_VMA = 0x11f3a4; // int ()
constexpr uintptr_t F_GET_ATTR_VMA = 0xdfd18;        // int32 (void*, int)
constexpr uintptr_t F_GET_EQUIP_VMA = 0xda20c;       // void* (void*, int)
constexpr uintptr_t F_GET_EXP_VMA = 0xd9b54;         // int64 (void*)
constexpr uintptr_t F_GET_NEXT_EXP_VMA = 0xd9b68;    // int64 (void*)
constexpr uintptr_t F_GET_RARITY_VMA = 0x10d700;     // int (void*)
constexpr uintptr_t F_GET_BAG_SIZE_VMA = 0x103250;   // int (int)
constexpr uintptr_t F_GET_BIT_VMA = 0x140528;        // int (int,int,int)
constexpr uintptr_t F_GET_DAMAGE_VMA = 0x1099f0;     // int (void*) 物品攻击
constexpr uintptr_t F_GET_DEFENSE_VMA = 0x109cc0;    // int (void*) 物品防御
constexpr uintptr_t F_GET_STAT_VMA = 0xdf8d0;        // int (void*, int) 主属性总属性=Base+Main+Bonus+Sub (0=力量 1=敏捷 2=体力 3=智力 4=精力)
constexpr uintptr_t F_GET_STAT_BASE_VMA = 0xdb9e4;   // int (void*, int) 基础属性 [ch+0x250+i] s8
constexpr uintptr_t F_GET_STAT_BONUS_VMA = 0xdb9fc;  // int (void*, int) 加成属性 [ch+0x260+i] s8（存档独立保存）
constexpr uintptr_t F_GET_STATUS_POINT_VMA = 0xd9c44; // int (void*) 剩余能力点
constexpr uintptr_t F_GET_STAT_MAIN_VMA = 0xdb9f0;    // int (void*, int) 读主属性 [ch+0x256+i*2]（i=0-4 力量/敏捷/体力/智力/精力）
constexpr uintptr_t F_SET_STAT_MAIN_VMA = 0xdf1c4;    // void (void*, int, int) 写主属性 + CHAR_ResetAttrFromStat 重算衍生
constexpr uintptr_t F_SET_STAT_BASE_VMA = 0xdf170;    // void (void*, int, int) 写基础属性 [ch+0x250+i] s8 + 重算衍生 + SV 同步
constexpr uintptr_t F_PUT_JEWEL_VMA = 0x10bcb4;       // int (void*, void*) 镶嵌宝石（equipItem+jewelItem）；返回 0=成功/2=无孔/3=非宝石或空装备
constexpr uintptr_t F_IS_JEWEL_VMA = 0x10b964;        // int (int32_t) 类别是否为宝石
constexpr uintptr_t F_CHAR_INITIALIZE_STATUS_VMA = 0xe68c8;  // void (void*) 属性重置：5 项主属性归 0 + 能力点按 (等级-1)×职业基础值 还原
constexpr uintptr_t F_CHAR_INITIALIZE_SKILL_VMA = 0xe67c8;   // void (void*) 技能重置：移除技能链表非基础技能（ACTLIST_RemoveNode）+ 技能点按职业还原（CHAR_SetSkillPoint）+ 清快捷键 + 重算属性
constexpr uintptr_t F_CHAR_SET_ACTION_ID_VMA = 0xe79ec;      // void (void*, int32_t, void*) 释放技能动作（ch+actionId+目标指针；内部 FindAction→SetAction 写 [ch+0x280]）。⚠️ 第 3 参是目标对象指针非 level（技能动作 type==2 读 [target+2]/[target+4] 坐标算朝向）
constexpr uintptr_t F_SET_LEVEL_VMA = 0xe05a0;               // int (void*, int32_t) 设置角色等级：写 [ch+0xe] + CHAR_SetNextExperience(0xd9c28) + CHAR_InitializeFromLevel(0xdf2c0) + 升级加能力点/技能点（表驱动）+ 回满血蓝（C_HP/C_MP=GetAttr(0x1e/0x1f)）。⚠️ 只允许升级/同级（b.le 分支），降级直接返回 0
constexpr uintptr_t F_CHAR_GET_ENEMY_TARGET_VMA = 0xe42b4;   // void* (void*, int32_t, int32_t) 获取敌人目标（[ch+0x2c8] bit13 或 [ch+0x278] 有则返回，否则 FindBestTargetByAct 自动找）
constexpr uintptr_t F_QUESTSYSTEM_FIND_VMA = 0x122914;       // int (int32_t) 按 questId 找任务槽索引（槽数组 [0x2f4000+0x3d0] 步长 12B +0 questId u16；未找到返回 -1）
constexpr uintptr_t F_QUESTSYSTEM_REMOVE_SLOT_VMA = 0x1229a4;  // int (int32_t) 删除任务槽（CopySlot 前移 + QUEST_Initialize 末槽清空 + 槽数-1；返回 1 成功）
constexpr uintptr_t F_SAVE_VMA = 0x129600;                // int (void) 完整静默保存（内部校验 SV_GoldGet/StatPoint/SkillPoint → KEY_ResetActive → 细分 SaveInformation/Player/CharacterAll/Inventory/Quest/Event/ETC 序列化；校验失败返回 0）
constexpr uintptr_t F_GAMESTATE_SET_STATE_VMA = 0x151590;  // void (int32_t) 游戏状态机切换（STATE_nState：4=主菜单 5=world；state==4 分支 GAME_Exit + STATE_Set(4) + Enter 回调）
constexpr uintptr_t F_SAVE_GET_SAVE_SLOT_VMA = 0x1289e4;    // void* (int32_t) 存档槽结构指针（[0x2f5000+0xe40] + slot×0x1d；slot>2 返 0）。槽结构：b0=存在标志 b2=槽标志 +0x1c=角色类型
constexpr uintptr_t F_UI_SET_POPUP_PROCESS_INFO_VMA = 0xaecc8;  // int (int32_t id, int32_t data) 注册 popup 流程（Array_Add 到 popup 数组 [0x2f5000+0xc38]）
constexpr uintptr_t F_GAME_START_RESUME_GAME_VMA = 0x1002e8;  // int (int32_t slot) 启动游戏读档（GAME_Initialize → [0x2f6000+0xd20]=slot → STATE_Set(5) → MAPCHANGE_Set → GAMESTATE_SetState(3) → 主循环读档进 world）
constexpr uintptr_t F_SAVE_CREATE_SAVE_SLOT_VMA = 0x129b38;    // void (void) 初始化全部 3 槽（循环 SAVESLOT_Initialize + SAVE_LoadSaveSlot 加载存档到槽区）
constexpr uintptr_t F_SAVESLOT_GET_HERO_VMA = 0x14cda4;       // void* (void*) 取主控角色指针（[slot+0x1c] 索引 → [slot+0x4+idx*8]）
constexpr uintptr_t F_UINPC_INIT_VMA = 0xc2cfc;              // u8 (void) NPC 交互触发（UINpc_InitNPC：建 NPCBOX+任务列表+功能列表；前置 PLAYER_pNearNPC 已设）
constexpr uintptr_t F_UINPC_EXE_CURRENT_TASK_VMA = 0xc3070;  // void (void) 执行当前选中任务（slot=GetSlot(nIndex)→SetSelectedTask→ExeNpcTask 跳转表）
constexpr uintptr_t F_NPCTASKLIST_MAKE_DLG_VMA = 0x11e6a4;   // char* (void) 对话下一句（按 slot type 读 desc 表文本 ID → MEMORYTEXT）
constexpr uintptr_t F_PLAYER_DO_CHECK_NEAR_NPC_VMA = 0x120d14; // void (void) 检查附近 NPC（设 PLAYER_pNearNPC=0x728fb8，type==1 非队员距离<0x18）
constexpr uintptr_t F_CHAR_GET_SKILL_USAGE_VMA = 0xe496c;    // int (void*) 战斗 AI 技能总开关（读 [ch+0x3a0] bit0-2）
constexpr uintptr_t F_CHAR_SET_SKILL_USAGE_VMA = 0xe4cc0;    // void (void*, int) 写 [ch+0x3a0] bit0-2（AI 技能开关 0-7）
constexpr uintptr_t F_GET_NAME_VMA = 0xd9c54;         // char* (void*) 角色名称（UTF-8 字符串）
constexpr uintptr_t F_FIND_MERC_SLOT_VMA = 0xf4254;   // void* (int) 按佣兵槽找角色（CHARSYSTEM_FindAsMercenarySlot）
constexpr uintptr_t F_SEARCH_PATH_VMA = 0xdb094;      // int (void*, int, int, int) 角色寻路（CHAR_SearchPath：目标像素+flag）

// ---- 写操作函数 VMA（2026-08-05 objdump 逆向确认，见 docs/notes/control-capability.md §5）----
constexpr uintptr_t F_SET_MONEY_VMA = 0x10449c;        // void (int64) 设金币
constexpr uintptr_t F_ADD_MONEY_VMA = 0x1044e4;        // int (int64) 加金币（溢出返回 0）
constexpr uintptr_t F_MINUS_MONEY_VMA = 0x104780;      // int (int64) 减金币（不足返回 0）
constexpr uintptr_t F_REMOVE_ITEM_VMA = 0x104044;      // int (void*) INVEN_RemoveItem 按 item 指针删（内部 FindItemSlot+RemoveItemDirect）
constexpr uintptr_t F_ITEM_GET_PRICE_VMA = 0x109f50;   // int (void*) ITEM_GetPrice 读静态表价格（item+8 字段 + ITEM_GetAbilityLevel）
constexpr uintptr_t F_ITEM_GET_BUY_PRICE_VMA = 0x10a200;  // int (void*) 买入价（ITEM_GetPrice + MERCENARYGROUPSKILLSYSTEM 折扣系数）
constexpr uintptr_t F_INVEN_FIND_SAVE_SLOT_VMA = 0x103960;  // int (void*) 找背包空槽（返回 bag*16+slot 或 -1）
constexpr uintptr_t F_INVEN_SAVE_ITEM_VMA = 0x104528;   // int (int32_t, void*) 物品存入背包槽
constexpr uintptr_t F_DEALSYSTEM_FIND_SALE_BY_ID_VMA = 0xf636c;  // void* (void*) 按物品类别找商店商品槽（遍历 saleList 到 +0x300 步长 0x10）
constexpr uintptr_t F_INVEN_MOVE_ITEM_VMA = 0x104934;  // int (void*,int,int,int) INVEN_MoveItem 物品移动/堆叠合并（item+count+targetBag+targetSlot）
constexpr uintptr_t F_SET_EXP_VMA = 0xd9b5c;           // void (void*, int32) 设经验
constexpr uintptr_t F_ADD_EXP_VMA = 0xe7028;           // int (void*, int32, u8) 加经验（走升级判定链）
constexpr uintptr_t F_SET_STATUS_POINT_VMA = 0xd9c4c;  // void (void*, int32) 设能力点（写 +0x32a）
constexpr uintptr_t F_SET_AUTO_ATTACK_VMA = 0xe4cf4;   // void (void*, int32) 自动攻击开关
constexpr uintptr_t F_EQUIP_ITEM_VMA = 0xe51c0;        // int (void*, void*) 穿装备（自动找槽，槽占用返回 0）
constexpr uintptr_t F_UNEQUIP_VMA = 0xe2f68;           // int (void*, int32) 脱装备槽→背包
constexpr uintptr_t F_CAN_EQUIP_VMA = 0xe4eb4;         // int (void*, void*) 可否装备
constexpr uintptr_t F_FIND_EQUIP_SLOT_VMA = 0xe4fd0;   // int (void*, void*) 计算目标装备槽（-1=不可装备）
constexpr uintptr_t F_GET_EQUIP_ITEM_VMA = 0xda20c;    // void* (void*, int32) 读指定装备槽物品指针
constexpr uintptr_t F_IS_SPECIAL_NPC_VMA = 0xe4d90;    // int (void*) 是否任务特殊 NPC（type==2 且表 bit2）
constexpr uintptr_t F_LEARN_ACTION_VMA = 0xe2390;      // void* (void*, int32, int32) 学习/升级技能
constexpr uintptr_t F_SET_ACTIVE_PLAYER_VMA = 0x11f584; // int (int32) 切换主控角色
constexpr uintptr_t F_PARTY_SWAP_VMA = 0x11ff5c;       // void (int32, int32) 交换队伍槽
constexpr uintptr_t F_SET_POSITION_VMA = 0x12aa14;     // void (int32, int32) 全队传送（写 +0x2/+0x4）
constexpr uintptr_t F_CHANGE_MAP_VMA = 0x114fc4;       // void (int32, int32, int32, int32) 切图（mapId,x,y,dir）

// ---- 合法操作函数 VMA（v0.3.1，玩家游戏内可做的事，见 control-capability.md §5.1）----
constexpr uintptr_t F_MOVE_AS_PATH_VMA = 0xe9db8;      // int (void*) 沿已存路径移动（读 +0x2f0 PATHLIST）
constexpr uintptr_t F_CHAR_MOVE_VMA = 0xe9808;         // int (void*, int, int*, u8) 方向键移动（mode 0-3=上/下/右/左，delta 像素/帧，flag 方向键状态）
constexpr uintptr_t F_CHAR_REMOVE_PATH_VMA = 0xdb064;  // void (void*) 清除已存路径（打断移动）
constexpr uintptr_t F_MAP_SET_FOCUS_VMA = 0x11336c;    // void (int32 x, int32 y) 像素坐标；写焦点 + MAP_SetDisplayInformation 转 4 个滚动偏移（摄像机=MAP Focus 体系）
constexpr uintptr_t F_GAMEPLAY_GO_MAP_LINK_BY_CHAR_VMA = 0x9cdc0;  // int (void* ch, int32 tile_x, int32 tile_y) 按角色触发出口检测→MAPCHANGE_Set→切图状态机
constexpr uintptr_t F_CHAR_SET_TARGET_VMA = 0xdc754;    // void (void*, void*) 设置攻击目标（写 [ch+0x278]）
constexpr uintptr_t F_CHAR_STOP_COMBAT_VMA = 0xe7c24;   // void (void*) 停止战斗（清战斗标志+移除仇恨+动作复位）
constexpr uintptr_t F_CONSUME_ITEM_VMA = 0x1047bc;     // void (void*) 消耗 1 个（使用药水/卷轴）
constexpr uintptr_t F_CHAR_USE_ITEM_EX_VMA = 0xeb670;  // void (void* ch, void* item, int flag) 物品效果分派核心
constexpr uintptr_t F_REMOVE_ITEM_DIRECT_VMA = 0x103fd8; // int (int32 bag, int32 slot) 按槽删物品
constexpr uintptr_t F_INCLUDE_PARTY_VMA = 0x118e04;    // int (void*) 佣兵入队（内部校验）
constexpr uintptr_t F_EXCLUDE_PARTY_VMA = 0x118d0c;    // int (void*) 佣兵离队
constexpr uintptr_t F_MERCENARY_RELEASE_VMA = 0x118ab4; // void (int) 佣兵遣散：FindAsMercenarySlot→清 +0x352/-0x3cc 标志→MERCENARYSLOT_Initialize→GAMESTATE_SetState
constexpr uintptr_t F_ITEMDATA_IS_USE_VMA = 0x1058ac;  // int (int32 itemId) 物品是否可使用（ITEMDATABASE_IsUse，读表 +2 u8 ∈ {0x16,0x17} 可消耗）
constexpr uintptr_t F_POPUPSTATE_EXIST_VMA = 0x1223f8; // int () 弹窗栈是否有激活状态（readelf 符号表）
constexpr uintptr_t F_BUTTON_OK_EXE_VMA = 0xca9d8;      // void () 弹窗确定按钮执行（bOn=0 + 调 fpOK(param)；无参直接调用，v0.3.11 frida 验证）
constexpr uintptr_t F_BUTTON_CANCEL_EXE_VMA = 0xcaa78;  // void () 弹窗取消按钮执行（bOn=0 + 调 fpCancel(param) 或 Free；无参直接调用）
constexpr uintptr_t F_NETWORKSTORE_SET_STATE_VMA = 0x15b0c4;  // void (int) 写 NetworkStore state（0 复位）——反汇编证实 SetState@0x15b0c4，0x15b0d0 是 GetState（只读）
constexpr uintptr_t F_OPEN_ITEM_BOX_VMA = 0x10e970;   // int (int32_t category) 开箱（按表权重随机出物品，内部调 INVEN_SaveItem）
constexpr uintptr_t F_RELEASE_SEALED_VMA = 0x10af4c;  // int (int32_t category) 解封（类别需在 0x3a6-0x3ab）
constexpr uintptr_t F_IS_DICE_VMA = 0x10be60;         // int (int32_t category) 是否骰子（类别 ∈[0x34,0x38]）
constexpr uintptr_t F_STATUSDICE_ROLL_VMA = 0x138338; // int (int32_t charIdx, int32_t type) 掷骰：纯表驱动计算写 pending[0..4]（charIdx=[ch+0xd] 0-5 职业索引，type=category-0x34 0-4；不读 UI，同步调用安全）
constexpr uintptr_t F_IS_SEALED_VMA = 0x10be50;       // int (int32_t category) 是否可解封（类别 ∈[0x3a6,0x3ab]，与 ReleaseSealed 内联判定一致）
constexpr uintptr_t F_IS_ITEMBOX_VMA = 0x10cda0;     // int (int32_t category) 是否开箱类（类别 ∈[0x3ef,0x3f1]，UIEquip_SetDescMenu 开箱按钮判定）
constexpr uintptr_t F_MAKE_ITEM_VMA = 0x10c6c8;      // void* (int32_t category, int32_t count, int32_t flag) ITEMSYSTEM_MakeItem 创建物品对象
constexpr uintptr_t F_CREATE_ITEM_VMA = 0x10be9c;    // void* (int32_t category, int32_t, int32_t, int32_t) ITEMSYSTEM_CreateItem 创建物品对象（无 search_tbl 校验，OP 直调可靠）

// ---- 函数签名 ----
using GetMoneyFn = int64_t (*)();
using GetMemberFn = void* (*)(int);
using GetPartySizeFn = int (*)();
using GetAttrFn = int32_t (*)(void*, int);
using GetEquipFn = void* (*)(void*, int);
using GetExpFn = int64_t (*)(void*);
using GetRarityFn = int (*)(void*);
using GetBagSizeFn = int (*)(int);
using GetBitFn = int (*)(int, int, int);
using GetItemStatFn = int (*)(void*);
using GetAttrFn2 = int (*)(void*, int);
using GetStatusPointFn = int (*)(void*);
using GetNameFn = char* (*)(void*);
using FindMercSlotFn = void* (*)(int);
using SearchPathFn = int (*)(void*, int, int, int);

// ---- 写操作函数签名 ----
using SetMoneyFn = void (*)(int64_t);
using AddMoneyFn = int (*)(int64_t);
using RemoveItemFn = int (*)(void*);          // INVEN_RemoveItem(item 指针)
using ItemGetPriceFn = int (*)(void*);        // ITEM_GetPrice(item 指针) → 静态表价格
using InvenMoveItemFn = int (*)(void*, int, int, int);  // INVEN_MoveItem(item, count, targetBag, targetSlot)
using SetExpFn = void (*)(void*, int32_t);
using SetLevelFn = int (*)(void*, int32_t);   // CHAR_SetLevel(0xe05a0)：返回 1=成功（升级/同级）/ 0=降级拒绝
using AddExpFn = int (*)(void*, int32_t, uint8_t);
using SetStatusPointFn = void (*)(void*, int32_t);
using GetStatMainFn = int (*)(void*, int32_t);
using SetStatMainFn = void (*)(void*, int32_t, int32_t);
using SetStatBaseFn = void (*)(void*, int32_t, int32_t);
using RollStatusDiceFn = int (*)(int32_t, int32_t);
using PutJewelFn = int (*)(void*, void*);
using IsJewelFn = int (*)(int32_t);
using CharInitializeStatusFn = void (*)(void*);
using CharInitializeSkillFn = void (*)(void*);
using CharSetActionIdFn = void (*)(void*, int32_t, void*);
using CharGetEnemyTargetFn = void* (*)(void*, int32_t, int32_t);
using QuestSystemFindFn = int (*)(int32_t);
using QuestSystemRemoveSlotFn = int (*)(int32_t);
using SaveFn = int (*)();
using GamestateSetStateFn = void (*)(int32_t);
using SaveGetSaveSlotFn = void* (*)(int32_t);
using UiSetPopupProcessInfoFn = int (*)(int32_t, int32_t);
using GameStartResumeGameFn = int (*)(int32_t);
using SaveCreateSaveSlotFn = void (*)();
using SaveslotGetHeroFn = void* (*)(void*);
using ItemGetBuyPriceFn = int (*)(void*);
using InvenFindSaveSlotFn = int (*)(void*, int32_t);
using InvenSaveItemFn = int (*)(void*, void*);
using DealSystemFindSaleByIdFn = void* (*)(void*);
using UinpcInitFn = uint8_t (*)();
using UinpcExeTaskFn = void (*)();
using NpctasklistMakeDlgFn = char* (*)();
using PlayerCheckNearNpcFn = void (*)();
using GetSkillUsageFn = int (*)(void*);
using SetSkillUsageFn = void (*)(void*, int32_t);
using SetAutoAttackFn = void (*)(void*, int32_t);
using EquipItemFn = int (*)(void*, void*);
using UnequipFn = int (*)(void*, int32_t);
using CanEquipFn = int (*)(void*, void*);
using FindEquipSlotFn = int (*)(void*, void*);
using GetEquipItemFn = void* (*)(void*, int32_t);
using IsSpecialNpcFn = int (*)(void*);
using LearnActionFn = void* (*)(void*, int32_t, int32_t);
using SetActivePlayerFn = int (*)(int32_t);
using PartySwapFn = void (*)(int32_t, int32_t);
using SetPositionFn = void (*)(int32_t, int32_t);
using ChangeMapFn = void (*)(int32_t, int32_t, int32_t, int32_t);

// ---- 合法操作函数签名 ----
using MoveAsPathFn = int (*)(void*);
using CharMoveFn = int (*)(void*, int, int, unsigned char);
using CharRemovePathFn = void (*)(void*);
using MapSetFocusFn = void (*)(int32_t, int32_t);
using GoMapLinkByCharFn = int (*)(void*, int32_t, int32_t);
using CharSetTargetFn = void (*)(void*, void*);
using CharStopCombatFn = void (*)(void*);
using ConsumeItemFn = void (*)(void*);
using CharUseItemExFn = int (*)(void*, void*, int);  // 返回 1=成功(内部已消耗) 0=失败
using RemoveItemDirectFn = int (*)(int32_t, int32_t);
using IncludePartyFn = int (*)(void*);
using ExcludePartyFn = int (*)(void*);
using MercenaryReleaseFn = void (*)(int32_t);
using ItemIsUseFn = int (*)(int32_t);
using PopupStateExistFn = int (*)();
using OpenItemBoxFn = int (*)(int32_t);
using ReleaseSealedFn = int (*)(int32_t);
using IsDiceFn = int (*)(int32_t);
using IsSealedFn = int (*)(int32_t);
using IsItemBoxFn = int (*)(int32_t);
using MakeItemFn = void* (*)(int32_t, int32_t, int32_t);
using CreateItemFn = void* (*)(int32_t, int32_t, int32_t, int32_t);
using NetworkStoreSetStateFn = void (*)(int);
