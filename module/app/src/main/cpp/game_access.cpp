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
void* g_state = nullptr;
void* g_gamestate = nullptr;
void* g_initstate = nullptr;

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
GetNameFn fn_get_name = nullptr;
FindMercSlotFn fn_find_merc_slot = nullptr;
SearchPathFn fn_search_path = nullptr;

SetMoneyFn fn_set_money = nullptr;
AddMoneyFn fn_add_money = nullptr;
AddMoneyFn fn_minus_money = nullptr;
RemoveItemFn fn_remove_item = nullptr;
SetExpFn fn_set_exp = nullptr;
AddExpFn fn_add_exp = nullptr;
SetStatusPointFn fn_set_status_point = nullptr;
SetAutoAttackFn fn_set_auto_attack = nullptr;
EquipItemFn fn_equip_item = nullptr;
UnequipFn fn_unequip = nullptr;
CanEquipFn fn_can_equip = nullptr;
LearnActionFn fn_learn_action = nullptr;
SetActivePlayerFn fn_set_active_player = nullptr;
PartySwapFn fn_party_swap = nullptr;
SetPositionFn fn_set_position = nullptr;
ChangeMapFn fn_change_map = nullptr;

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
    resolve_global(g_state, G_STATE_VMA, "G_STATE_VMA");
    resolve_global(g_gamestate, G_GAMESTATE_VMA, "G_GAMESTATE_VMA");
    resolve_global(g_initstate, G_INITSTATE_VMA, "G_INITSTATE_VMA");
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
    fn_get_name = reinterpret_cast<GetNameFn>(g_base + F_GET_NAME_VMA);
    fn_find_merc_slot = reinterpret_cast<FindMercSlotFn>(g_base + F_FIND_MERC_SLOT_VMA);
    fn_search_path = reinterpret_cast<SearchPathFn>(g_base + F_SEARCH_PATH_VMA);
    fn_set_money = reinterpret_cast<SetMoneyFn>(g_base + F_SET_MONEY_VMA);
    fn_add_money = reinterpret_cast<AddMoneyFn>(g_base + F_ADD_MONEY_VMA);
    fn_minus_money = reinterpret_cast<AddMoneyFn>(g_base + F_MINUS_MONEY_VMA);
    fn_remove_item = reinterpret_cast<RemoveItemFn>(g_base + F_REMOVE_ITEM_VMA);
    fn_set_exp = reinterpret_cast<SetExpFn>(g_base + F_SET_EXP_VMA);
    fn_add_exp = reinterpret_cast<AddExpFn>(g_base + F_ADD_EXP_VMA);
    fn_set_status_point = reinterpret_cast<SetStatusPointFn>(g_base + F_SET_STATUS_POINT_VMA);
    fn_set_auto_attack = reinterpret_cast<SetAutoAttackFn>(g_base + F_SET_AUTO_ATTACK_VMA);
    fn_equip_item = reinterpret_cast<EquipItemFn>(g_base + F_EQUIP_ITEM_VMA);
    fn_unequip = reinterpret_cast<UnequipFn>(g_base + F_UNEQUIP_VMA);
    fn_can_equip = reinterpret_cast<CanEquipFn>(g_base + F_CAN_EQUIP_VMA);
    fn_learn_action = reinterpret_cast<LearnActionFn>(g_base + F_LEARN_ACTION_VMA);
    fn_set_active_player = reinterpret_cast<SetActivePlayerFn>(g_base + F_SET_ACTIVE_PLAYER_VMA);
    fn_party_swap = reinterpret_cast<PartySwapFn>(g_base + F_PARTY_SWAP_VMA);
    fn_set_position = reinterpret_cast<SetPositionFn>(g_base + F_SET_POSITION_VMA);
    fn_change_map = reinterpret_cast<ChangeMapFn>(g_base + F_CHANGE_MAP_VMA);
    return true;
}
