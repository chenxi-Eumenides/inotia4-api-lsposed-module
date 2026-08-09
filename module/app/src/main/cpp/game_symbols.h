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
constexpr size_t I_COUNT = 0x10; // u32 数量位域 (bit25-31=数量)
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
constexpr uintptr_t G_CHARLOC_POOL_VMA = 0x307530;   // CHARLOCSYSTEM_pPool 位置登记池（CHARLOC_Copy 反汇编：10B/条）
constexpr uintptr_t G_CHARLOC_COUNT_VMA = 0x307528;  // CHARLOCSYSTEM_nCount (u16) 位置登记条数
constexpr uintptr_t G_PREV_STATE_VMA = 0x307490;      // STATE_nPrevState (u16) 上一个 UI 状态（readelf 符号表）
constexpr uintptr_t G_STATE_VMA = 0x307492;          // STATE_nState (u16) UI 状态机（4=主菜单流程 5=游戏中, frida 实测）
constexpr uintptr_t G_GAMESTATE_VMA = 0x72b068;      // GAMESTATE_nState (u32) 游戏状态
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
constexpr uintptr_t G_MERC_MAX_VMA = 0x2f3978;       // 佣兵槽上限 (s8)
constexpr uintptr_t G_TILE_GOT_VMA = 0x2f3f48;       // MAP 通行矩阵 GOT（双层解引用 *(*(base+0x2f3f48))，MAP_IsBlocking 反汇编确认；frida 实测与 MAP_nBaseTile 0x7148a8 非同一数据——0x7148a8 为渲染基础瓦片）
constexpr size_t TILE_ROW_STRIDE = 64;               // 瓦片行字节步长（MAP_IsBlocking 中 y*64+x 索引）
constexpr uint8_t TILE_BLOCK_BIT = 0x08;             // 阻挡标志位（ubfx bit3）

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
constexpr uintptr_t F_GET_STAT_VMA = 0xdf8d0;        // int (void*, int) 主属性 (0=力量 1=敏捷 2=体力 3=智力 4=精力)
constexpr uintptr_t F_GET_STATUS_POINT_VMA = 0xd9c44; // int (void*) 剩余能力点
constexpr uintptr_t F_GET_STAT_MAIN_VMA = 0xdb9f0;    // int (void*, int) 读主属性 [ch+0x256+i*2]（i=0-4 力量/敏捷/体力/智力/精力）
constexpr uintptr_t F_SET_STAT_MAIN_VMA = 0xdf1c4;    // void (void*, int, int) 写主属性 + CHAR_ResetAttrFromStat 重算衍生
constexpr uintptr_t F_PUT_JEWEL_VMA = 0x10bcb4;       // int (void*, void*) 镶嵌宝石（equipItem+jewelItem）；返回 0=成功/2=无孔/3=非宝石或空装备
constexpr uintptr_t F_IS_JEWEL_VMA = 0x10b964;        // int (int32_t) 类别是否为宝石
constexpr uintptr_t F_CHAR_INITIALIZE_STATUS_VMA = 0xe68c8;  // void (void*) 属性重置：5 项主属性归 0 + 能力点按 (等级-1)×职业基础值 还原
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
constexpr uintptr_t F_CHAR_SET_TARGET_VMA = 0xdc754;    // void (void*, void*) 设置攻击目标（写 [ch+0x278]）
constexpr uintptr_t F_CHAR_MAKE_DEFAULT_ATTACK_VMA = 0xe2730; // int (void*) 置普攻动作（actionId=5）
constexpr uintptr_t F_CHAR_STOP_COMBAT_VMA = 0xe7c24;   // void (void*) 停止战斗（清战斗标志+移除仇恨+动作复位）
constexpr uintptr_t F_CONSUME_ITEM_VMA = 0x1047bc;     // void (void*) 消耗 1 个（使用药水/卷轴）
constexpr uintptr_t F_REMOVE_ITEM_DIRECT_VMA = 0x103fd8; // int (int32 bag, int32 slot) 按槽删物品
constexpr uintptr_t F_INCLUDE_PARTY_VMA = 0x118e04;    // int (void*) 佣兵入队（内部校验）
constexpr uintptr_t F_EXCLUDE_PARTY_VMA = 0x118d0c;    // int (void*) 佣兵离队
constexpr uintptr_t F_MERCENARY_RELEASE_VMA = 0x118ab4; // void (int) 佣兵遣散：FindAsMercenarySlot→清 +0x352/-0x3cc 标志→MERCENARYSLOT_Initialize→GAMESTATE_SetState
constexpr uintptr_t F_ITEMDATA_IS_USE_VMA = 0x1058ac;  // int (int32 itemId) 物品是否可使用（ITEMDATABASE_IsUse，读表 +2 u8 ∈ {0x16,0x17} 可消耗）
constexpr uintptr_t F_POPUPSTATE_EXIST_VMA = 0x1223f8; // int () 弹窗栈是否有激活状态（readelf 符号表）
constexpr uintptr_t F_BUTTON_OK_EXE_VMA = 0xca9d8;      // void () 弹窗确定按钮执行（bOn=0 + 调 fpOK(param)；无参直接调用，v0.3.11 frida 验证）
constexpr uintptr_t F_BUTTON_CANCEL_EXE_VMA = 0xcaa78;  // void () 弹窗取消按钮执行（bOn=0 + 调 fpCancel(param) 或 Free；无参直接调用）

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
using AddExpFn = int (*)(void*, int32_t, uint8_t);
using SetStatusPointFn = void (*)(void*, int32_t);
using GetStatMainFn = int (*)(void*, int32_t);
using SetStatMainFn = void (*)(void*, int32_t, int32_t);
using PutJewelFn = int (*)(void*, void*);
using IsJewelFn = int (*)(int32_t);
using CharInitializeStatusFn = void (*)(void*);
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
using CharSetTargetFn = void (*)(void*, void*);
using CharMakeDefaultAttackFn = int (*)(void*);
using CharStopCombatFn = void (*)(void*);
using ConsumeItemFn = void (*)(void*);
using RemoveItemDirectFn = int (*)(int32_t, int32_t);
using IncludePartyFn = int (*)(void*);
using ExcludePartyFn = int (*)(void*);
using MercenaryReleaseFn = void (*)(int32_t);
using ItemIsUseFn = int (*)(int32_t);
using PopupStateExistFn = int (*)();
