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
extern uint32_t current_map_id();
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
extern GetAttrFn2 fn_get_stat_base;
extern GetAttrFn2 fn_get_stat_bonus;
extern GetStatusPointFn fn_get_status_point;
extern GetStatMainFn fn_get_stat_main;
extern SetStatMainFn fn_set_stat_main;
extern SetStatBaseFn fn_set_stat_base;
extern RollStatusDiceFn fn_status_dice_roll;
extern PutJewelFn fn_put_jewel;
extern IsJewelFn fn_is_jewel;
extern CharInitializeStatusFn fn_char_initialize_status;
extern CharInitializeSkillFn fn_char_initialize_skill;
extern CharSetActionIdFn fn_char_set_action_id;
extern CharGetEnemyTargetFn fn_char_get_enemy_target;
extern QuestSystemFindFn fn_questsystem_find;
extern QuestSystemRemoveSlotFn fn_questsystem_remove_slot;
extern SaveFn fn_save;
extern SaveGetSaveSlotFn fn_save_get_save_slot;
extern UiSetPopupProcessInfoFn fn_ui_set_popup_process_info;
extern GameStartResumeGameFn fn_game_start_resume_game;
extern SaveCreateSaveSlotFn fn_save_create_save_slot;
extern SaveslotGetHeroFn fn_saveslot_get_hero;
extern GamestateSetStateFn fn_gamestate_set_state;
extern UinpcInitFn fn_uinpc_init;
extern UinpcExeTaskFn fn_uinpc_exe_current_task;
extern NpctasklistMakeDlgFn fn_npctasklist_make_dlg;
extern PlayerCheckNearNpcFn fn_player_check_near_npc;
extern GetSkillUsageFn fn_get_skill_usage;
extern SetSkillUsageFn fn_set_skill_usage;
extern GetNameFn fn_get_name;
extern FindMercSlotFn fn_find_merc_slot;
extern SearchPathFn fn_search_path;
extern EvtSetStateFn fn_evt_set_state;
extern TextctrlMoveNextPageFn fn_textctrl_move_next_page;
extern KeySetCodeFn fn_key_set_code;
extern IntVoidFn fn_wipeout_button_revive;
extern IntVoidFn fn_wipeout_button_special_revive;
extern IntVoidFn fn_wipeout_button_gameover;

extern SetMoneyFn fn_set_money;
extern AddMoneyFn fn_add_money;
extern AddMoneyFn fn_minus_money;
extern RemoveItemFn fn_remove_item;
extern ItemGetPriceFn fn_item_get_price;
extern ItemGetBuyPriceFn fn_item_get_buy_price;
extern InvenFindSaveSlotFn fn_inven_find_save_slot;
extern InvenSaveItemFn fn_inven_save_item;
extern DealSystemFindSaleByIdFn fn_dealsystem_find_sale_by_id;
extern InvenMoveItemFn fn_inven_move_item;
extern SetExpFn fn_set_exp;
extern SetLevelFn fn_set_level;
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
extern MapSetFocusFn fn_map_set_focus;
extern GoMapLinkByCharFn fn_go_map_link_by_char;
extern CharSetTargetFn fn_char_set_target;
extern CharStopCombatFn fn_char_stop_combat;
extern ConsumeItemFn fn_consume_item;
extern CharUseItemExFn fn_char_use_item_ex;
extern RemoveItemDirectFn fn_remove_item_direct;
extern IncludePartyFn fn_include_party;
extern ExcludePartyFn fn_exclude_party;
extern MercenaryReleaseFn fn_mercenary_release;
extern ItemIsUseFn fn_is_use;
extern PopupStateExistFn fn_popup_exist;
extern NetworkStoreSetStateFn fn_networkstore_set_state;
extern OpenItemBoxFn fn_open_item_box;
extern ReleaseSealedFn fn_release_sealed;
extern IsDiceFn fn_is_dice;
extern IsSealedFn fn_is_sealed;
extern IsItemBoxFn fn_is_item_box;
extern MakeItemFn fn_make_item;
extern CreateItemFn fn_create_item;

extern std::vector<std::pair<const char*, bool>> g_symbol_report;
extern std::string g_dl_error;
extern std::string g_lib_path;

bool bridge_init();
