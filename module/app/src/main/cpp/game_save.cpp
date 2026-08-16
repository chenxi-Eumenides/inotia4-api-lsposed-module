// game_save.cpp —— 存档域：当前存档槽 + 存档槽列表（parse 域）
// 由 game_misc.cpp 拆分生成（纯搬代码，零逻辑变更）。

#include "game_save.h"

#include "game_access.h"
#include "game_ops_common.h"

// v0.5.5：当前加载存档槽（S5）——G_CURRENT_SLOT 双层解引用（SaveSlot_GoToNewGame/STATE_EnterGame 写，v0.5.5 frida 实测 world=0）
std::string data_current_save_slot_json() {
    if (g_base == 0) return "{\"current_save_slot\":-1}";
    uint8_t* cur_var = *reinterpret_cast<uint8_t**>(g_base + G_CURRENT_SLOT_GOT_VMA);
    int slot = (cur_var != nullptr) ? static_cast<int>(*cur_var) : -1;
    return "{\"current_save_slot\":" + std::to_string(slot) + "}";
}

std::string data_save_slots_json() {
    if (fn_save_get_save_slot == nullptr || fn_saveslot_get_hero == nullptr || fn_save_create_save_slot == nullptr)
        return op_err("symbol not resolved");
    fn_save_create_save_slot();
    std::string s = "{\"slots\":[";
    for (int i = 0; i < 3; ++i) {
        if (i > 0) s += ",";
        void* slot = fn_save_get_save_slot(i);
        uint8_t b2 = slot ? *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(slot) + 2) : 0;
        int8_t hero_idx = slot ? *reinterpret_cast<int8_t*>(reinterpret_cast<uint8_t*>(slot) + 0x1c) : -1;
        bool exists = (b2 != 0);
        s += "{\"slot\":" + std::to_string(i) + ",\"exists\":" + (exists ? "true" : "false");
        if (exists) {
            void* hero = fn_saveslot_get_hero(slot);
            int level = hero ? static_cast<int8_t>(*reinterpret_cast<int8_t*>(reinterpret_cast<uint8_t*>(hero) + C_LEVEL)) : 0;
            s += ",\"hero_level\":" + std::to_string(level) + ",\"hero_index\":" + std::to_string(hero_idx);
        }
        s += "}";
    }
    s += "]}";
    return s;
}
