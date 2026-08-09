#pragma once

#include "game_symbols.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// 符号解析层：从 /proc/self/maps 定位 libgame.so 基址，计算全部符号运行时地址。
// 不用 dlopen/dlsym：Android linker namespace 隔离下 dlopen 会加载独立副本，读不到游戏数据。

extern uintptr_t g_base;
extern void* g_money;
extern void* g_map_id;
extern void* g_party;
extern void* g_active_quest;
extern void* g_inven;
extern void* g_main_merc_slot;
extern void* g_prev_state;
extern void* g_state;
extern void* g_gamestate;
extern void* g_initstate;
extern void* g_popup_on;
extern void* g_mainmenu_draw;
extern void* g_popup_stack;

extern GetMoneyFn fn_get_money;
extern GetMemberFn fn_get_member;
extern GetPartySizeFn fn_get_party_size;
extern GetAttrFn fn_get_attr;
extern GetEquipFn fn_get_equip;
extern GetExpFn fn_get_exp;
extern GetExpFn fn_get_next_exp;
extern GetRarityFn fn_get_rarity;
extern GetBagSizeFn fn_get_bag_size;
extern GetBitFn fn_get_bit;
extern GetItemStatFn fn_get_damage;
extern GetItemStatFn fn_get_defense;
extern GetAttrFn2 fn_get_stat;
extern GetStatusPointFn fn_get_status_point;
extern GetNameFn fn_get_name;
extern FindMercSlotFn fn_find_merc_slot;
extern SearchPathFn fn_search_path;

extern SetMoneyFn fn_set_money;
extern AddMoneyFn fn_add_money;
extern AddMoneyFn fn_minus_money;
extern RemoveItemFn fn_remove_item;
extern SetExpFn fn_set_exp;
extern AddExpFn fn_add_exp;
extern SetStatusPointFn fn_set_status_point;
extern SetAutoAttackFn fn_set_auto_attack;
extern EquipItemFn fn_equip_item;
extern UnequipFn fn_unequip;
extern CanEquipFn fn_can_equip;
extern FindEquipSlotFn fn_find_equip_slot;
extern GetEquipItemFn fn_get_equip_item;
extern IsSpecialNpcFn fn_is_special_npc;
extern LearnActionFn fn_learn_action;
extern SetActivePlayerFn fn_set_active_player;
extern PartySwapFn fn_party_swap;
extern SetPositionFn fn_set_position;
extern ChangeMapFn fn_change_map;

extern MoveAsPathFn fn_move_as_path;
extern CharMoveFn fn_char_move;
extern CharRemovePathFn fn_char_remove_path;
extern ConsumeItemFn fn_consume_item;
extern RemoveItemDirectFn fn_remove_item_direct;
extern IncludePartyFn fn_include_party;
extern ExcludePartyFn fn_exclude_party;
extern ItemIsUseFn fn_is_use;
extern PopupStateExistFn fn_popup_exist;

extern std::vector<std::pair<const char*, bool>> g_symbol_report;
extern std::string g_dl_error;
extern std::string g_lib_path;

bool bridge_init();
