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
    return true;
}
