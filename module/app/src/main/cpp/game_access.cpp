#include "game_access.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

uintptr_t g_base = 0;
void* g_money = nullptr;
void* g_map_id = nullptr;
void* g_party = nullptr;
void* g_active_quest = nullptr;
void* g_inven = nullptr;
void* g_main_merc_slot = nullptr;
void* g_prev_state = nullptr;
void* g_state = nullptr;
void* g_gamestate = nullptr;
void* g_initstate = nullptr;
void* g_popup_on = nullptr;
void* g_mainmenu_draw = nullptr;
void* g_popup_stack = nullptr;

GetMoneyFn fn_get_money = nullptr;
GetMemberFn fn_get_member = nullptr;
GetPartySizeFn fn_get_party_size = nullptr;
GetAttrFn fn_get_attr = nullptr;
GetEquipFn fn_get_equip = nullptr;
GetExpFn fn_get_exp = nullptr;
GetExpFn fn_get_next_exp = nullptr;
GetRarityFn fn_get_rarity = nullptr;
GetBagSizeFn fn_get_bag_size = nullptr;
GetBitFn fn_get_bit = nullptr;
GetItemStatFn fn_get_damage = nullptr;
GetItemStatFn fn_get_defense = nullptr;
GetAttrFn2 fn_get_stat = nullptr;
GetStatusPointFn fn_get_status_point = nullptr;
GetStatMainFn fn_get_stat_main = nullptr;
SetStatMainFn fn_set_stat_main = nullptr;
PutJewelFn fn_put_jewel = nullptr;
IsJewelFn fn_is_jewel = nullptr;
CharInitializeStatusFn fn_char_initialize_status = nullptr;
CharInitializeSkillFn fn_char_initialize_skill = nullptr;
CharSetActionIdFn fn_char_set_action_id = nullptr;
CharGetEnemyTargetFn fn_char_get_enemy_target = nullptr;
QuestSystemFindFn fn_questsystem_find = nullptr;
QuestSystemRemoveSlotFn fn_questsystem_remove_slot = nullptr;
SaveFn fn_save = nullptr;
SaveGetSaveSlotFn fn_save_get_save_slot = nullptr;
UiSetPopupProcessInfoFn fn_ui_set_popup_process_info = nullptr;
GameStartResumeGameFn fn_game_start_resume_game = nullptr;
SaveCreateSaveSlotFn fn_save_create_save_slot = nullptr;
SaveslotGetHeroFn fn_saveslot_get_hero = nullptr;
GamestateSetStateFn fn_gamestate_set_state = nullptr;
UinpcInitFn fn_uinpc_init = nullptr;
UinpcExeTaskFn fn_uinpc_exe_current_task = nullptr;
NpctasklistMakeDlgFn fn_npctasklist_make_dlg = nullptr;
PlayerCheckNearNpcFn fn_player_check_near_npc = nullptr;
GetSkillUsageFn fn_get_skill_usage = nullptr;
SetSkillUsageFn fn_set_skill_usage = nullptr;
GetNameFn fn_get_name = nullptr;
FindMercSlotFn fn_find_merc_slot = nullptr;
SearchPathFn fn_search_path = nullptr;

SetMoneyFn fn_set_money = nullptr;
AddMoneyFn fn_add_money = nullptr;
AddMoneyFn fn_minus_money = nullptr;
RemoveItemFn fn_remove_item = nullptr;
ItemGetPriceFn fn_item_get_price = nullptr;
ItemGetBuyPriceFn fn_item_get_buy_price = nullptr;
InvenFindSaveSlotFn fn_inven_find_save_slot = nullptr;
InvenSaveItemFn fn_inven_save_item = nullptr;
DealSystemFindSaleByIdFn fn_dealsystem_find_sale_by_id = nullptr;
InvenMoveItemFn fn_inven_move_item = nullptr;
SetExpFn fn_set_exp = nullptr;
AddExpFn fn_add_exp = nullptr;
SetStatusPointFn fn_set_status_point = nullptr;
SetAutoAttackFn fn_set_auto_attack = nullptr;
EquipItemFn fn_equip_item = nullptr;
UnequipFn fn_unequip = nullptr;
CanEquipFn fn_can_equip = nullptr;
FindEquipSlotFn fn_find_equip_slot = nullptr;
GetEquipItemFn fn_get_equip_item = nullptr;
IsSpecialNpcFn fn_is_special_npc = nullptr;
LearnActionFn fn_learn_action = nullptr;
SetActivePlayerFn fn_set_active_player = nullptr;
PartySwapFn fn_party_swap = nullptr;
SetPositionFn fn_set_position = nullptr;
ChangeMapFn fn_change_map = nullptr;

MoveAsPathFn fn_move_as_path = nullptr;
CharMoveFn fn_char_move = nullptr;
CharRemovePathFn fn_char_remove_path = nullptr;
CharSetTargetFn fn_char_set_target = nullptr;
CharStopCombatFn fn_char_stop_combat = nullptr;
ConsumeItemFn fn_consume_item = nullptr;
CharUseItemExFn fn_char_use_item_ex = nullptr;
RemoveItemDirectFn fn_remove_item_direct = nullptr;
IncludePartyFn fn_include_party = nullptr;
ExcludePartyFn fn_exclude_party = nullptr;
MercenaryReleaseFn fn_mercenary_release = nullptr;
ItemIsUseFn fn_is_use = nullptr;
PopupStateExistFn fn_popup_exist = nullptr;
NetworkStoreSetStateFn fn_networkstore_set_state = nullptr;
OpenItemBoxFn fn_open_item_box = nullptr;
ReleaseSealedFn fn_release_sealed = nullptr;
IsDiceFn fn_is_dice = nullptr;
IsSealedFn fn_is_sealed = nullptr;
IsItemBoxFn fn_is_item_box = nullptr;
MakeItemFn fn_make_item = nullptr;
CreateItemFn fn_create_item = nullptr;

std::vector<std::pair<const char*, bool>> g_symbol_report;
std::string g_dl_error;
std::string g_lib_path;

namespace {

std::mutex g_mutex;

bool find_libgame_base() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (f == nullptr) return false;
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f) != nullptr) {
        if (strstr(line, "libgame.so") != nullptr) {
            g_base = strtoul(line, nullptr, 16);
            found = true;
            char* p = strchr(line, '/');
            if (p != nullptr) {
                std::string s(p);
                s.erase(s.find_last_not_of("\n\r") + 1);
                g_lib_path = s;
            }
            break;
        }
    }
    fclose(f);
    return found;
}

void resolve_global(void*& dst, uintptr_t vma, const char* name) {
    dst = reinterpret_cast<void*>(g_base + vma);
    g_symbol_report.emplace_back(name, true);
}

}  // namespace

bool bridge_init() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_base != 0) return true;
    g_symbol_report.clear();
    if (!find_libgame_base()) {
        g_symbol_report.emplace_back("libgame_not_loaded", false);
        g_dl_error = "libgame.so not loaded yet";
        return false;
    }
    g_dl_error.clear();
    resolve_global(g_money, G_MONEY_VMA, "G_MONEY_VMA");
    resolve_global(g_map_id, G_MAP_ID_VMA, "G_MAP_ID_VMA");
    resolve_global(g_party, G_PARTY_VMA, "G_PARTY_VMA");
    resolve_global(g_active_quest, G_ACTIVE_QUEST_VMA, "G_ACTIVE_QUEST_VMA");
    g_inven = reinterpret_cast<void*>(g_base + G_INVEN_VMA);
    resolve_global(g_main_merc_slot, G_MAIN_MERC_SLOT_VMA, "G_MAIN_MERC_SLOT_VMA");
    resolve_global(g_prev_state, G_PREV_STATE_VMA, "G_PREV_STATE_VMA");
    resolve_global(g_state, G_STATE_VMA, "G_STATE_VMA");
    resolve_global(g_gamestate, G_GAMESTATE_VMA, "G_GAMESTATE_VMA");
    resolve_global(g_initstate, G_INITSTATE_VMA, "G_INITSTATE_VMA");
    resolve_global(g_popup_on, G_POPUP_ON_VMA, "G_POPUP_ON_VMA");
    resolve_global(g_mainmenu_draw, G_MAINMENU_DRAW_VMA, "G_MAINMENU_DRAW_VMA");
    resolve_global(g_popup_stack, G_POPUP_STACK_VMA, "G_POPUP_STACK_VMA");
    fn_get_money = reinterpret_cast<GetMoneyFn>(g_base + F_GET_MONEY_VMA);
    fn_get_member = reinterpret_cast<GetMemberFn>(g_base + F_GET_MEMBER_VMA);
    fn_get_party_size = reinterpret_cast<GetPartySizeFn>(g_base + F_GET_PARTY_SIZE_VMA);
    fn_get_attr = reinterpret_cast<GetAttrFn>(g_base + F_GET_ATTR_VMA);
    fn_get_equip = reinterpret_cast<GetEquipFn>(g_base + F_GET_EQUIP_VMA);
    fn_get_exp = reinterpret_cast<GetExpFn>(g_base + F_GET_EXP_VMA);
    fn_get_next_exp = reinterpret_cast<GetExpFn>(g_base + F_GET_NEXT_EXP_VMA);
    fn_get_rarity = reinterpret_cast<GetRarityFn>(g_base + F_GET_RARITY_VMA);
    fn_get_bag_size = reinterpret_cast<GetBagSizeFn>(g_base + F_GET_BAG_SIZE_VMA);
    fn_get_bit = reinterpret_cast<GetBitFn>(g_base + F_GET_BIT_VMA);
    fn_get_damage = reinterpret_cast<GetItemStatFn>(g_base + F_GET_DAMAGE_VMA);
    fn_get_defense = reinterpret_cast<GetItemStatFn>(g_base + F_GET_DEFENSE_VMA);
    fn_get_stat = reinterpret_cast<GetAttrFn2>(g_base + F_GET_STAT_VMA);
    fn_get_status_point = reinterpret_cast<GetStatusPointFn>(g_base + F_GET_STATUS_POINT_VMA);
    fn_get_stat_main = reinterpret_cast<GetStatMainFn>(g_base + F_GET_STAT_MAIN_VMA);
    fn_set_stat_main = reinterpret_cast<SetStatMainFn>(g_base + F_SET_STAT_MAIN_VMA);
    fn_put_jewel = reinterpret_cast<PutJewelFn>(g_base + F_PUT_JEWEL_VMA);
    fn_is_jewel = reinterpret_cast<IsJewelFn>(g_base + F_IS_JEWEL_VMA);
    fn_char_initialize_status = reinterpret_cast<CharInitializeStatusFn>(g_base + F_CHAR_INITIALIZE_STATUS_VMA);
    fn_char_initialize_skill = reinterpret_cast<CharInitializeSkillFn>(g_base + F_CHAR_INITIALIZE_SKILL_VMA);
    fn_char_set_action_id = reinterpret_cast<CharSetActionIdFn>(g_base + F_CHAR_SET_ACTION_ID_VMA);
    fn_char_get_enemy_target = reinterpret_cast<CharGetEnemyTargetFn>(g_base + F_CHAR_GET_ENEMY_TARGET_VMA);
    fn_questsystem_find = reinterpret_cast<QuestSystemFindFn>(g_base + F_QUESTSYSTEM_FIND_VMA);
    fn_questsystem_remove_slot = reinterpret_cast<QuestSystemRemoveSlotFn>(g_base + F_QUESTSYSTEM_REMOVE_SLOT_VMA);
    fn_save = reinterpret_cast<SaveFn>(g_base + F_SAVE_VMA);
    fn_save_get_save_slot = reinterpret_cast<SaveGetSaveSlotFn>(g_base + F_SAVE_GET_SAVE_SLOT_VMA);
    fn_ui_set_popup_process_info = reinterpret_cast<UiSetPopupProcessInfoFn>(g_base + F_UI_SET_POPUP_PROCESS_INFO_VMA);
    fn_game_start_resume_game = reinterpret_cast<GameStartResumeGameFn>(g_base + F_GAME_START_RESUME_GAME_VMA);
    fn_save_create_save_slot = reinterpret_cast<SaveCreateSaveSlotFn>(g_base + F_SAVE_CREATE_SAVE_SLOT_VMA);
    fn_saveslot_get_hero = reinterpret_cast<SaveslotGetHeroFn>(g_base + F_SAVESLOT_GET_HERO_VMA);
    fn_gamestate_set_state = reinterpret_cast<GamestateSetStateFn>(g_base + F_GAMESTATE_SET_STATE_VMA);
    fn_uinpc_init = reinterpret_cast<UinpcInitFn>(g_base + F_UINPC_INIT_VMA);
    fn_uinpc_exe_current_task = reinterpret_cast<UinpcExeTaskFn>(g_base + F_UINPC_EXE_CURRENT_TASK_VMA);
    fn_npctasklist_make_dlg = reinterpret_cast<NpctasklistMakeDlgFn>(g_base + F_NPCTASKLIST_MAKE_DLG_VMA);
    fn_player_check_near_npc = reinterpret_cast<PlayerCheckNearNpcFn>(g_base + F_PLAYER_DO_CHECK_NEAR_NPC_VMA);
    fn_get_skill_usage = reinterpret_cast<GetSkillUsageFn>(g_base + F_CHAR_GET_SKILL_USAGE_VMA);
    fn_set_skill_usage = reinterpret_cast<SetSkillUsageFn>(g_base + F_CHAR_SET_SKILL_USAGE_VMA);
    fn_get_name = reinterpret_cast<GetNameFn>(g_base + F_GET_NAME_VMA);
    fn_find_merc_slot = reinterpret_cast<FindMercSlotFn>(g_base + F_FIND_MERC_SLOT_VMA);
    fn_search_path = reinterpret_cast<SearchPathFn>(g_base + F_SEARCH_PATH_VMA);
    fn_set_money = reinterpret_cast<SetMoneyFn>(g_base + F_SET_MONEY_VMA);
    fn_add_money = reinterpret_cast<AddMoneyFn>(g_base + F_ADD_MONEY_VMA);
    fn_minus_money = reinterpret_cast<AddMoneyFn>(g_base + F_MINUS_MONEY_VMA);
    fn_remove_item = reinterpret_cast<RemoveItemFn>(g_base + F_REMOVE_ITEM_VMA);
    fn_item_get_price = reinterpret_cast<ItemGetPriceFn>(g_base + F_ITEM_GET_PRICE_VMA);
    fn_item_get_buy_price = reinterpret_cast<ItemGetBuyPriceFn>(g_base + F_ITEM_GET_BUY_PRICE_VMA);
    fn_inven_find_save_slot = reinterpret_cast<InvenFindSaveSlotFn>(g_base + F_INVEN_FIND_SAVE_SLOT_VMA);
    fn_inven_save_item = reinterpret_cast<InvenSaveItemFn>(g_base + F_INVEN_SAVE_ITEM_VMA);
    fn_dealsystem_find_sale_by_id = reinterpret_cast<DealSystemFindSaleByIdFn>(g_base + F_DEALSYSTEM_FIND_SALE_BY_ID_VMA);
    fn_inven_move_item = reinterpret_cast<InvenMoveItemFn>(g_base + F_INVEN_MOVE_ITEM_VMA);
    fn_set_exp = reinterpret_cast<SetExpFn>(g_base + F_SET_EXP_VMA);
    fn_add_exp = reinterpret_cast<AddExpFn>(g_base + F_ADD_EXP_VMA);
    fn_set_status_point = reinterpret_cast<SetStatusPointFn>(g_base + F_SET_STATUS_POINT_VMA);
    fn_set_auto_attack = reinterpret_cast<SetAutoAttackFn>(g_base + F_SET_AUTO_ATTACK_VMA);
    fn_equip_item = reinterpret_cast<EquipItemFn>(g_base + F_EQUIP_ITEM_VMA);
    fn_unequip = reinterpret_cast<UnequipFn>(g_base + F_UNEQUIP_VMA);
    fn_can_equip = reinterpret_cast<CanEquipFn>(g_base + F_CAN_EQUIP_VMA);
    fn_find_equip_slot = reinterpret_cast<FindEquipSlotFn>(g_base + F_FIND_EQUIP_SLOT_VMA);
    fn_get_equip_item = reinterpret_cast<GetEquipItemFn>(g_base + F_GET_EQUIP_ITEM_VMA);
    fn_is_special_npc = reinterpret_cast<IsSpecialNpcFn>(g_base + F_IS_SPECIAL_NPC_VMA);
    fn_learn_action = reinterpret_cast<LearnActionFn>(g_base + F_LEARN_ACTION_VMA);
    fn_set_active_player = reinterpret_cast<SetActivePlayerFn>(g_base + F_SET_ACTIVE_PLAYER_VMA);
    fn_party_swap = reinterpret_cast<PartySwapFn>(g_base + F_PARTY_SWAP_VMA);
    fn_set_position = reinterpret_cast<SetPositionFn>(g_base + F_SET_POSITION_VMA);
    fn_change_map = reinterpret_cast<ChangeMapFn>(g_base + F_CHANGE_MAP_VMA);
    fn_move_as_path = reinterpret_cast<MoveAsPathFn>(g_base + F_MOVE_AS_PATH_VMA);
    fn_char_move = reinterpret_cast<CharMoveFn>(g_base + F_CHAR_MOVE_VMA);
    fn_char_remove_path = reinterpret_cast<CharRemovePathFn>(g_base + F_CHAR_REMOVE_PATH_VMA);
    fn_char_set_target = reinterpret_cast<CharSetTargetFn>(g_base + F_CHAR_SET_TARGET_VMA);
    fn_char_stop_combat = reinterpret_cast<CharStopCombatFn>(g_base + F_CHAR_STOP_COMBAT_VMA);
    fn_consume_item = reinterpret_cast<ConsumeItemFn>(g_base + F_CONSUME_ITEM_VMA);
    fn_char_use_item_ex = reinterpret_cast<CharUseItemExFn>(g_base + F_CHAR_USE_ITEM_EX_VMA);
    fn_remove_item_direct = reinterpret_cast<RemoveItemDirectFn>(g_base + F_REMOVE_ITEM_DIRECT_VMA);
    fn_include_party = reinterpret_cast<IncludePartyFn>(g_base + F_INCLUDE_PARTY_VMA);
    fn_exclude_party = reinterpret_cast<ExcludePartyFn>(g_base + F_EXCLUDE_PARTY_VMA);
    fn_mercenary_release = reinterpret_cast<MercenaryReleaseFn>(g_base + F_MERCENARY_RELEASE_VMA);
    fn_is_use = reinterpret_cast<ItemIsUseFn>(g_base + F_ITEMDATA_IS_USE_VMA);
    fn_popup_exist = reinterpret_cast<PopupStateExistFn>(g_base + F_POPUPSTATE_EXIST_VMA);
    fn_networkstore_set_state = reinterpret_cast<NetworkStoreSetStateFn>(g_base + F_NETWORKSTORE_SET_STATE_VMA);
    fn_open_item_box = reinterpret_cast<OpenItemBoxFn>(g_base + F_OPEN_ITEM_BOX_VMA);
    fn_release_sealed = reinterpret_cast<ReleaseSealedFn>(g_base + F_RELEASE_SEALED_VMA);
    fn_is_dice = reinterpret_cast<IsDiceFn>(g_base + F_IS_DICE_VMA);
    fn_is_sealed = reinterpret_cast<IsSealedFn>(g_base + F_IS_SEALED_VMA);
    fn_is_item_box = reinterpret_cast<IsItemBoxFn>(g_base + F_IS_ITEMBOX_VMA);
    fn_make_item = reinterpret_cast<MakeItemFn>(g_base + F_MAKE_ITEM_VMA);
    fn_create_item = reinterpret_cast<CreateItemFn>(g_base + F_CREATE_ITEM_VMA);
    return true;
}
