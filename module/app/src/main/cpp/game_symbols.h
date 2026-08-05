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
constexpr size_t C_EQUIP_SLOTS = 10;
constexpr size_t C_POS_X = 0x02;     // int16 实时 X（CHAR_GetDistance 反汇编证实）
constexpr size_t C_POS_Y = 0x04;     // int16 实时 Y
constexpr size_t C_OBJ_SIZE = 0x430; // 角色对象步长（CHARSYSTEM 池相邻对象间隔, frida 实测）

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
constexpr uintptr_t G_STATE_VMA = 0x307492;          // STATE_nState (u16) UI 状态机（4=主菜单流程 5=游戏中, frida 实测）
constexpr uintptr_t G_GAMESTATE_VMA = 0x72b068;      // GAMESTATE_nState (u32) 游戏状态
constexpr uintptr_t G_INITSTATE_VMA = 0x72b06d;      // INITSTATE_nState (u8) 初始化状态
constexpr uintptr_t G_MERC_SLOTLIST_GOT_VMA = 0x2f6010; // 佣兵槽数组指针（需双层解引用 *(*(base+0x2f6010))，20B/槽）
constexpr uintptr_t G_MERC_MAX_VMA = 0x2f3978;       // 佣兵槽上限 (s8)

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
constexpr uintptr_t F_GET_NAME_VMA = 0xd9c54;         // char* (void*) 角色名称（UTF-8 字符串）
constexpr uintptr_t F_FIND_MERC_SLOT_VMA = 0xf4254;   // void* (int) 按佣兵槽找角色（CHARSYSTEM_FindAsMercenarySlot）
constexpr uintptr_t F_SEARCH_PATH_VMA = 0xdb094;      // int (void*, int, int, int) 角色寻路（CHAR_SearchPath：目标像素+flag）

// ---- 写操作函数 VMA（2026-08-05 objdump 逆向确认，见 docs/notes/control-capability.md §5）----
constexpr uintptr_t F_SET_MONEY_VMA = 0x10449c;        // void (int64) 设金币
constexpr uintptr_t F_ADD_MONEY_VMA = 0x1044e4;        // int (int64) 加金币（溢出返回 0）
constexpr uintptr_t F_MINUS_MONEY_VMA = 0x104780;      // int (int64) 减金币（不足返回 0）
constexpr uintptr_t F_REMOVE_ITEM_VMA = 0x104044;      // int (int32) 按类别删物品
constexpr uintptr_t F_SET_EXP_VMA = 0xd9b5c;           // void (void*, int32) 设经验
constexpr uintptr_t F_ADD_EXP_VMA = 0xe7028;           // int (void*, int32, u8) 加经验（走升级判定链）
constexpr uintptr_t F_SET_STATUS_POINT_VMA = 0xd9c4c;  // void (void*, int32) 设能力点（写 +0x32a）
constexpr uintptr_t F_SET_AUTO_ATTACK_VMA = 0xe4cf4;   // void (void*, int32) 自动攻击开关
constexpr uintptr_t F_EQUIP_ITEM_VMA = 0xe51c0;        // int (void*, void*) 穿装备（自动找槽）
constexpr uintptr_t F_UNEQUIP_VMA = 0xe2f68;           // int (void*, int32) 脱装备槽→背包
constexpr uintptr_t F_CAN_EQUIP_VMA = 0xe4eb4;         // int (void*, void*) 可否装备
constexpr uintptr_t F_LEARN_ACTION_VMA = 0xe2390;      // void* (void*, int32, int32) 学习/升级技能
constexpr uintptr_t F_SET_ACTIVE_PLAYER_VMA = 0x11f584; // int (int32) 切换主控角色
constexpr uintptr_t F_PARTY_SWAP_VMA = 0x11ff5c;       // void (int32, int32) 交换队伍槽
constexpr uintptr_t F_SET_POSITION_VMA = 0x12aa14;     // void (int32, int32) 全队传送（写 +0x2/+0x4）
constexpr uintptr_t F_CHANGE_MAP_VMA = 0x114fc4;       // void (int32, int32, int32, int32) 切图（mapId,x,y,dir）

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
using RemoveItemFn = int (*)(int32_t);
using SetExpFn = void (*)(void*, int32_t);
using AddExpFn = int (*)(void*, int32_t, uint8_t);
using SetStatusPointFn = void (*)(void*, int32_t);
using SetAutoAttackFn = void (*)(void*, int32_t);
using EquipItemFn = int (*)(void*, void*);
using UnequipFn = int (*)(void*, int32_t);
using CanEquipFn = int (*)(void*, void*);
using LearnActionFn = void* (*)(void*, int32_t, int32_t);
using SetActivePlayerFn = int (*)(int32_t);
using PartySwapFn = void (*)(int32_t, int32_t);
using SetPositionFn = void (*)(int32_t, int32_t);
using ChangeMapFn = void (*)(int32_t, int32_t, int32_t, int32_t);
