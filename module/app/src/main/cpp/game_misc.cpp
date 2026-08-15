// game_misc.cpp —— 由 game_data.cpp 拆分生成（纯搬代码，零逻辑变更）

#include "game_data.h"

#include "game_access.h"
#include "game_symbols.h"

#include <android/log.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "game_misc.h"
#include "game_nav.h"
#include "game_state.h"
#include "game_json.h"
#include "game_read.h"

// 事件流基线（审计 H4 修复：diff 全程锁保护）
std::mutex g_events_mtx;
bool g_events_has_last = false;
Snapshot g_events_last;


std::string data_debug_ui_json() {
    char buf[4096];
    uint16_t state = g_state ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    uint16_t prev = g_prev_state ? *reinterpret_cast<uint16_t*>(g_prev_state) : 0xFFFF;
    uint32_t gs = g_gamestate ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0xFFFFFFFF;
    uint8_t init = g_initstate ? *reinterpret_cast<uint8_t*>(g_initstate) : 0xFF;
    uint8_t popup_on = g_popup_on ? *reinterpret_cast<uint8_t*>(g_popup_on) : 0xFF;
    uint8_t menu_draw = g_mainmenu_draw ? *reinterpret_cast<uint8_t*>(g_mainmenu_draw) : 0xFF;

    snprintf(buf, sizeof(buf),
        "{"
        "\"state\":%u,\"prev_state\":%u,\"gamestate\":%u,\"initstate\":%u,"
        "\"popup_on\":%u,\"menu_draw\":%u,"
        "\"popup_stack\":[",
        state, prev, gs, init, popup_on, menu_draw);

    std::string s = buf;
    if (g_popup_stack) {
        uint8_t* stk = reinterpret_cast<uint8_t*>(g_popup_stack);
        for (int i = 0; i < 32; i += 4) {
            if (i > 0) s += ",";
            s += std::to_string(*reinterpret_cast<uint32_t*>(stk + i));
        }
    } else {
        s += "null";
    }
    s += "],\"popup_stack_hex\":\"";
    if (g_popup_stack) {
        uint8_t* stk = reinterpret_cast<uint8_t*>(g_popup_stack);
        for (int i = 0; i < 32; ++i) {
            snprintf(buf, sizeof(buf), "%02x", stk[i]);
            s += buf;
        }
    }
    s += "\"";

    if (g_base != 0) {
        int32_t i32type = *reinterpret_cast<int32_t*>(g_base + G_POPUP_TYPE_VMA);
        int32_t i32disp  = *reinterpret_cast<int32_t*>(g_base + G_POPUP_DISPTYPE_VMA);
        s += ",\"popup_type\":" + std::to_string(i32type);
        s += ",\"popup_disp_type\":" + std::to_string(i32disp);

        auto r8 = [&](uintptr_t vma, const char* name) {
            s += ",\"" + std::string(name) + "\":" +
                 std::to_string(static_cast<int>(*reinterpret_cast<int8_t*>(g_base + vma)));
        };
        auto ru8 = [&](uintptr_t vma, const char* name) {
            s += ",\"" + std::string(name) + "\":" +
                 std::to_string(static_cast<int>(*reinterpret_cast<uint8_t*>(g_base + vma)));
        };
        auto ru16 = [&](uintptr_t vma, const char* name) {
            s += ",\"" + std::string(name) + "\":" +
                 std::to_string(*reinterpret_cast<uint16_t*>(g_base + vma));
        };
        auto ru32 = [&](uintptr_t vma, const char* name) {
            s += ",\"" + std::string(name) + "\":" +
                 std::to_string(*reinterpret_cast<uint32_t*>(g_base + vma));
        };

        r8(G_UI_PARTY_MENU_INDEX_VMA, "partyMenuIndex");
        ru8(G_UI_QUEST_MENU_STATE_VMA, "questMenuState");
        ru8(G_UI_STORE_BUY_TYPE_VMA, "storeBuyType");
        ru8(G_UI_STORE_SEL_CLASS_VMA, "storeSelectedClass");
        ru8(G_UI_HELP_STATE_VMA, "helpState");
        ru8(G_UI_MMENU_SEL_CLASS_VMA, "mainmenuSelectedClass");
        ru8(G_UI_MMENU_SAVE_SLOT_VMA, "mainmenuSaveSlotType");
        ru8(G_UICHOICE_FOCUS_VMA, "choiceFocusIndex");
        ru8(G_UI_SHORTCUT_PAGE_VMA, "shortcutPage");
        ru16(G_UI_QUEST_MENU_MAIN_SIZE_VMA, "questMenuMainListSize");
        ru16(G_UI_QUEST_MENU_SUB_SIZE_VMA, "questMenuSubListSize");
        ru32(G_POPUP_FPCANCEL_VMA, "popupFpCancelLo");
    }

    s += "}";
    return s;
}


int64_t data_frame_count() {
    if (g_base == 0) return -1;
    // 帧计数 GOT 槽（G_FRAME_COUNT_VMA）：先解引用取 u64 指针，再读计数
    uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
    uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
    return cnt != nullptr ? static_cast<int64_t>(*cnt) : -1;
}


bool data_story_active() {
    if (g_base == 0) return false;
    uint32_t gs = g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
    return gs == 1;
}


uintptr_t data_popup_top_vma() {
    if (g_popup_stack == nullptr || g_base == 0) return 0;
    uint8_t* stk = reinterpret_cast<uint8_t*>(g_popup_stack);
    uint32_t count = *reinterpret_cast<uint32_t*>(stk + 8);
    if (count == 0 || count > 27) return 0;
    uint64_t data = *reinterpret_cast<uint64_t*>(stk + 0x18);
    if (data == 0) return 0;
    uint8_t* top = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data)) + (count - 1) * 0x40;
    uintptr_t enter = *reinterpret_cast<uintptr_t*>(top + 0x10);
    return enter > g_base ? enter - g_base : 0;
}

const char* data_top_panel_name() {
    uintptr_t vma = data_popup_top_vma();
    switch (vma) {
        case F_PANEL_CHARACTER_INFO_ENTER: return "character_info";
        case F_PANEL_CHOICE_ENTER: return "choice";
        case F_PANEL_INVENTORY_ENTER: return "inventory";
        case F_PANEL_INPUT_COUNT_ENTER: return "input_count";
        case F_PANEL_MERCENARY_ENTER: return "mercenary";
        case F_PANEL_CRAFT_ENTER: return "craft";
        case F_PANEL_NPC_ENTER: return "npc";
        case F_PANEL_NPC_QUEST_ENTER: return "npc_quest";
        case F_PANEL_NPC_REST_ENTER: return "npc_rest";
        case F_PANEL_NPC_REVIVE_ENTER: return "npc_revive";
        case F_PANEL_OPTIONS_ENTER: return "options";
        case F_PANEL_QUESTS_ENTER: return "quests";
        case F_PANEL_SAVE_SLOT_ENTER: return "save_slot";
        case F_PANEL_CHAR_SELECT_ENTER: return "character_select";
        case F_PANEL_SHORTCUT_ENTER: return "shortcut";
        case F_PANEL_SKILLS_ENTER: return "skills";
        case F_PANEL_SHOP_ENTER: return "shop";
        case F_PANEL_SETTINGS_ENTER: return "settings";
        case F_PANEL_WIPEOUT_ENTER: return "wipeout";
        case F_PANEL_WORLD_MAP_ENTER: return "world_map";
        case F_PANEL_IN_APP_ENTER:
        case F_PANEL_UNK1_ENTER:
        case F_PANEL_UNK2_ENTER:
        case F_PANEL_UNK3_ENTER:
        case F_PANEL_UNK4_ENTER:
        case F_PANEL_UNK5_ENTER: return "in_app";
        case F_PANEL_DAILY_REWARD_ENTER: return "daily_reward";
        default: return nullptr;
    }
}


std::string data_story_json() {
    if (g_base == 0) return "{\"active\":false}";
    std::string out = "{\"active\":true";
    void* teller = *reinterpret_cast<void**>(g_base + G_EVT_PTELLER_VMA);
    if (teller != nullptr && fn_get_name != nullptr) {
        char* name = fn_get_name(teller);
        if (name != nullptr) out += ",\"speaker\":\"" + json_escape(name) + "\"";
    }
    uint8_t* pt = *reinterpret_cast<uint8_t**>(g_base + G_EVT_PTEXT_VMA);
    if (pt != nullptr) {
        std::string text;
        for (int i = 0; i < 2048 && pt[i] != 0; ++i) text += static_cast<char>(pt[i]);
        out += ",\"text\":\"" + json_escape(text.c_str()) + "\"";
    }
    uint32_t idx = *reinterpret_cast<uint32_t*>(g_base + G_EVT_INDEX_VMA);
    uint32_t cnt = *reinterpret_cast<uint32_t*>(g_base + G_EVT_DATA_COUNT_VMA);
    out += ",\"index\":" + std::to_string(idx) + ",\"count\":" + std::to_string(cnt);
    out += "}";
    return out;
}


std::string data_path_json(int tx, int ty) {
    // v0.4.29 自研 BFS（替代 CHAR_SearchPath：游戏寻路不能绕远路）
    // 返回：inMap/found/distance/path（tile 中心像素）/nearest（不可达时最近可达点）
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    void* hero = lead_member();
    if (hero == nullptr) return "{\"error\":\"no player\"}";
    int px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y);
    std::string s = "{\"target\":{\"x\":" + std::to_string(tx) + ",\"y\":" + std::to_string(ty) + "}";
    s += ",\"start\":{\"x\":" + std::to_string(px) + ",\"y\":" + std::to_string(py) + "}";
    s += ",\"in_map\":" + std::string((tx >= 0 && tx < NAV_W * 16 && ty >= 0 && ty < NAV_H * 16) ? "true" : "false");
    NavPath np;
    if (!nav_bfs(px >> 4, py >> 4, tx >> 4, ty >> 4, np) || np.dir_count == 0) {
        s += ",\"found\":false,\"distance\":-1,\"nearest\":null,\"path\":[]}";
        return s;
    }
    s += ",\"found\":" + std::string(np.found ? "true" : "false");
    s += ",\"distance\":" + std::to_string(np.distance);
    if (np.nearest_x >= 0) {
        s += ",\"nearest\":{\"x\":" + std::to_string(np.nearest_x * 16 + 8) +
             ",\"y\":" + std::to_string(np.nearest_y * 16 + 8) +
             ",\"distance\":" + std::to_string(np.nearest_dist) + "}";
    } else {
        s += ",\"nearest\":null";
    }
    s += ",\"path\":[";
    int cx = px >> 4, cy = py >> 4;
    bool first = true;
    for (int i = 0; i < np.dir_count; ++i) {
        cx += NAV_DX[np.dirs[i]];
        cy += NAV_DY[np.dirs[i]];
        if (!first) s += ",";
        s += "{\"x\":" + std::to_string(cx * 16 + 8) + ",\"y\":" + std::to_string(cy * 16 + 8) + "}";
        first = false;
    }
    s += "]}";
    return s;
}


std::string data_distance_json(int32_t tx, int32_t ty) {
    // v0.4.29 玩家到目标的 BFS 最短距离（tile 步数）+ 可达性 + 最近可达点
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    void* hero = lead_member();
    if (hero == nullptr) return "{\"error\":\"no player\"}";
    int px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y);
    NavPath np;
    std::string s = "{\"target\":{\"x\":" + std::to_string(tx) + ",\"y\":" + std::to_string(ty) + "}";
    s += ",\"start\":{\"x\":" + std::to_string(px) + ",\"y\":" + std::to_string(py) + "}";
    if (!nav_bfs(px >> 4, py >> 4, tx >> 4, ty >> 4, np)) {
        s += ",\"found\":false,\"distance\":-1,\"nearest\":null}";
        return s;
    }
    s += ",\"found\":" + std::string(np.found ? "true" : "false");
    s += ",\"distance\":" + std::to_string(np.distance);
    if (np.nearest_x >= 0) {
        s += ",\"nearest\":{\"x\":" + std::to_string(np.nearest_x * 16 + 8) +
             ",\"y\":" + std::to_string(np.nearest_y * 16 + 8) +
             ",\"distance\":" + std::to_string(np.nearest_dist) + "}";
    } else {
        s += ",\"nearest\":null";
    }
    s += "}";
    return s;
}


std::string data_debug_path_json(int32_t tx, int32_t ty) {
    // GET /api/debug/path?tx=&ty=（tile 坐标）：debug 端点，直接调 nav_bfs 返回完整路线 + 阻挡信息。
    // 用途（P0 导航问题排查）：对比模块 BFS 判定 vs 引擎 CHAR_Move 实际碰撞、
    // 尸体阻挡影响（unit_blocks 含 hp=0 尸体）、重规划 resume 格（nearest）。
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    void* hero = lead_member();
    if (hero == nullptr) return "{\"error\":\"no player\"}";
    int px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y);
    int sx = px >> 4, sy = py >> 4;

    std::string s = "{\"start\":{\"tx\":" + std::to_string(sx) + ",\"ty\":" + std::to_string(sy) +
                    ",\"x\":" + std::to_string(px) + ",\"y\":" + std::to_string(py) + "}";
    s += ",\"target\":{\"tx\":" + std::to_string(tx) + ",\"ty\":" + std::to_string(ty) +
         ",\"x\":" + std::to_string(tx * 16 + 8) + ",\"y\":" + std::to_string(ty * 16 + 8) + "}";

    NavPath np;
    if (nav_bfs(sx, sy, tx, ty, np) && np.dir_count > 0) {
        s += ",\"found\":" + std::string(np.found ? "true" : "false");
        s += ",\"distance\":" + std::to_string(np.distance);
        s += ",\"path\":[";
        int cx = sx, cy = sy;
        bool first = true;
        for (int i = 0; i < np.dir_count; ++i) {
            cx += NAV_DX[np.dirs[i]];
            cy += NAV_DY[np.dirs[i]];
            if (!first) s += ",";
            s += "{\"tx\":" + std::to_string(cx) + ",\"ty\":" + std::to_string(cy) +
                 ",\"dir\":" + std::to_string(static_cast<int>(np.dirs[i])) + "}";
            first = false;
        }
        s += "]";
        if (np.nearest_x >= 0) {
            s += ",\"nearest\":{\"tx\":" + std::to_string(np.nearest_x) + ",\"ty\":" + std::to_string(np.nearest_y) +
                 ",\"distance\":" + std::to_string(np.nearest_dist) + "}";
        } else {
            s += ",\"nearest\":null";
        }
    } else {
        s += ",\"found\":false,\"distance\":-1,\"path\":[],\"nearest\":null";
    }

    // 单位阻挡列表（复用 nav_unit_blocks 过滤逻辑，含 hp 便于识别 hp=0 尸体阻挡）
    s += ",\"unit_blocks\":[";
    {
        bool first = true;
        if (g_base != 0) {
            uint8_t* pool = *reinterpret_cast<uint8_t**>(g_base + G_CHAR_POOL_VMA);
            if (pool != nullptr) {
                for (int i = 0; i < C_CHARSYSTEM_POOL_SLOTS; ++i) {
                    uint8_t* obj = pool + i * C_OBJ_SIZE;
                    int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
                    int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
                    int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
                    uint8_t status = obj[C_STATUS];
                    if (obj[C_SITUATION] != 1) continue;
                    if (type < 0 || type > 2) continue;
                    if (status > 2) continue;
                    if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) continue;
                    if (obj == hero) continue;
                    int ux = x >> 4, uy = y >> 4;
                    if (ux < 0 || ux >= NAV_W || uy < 0 || uy >= NAV_H) continue;
                    if (!first) s += ",";
                    s += "{\"tx\":" + std::to_string(ux) + ",\"ty\":" + std::to_string(uy) +
                         ",\"slot\":" + std::to_string(i) +
                         ",\"type\":" + std::to_string(type) +
                          ",\"hp\":" + std::to_string(*reinterpret_cast<int32_t*>(obj + C_HP)) +
                          ",\"situation\":" + std::to_string(static_cast<int>(obj[C_SITUATION])) + "}";
                    first = false;
                }
            }
        }
    }
    s += "]";

    // 静态阻挡统计（全量 4096 tile 不输出，仅总数供参考）
    const uint8_t* tiles = nav_tiles();
    if (tiles != nullptr) {
        int blocked = 0;
        for (int i = 0; i < NAV_W * NAV_H; ++i) {
            if (nav_blocked(tiles, i % NAV_W, i / NAV_W)) ++blocked;
        }
        s += ",\"static_block_count\":" + std::to_string(blocked);
    }

    s += "}";
    return s;
}


int data_active_quest() {
    if (g_active_quest == nullptr) return -1;
    return *reinterpret_cast<uint16_t*>(g_active_quest);
}


std::string data_quest_list_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{\"quests\":[";
    if (g_base != 0) {
        uint8_t* cnt_ptr = *reinterpret_cast<uint8_t**>(g_base + G_QUEST_SLOT_COUNT_VMA);
        uint8_t* slots_ptr = *reinterpret_cast<uint8_t**>(g_base + G_QUEST_SLOTS_GOT_VMA);
        uint8_t*** st_got = reinterpret_cast<uint8_t***>(g_base + G_NPC_QUEST_STATE_GOT_VMA);
        uint8_t* states = (st_got != nullptr && *st_got != nullptr) ? **st_got : nullptr;
        if (cnt_ptr != nullptr && slots_ptr != nullptr) {
            uint8_t cnt = *cnt_ptr;
            uint8_t* slots = *reinterpret_cast<uint8_t**>(slots_ptr);
            if (slots != nullptr && cnt > 0 && cnt <= 20) {
                bool first = true;
                for (int i = 0; i < cnt; ++i) {
                    uint16_t qid = *reinterpret_cast<uint16_t*>(slots + i * 0xC);
                    if (!first) s += ",";
                    first = false;
                    int state = (states != nullptr) ? static_cast<int>(states[qid]) : -1;
                    bool deliv = (state == 2);
                    s += "{\"slot\":" + std::to_string(i) + ",\"quest_id\":" + std::to_string(qid) +
                         ",\"deliverable\":" + (deliv ? "true" : "false") + "}";
                }
            }
        }
    }
    s += "]}";
    return s;
}

// v0.5.4：已完成任务列表（Q3）——遍历 G_NPC_QUEST_STATE 状态表过滤 state==3
std::string data_quest_completed_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{\"quests\":[";
    if (g_base != 0) {
        uint16_t* qcnt_var = *reinterpret_cast<uint16_t**>(g_base + G_QUEST_COUNT_GOT_VMA);
        uint8_t*** st_got = reinterpret_cast<uint8_t***>(g_base + G_NPC_QUEST_STATE_GOT_VMA);
        if (qcnt_var != nullptr && st_got != nullptr && *st_got != nullptr && **st_got != nullptr) {
            uint16_t qcnt = *qcnt_var;
            uint8_t* states = **st_got;
            bool first = true;
            if (qcnt > 0 && qcnt <= 512) {
                for (uint16_t qid = 0; qid < qcnt; ++qid) {
                    if (states[qid] != 3) continue;
                    if (!first) s += ",";
                    first = false;
                    s += "{\"quest_id\":" + std::to_string(qid) + "}";
                }
            }
        }
    }
    s += "]}";
    return s;
}

    // v0.5.5：已接任务列表（Q2）——槽数组（QUESTSYSTEM_Find 0x12292c 遍历，12B/槽 +0 questId，G_QUEST_SLOTS_GOT_VMA）
// + G_NPC_QUEST_STATE 状态表（0 未接/1 进行/2 可完成/3 已完成）
std::string data_quest_active_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{\"quests\":[";
    if (g_base != 0) {
        uint8_t* cnt_ptr = *reinterpret_cast<uint8_t**>(g_base + G_QUEST_SLOT_COUNT_VMA);
        uint8_t* slots_ptr = *reinterpret_cast<uint8_t**>(g_base + G_QUEST_SLOTS_GOT_VMA);
        uint8_t*** st_got = reinterpret_cast<uint8_t***>(g_base + G_NPC_QUEST_STATE_GOT_VMA);
        uint8_t* states = (st_got != nullptr && *st_got != nullptr) ? **st_got : nullptr;
        if (cnt_ptr != nullptr && slots_ptr != nullptr && cnt_ptr != slots_ptr) {
            uint8_t cnt = *cnt_ptr;
            uint8_t* slots = *reinterpret_cast<uint8_t**>(slots_ptr);
            if (slots != nullptr && cnt > 0 && cnt <= 20) {
                bool first = true;
                for (int i = 0; i < cnt; ++i) {
                    uint16_t qid = *reinterpret_cast<uint16_t*>(slots + i * 0xC);
                    if (!first) s += ",";
                    first = false;
                    int state = (states != nullptr) ? static_cast<int>(states[qid]) : -1;
                    bool deliv = (state == 2);
                    s += "{\"slot\":" + std::to_string(i) + ",\"quest_id\":" + std::to_string(qid) +
                         ",\"deliverable\":" + (deliv ? "true" : "false") + "}";
                }
            }
        }
    }
    s += "]}";
    return s;
}

// v0.5.5：当前加载存档槽（S5）——G_CURRENT_SLOT 双层解引用（SaveSlot_GoToNewGame/STATE_EnterGame 写，v0.5.5 frida 实测 world=0）
std::string data_current_save_slot_json() {
    if (g_base == 0) return "{\"current_save_slot\":-1}";
    uint8_t* cur_var = *reinterpret_cast<uint8_t**>(g_base + G_CURRENT_SLOT_GOT_VMA);
    int slot = (cur_var != nullptr) ? static_cast<int>(*cur_var) : -1;
    return "{\"current_save_slot\":" + std::to_string(slot) + "}";
}



std::string data_init_report() {
    std::string s = "{";
    bool first = true;
    for (const auto& item : g_symbol_report) {
        if (!first) s += ",";
        s += "\"" + std::string(item.first) + "\":" + (item.second ? "true" : "false");
        first = false;
    }
    if (!g_dl_error.empty()) {
        s += ",\"error\":\"" + g_dl_error + "\"";
    }
    if (!g_lib_path.empty()) {
        s += ",\"path\":\"" + g_lib_path + "\"";
    }
    s += "}";
    return s;
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


std::string data_npc_dialog_options_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string out = "{\"count\":" +
        std::to_string(static_cast<int>(*reinterpret_cast<uint8_t*>(g_base + G_UICHOICE_COUNT_VMA)));
    out += ",\"focus\":" +
        std::to_string(static_cast<int>(*reinterpret_cast<uint8_t*>(g_base + G_UICHOICE_FOCUS_VMA)));
    out += ",\"options\":[";
    void** texts = reinterpret_cast<void**>(g_base + G_UICHOICE_ITEMTEXT_VMA);
    for (int i = 0; i < 6; ++i) {
        if (i > 0) out += ",";
        char* t = reinterpret_cast<char*>(texts[i]);
        if (t != nullptr) {
            out += "\"" + json_escape(t) + "\"";
        } else {
            out += "null";
        }
    }
    out += "]}";
    return out;
}


// 统一 UI 状态判定（v0.5.42）：screen 唯一来源，替代 dialog_active 布尔判定。
// 判定链：STATE 状态机（主菜单/世界中）→ 教学暂停 → UIPopupMsg 弹窗 → GAMESTATE 剧情
// → popup 栈顶分派（对话框 dialog_* / 面板 panel_*）→ world。
// 与 data_dialog_content_json 判定链同序（popup 最优先，v0.4.39）。
// 修复（v0.5.42）：不再用数据层计数（UICHOICE/NPCTASKLIST）直接判定——NPC 交互后数据残留
// 而 UI 栈已空时旧 dialog_active 误报 true（真机实测：关闭 NPC 对话框后 screen=world 但 dialog_active=true）。
// 现完全以 popup 栈顶为准：栈顶无面板/对话框 → world，残留计数不产生任何误报。
const char* data_ui_screen() {
    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    if (state == 4) {
        // 主菜单：按 popup 栈顶细分（v0.4.18 修复：标题屏/存档选择/职业选择）
        switch (data_popup_top_vma()) {
            case F_PANEL_SAVE_SLOT_ENTER: return "main_menu_save_slot";
            case F_PANEL_CHAR_SELECT_ENTER: return "main_menu_character_select";
            case F_PANEL_DAILY_REWARD_ENTER: return "main_menu_daily_reward";
            case F_PANEL_OPTIONS_ENTER: return "main_menu_options";
            case F_PANEL_SETTINGS_ENTER: return "main_menu_settings";
            default: return "main_menu";
        }
    }
    if (state != 5) return "loading";
    if (tutorial_state() == 6) return "tutorial_pause";  // 药水教学残血暂停
    // 弹窗最优先（v0.4.39：剧情段结束弹任务简报时 gs=1 残留但 UIPopupMsg 激活，弹窗阻塞一切交互）
    if (g_base != 0 && g_popup_on != nullptr && *reinterpret_cast<uint8_t*>(g_popup_on)) return "dialog_popup";
    if (data_story_active()) return "dialog_story";
    // popup 栈顶分派：对话框类（dialog_*）优先于面板类（panel_*）
    uintptr_t top_vma = data_popup_top_vma();
    if (top_vma == 0) return "world";  // 无任何面板/对话框（含数据残留场景）
    switch (top_vma) {
        case F_PANEL_WIPEOUT_ENTER: return "dialog_wipeout";        // 死亡面板
        case F_PANEL_NPC_QUEST_ENTER: return "dialog_quest";        // 任务完成面板
        case F_PANEL_NPC_ENTER: return "dialog_npc";                // NPC 对话
        case F_PANEL_CHOICE_ENTER: return "dialog_choice";          // 选择框（事件驱动）
        case F_PANEL_INPUT_COUNT_ENTER: return "dialog_input_count"; // 数量输入
        case F_PANEL_CHARACTER_INFO_ENTER: return "panel_character_info";
        case F_PANEL_INVENTORY_ENTER: return "panel_inventory";
        case F_PANEL_MERCENARY_ENTER: return "panel_mercenary";
        case F_PANEL_CRAFT_ENTER: return "panel_craft";
        case F_PANEL_NPC_REST_ENTER: return "panel_npc_rest";
        case F_PANEL_NPC_REVIVE_ENTER: return "panel_npc_revive";
        case F_PANEL_OPTIONS_ENTER: return "panel_options";
        case F_PANEL_QUESTS_ENTER: return "panel_quests";
        case F_PANEL_SAVE_SLOT_ENTER: return "panel_save_slot";
        case F_PANEL_CHAR_SELECT_ENTER: return "panel_character_select";
        case F_PANEL_SHORTCUT_ENTER: return "panel_shortcut";
        case F_PANEL_SKILLS_ENTER: return "panel_skills";
        case F_PANEL_SHOP_ENTER: return "panel_shop";
        case F_PANEL_SETTINGS_ENTER: return "panel_settings";
        case F_PANEL_WORLD_MAP_ENTER: return "panel_world_map";
        case F_PANEL_IN_APP_ENTER:
        case F_PANEL_UNK1_ENTER:
        case F_PANEL_UNK2_ENTER:
        case F_PANEL_UNK3_ENTER:
        case F_PANEL_UNK4_ENTER:
        case F_PANEL_UNK5_ENTER: return "panel_in_app";
        case F_PANEL_DAILY_REWARD_ENTER: return "panel_daily_reward";
        default: return "panel_ui_panel";  // 未知栈顶兜底
    }
}

std::string data_dialog_content_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    // 弹窗最优先（v0.4.39 修复）：剧情段结束弹任务简报时 gs=1 残留但 UIPopupMsg 激活，
    // 若 story 优先会遮蔽弹窗 → select ok 无法确认任务简报。弹窗会阻塞一切下层交互。
    if (g_base != 0 && g_popup_on != nullptr && *reinterpret_cast<uint8_t*>(g_popup_on)) {
        std::string out = "{\"type\":\"popup\"";
        uint8_t* pt = *reinterpret_cast<uint8_t**>(g_base + G_POPUP_TEXT_VMA);
        if (pt != nullptr) {
            std::string dtext;
            for (int i = 0; i < 256 && pt[i] != 0; ++i) dtext += static_cast<char>(pt[i]);
            out += ",\"text\":\"" + json_escape(dtext.c_str()) + "\"";
        }
        bool has_ok = *reinterpret_cast<uint64_t*>(g_base + G_POPUP_FPOK_VMA) != 0;
        bool has_cancel = *reinterpret_cast<uint64_t*>(g_base + G_POPUP_FPCANCEL_VMA) != 0;
        out += ",\"options\":[";
        bool first = true;
        if (has_ok) { out += "{\"id\":\"ok\",\"label\":\"确认\"}"; first = false; }
        if (has_cancel) { if (!first) out += ","; out += "{\"id\":\"cancel\",\"label\":\"取消\"}"; }
        out += "]}";
        return out;
    }
    if (data_story_active()) {
        // story 态：统一 type 字段 + 剧情推进/跳过作为选项暴露（v0.4.31 修复）
        std::string sj = data_story_json();
        if (sj.size() > 1 && sj[0] == '{') {
            sj.insert(1, "\"type\":\"story\",\"options\":[{\"id\":\"next\",\"label\":\"下一句\"},{\"id\":\"skip\",\"label\":\"跳过\"}],");
        }
        return sj;
    }
    if (g_base == 0) return "{\"type\":\"none\",\"options\":[]}";
    // NPC 对话（UICHOICE 选项优先）
    uint8_t choice_count = *reinterpret_cast<uint8_t*>(g_base + G_UICHOICE_COUNT_VMA);
    uint8_t task_count = *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_COUNT_VMA);
    // wipeout 死亡面板（v0.4.35）：栈顶 enter == F_PANEL_WIPEOUT_ENTER 时优先于 NPC 对话报告
    uintptr_t top_vma = data_popup_top_vma();
    if (top_vma == F_PANEL_WIPEOUT_ENTER) {
        std::string out = "{\"type\":\"wipeout\",\"options\":["
                          "{\"id\":\"revive\",\"label\":\"复活\"},"
                          "{\"id\":\"special_revive\",\"label\":\"特殊复活\"},"
                          "{\"id\":\"game_over\",\"label\":\"游戏结束\"}]}";
        return out;
    }
    // NPC 任务完成面板（v0.4.55）：栈顶 enter == F_PANEL_NPC_QUEST_ENTER（npc_quest）时报告任务完成态，
    // 选项 complete=完成任务（UINpcQuest_ButtonOKExe 官方链）close=关闭面板（panel/close）。
    if (top_vma == F_PANEL_NPC_QUEST_ENTER) {
        std::string out = "{\"type\":\"npc_quest\"";
        int quest_id = -1;
        if (g_base != 0) {
            uint8_t** idx_ptr = reinterpret_cast<uint8_t**>(g_base + G_NPC_QUEST_IDX_GOT_VMA);
            if (idx_ptr != nullptr && *idx_ptr != nullptr)
                quest_id = *reinterpret_cast<int16_t*>(*idx_ptr);
        }
        out += ",\"quest_id\":" + std::to_string(quest_id);
        uint8_t state = 0xFF;
        if (g_base != 0 && quest_id >= 0) {
            uint8_t*** st_got = reinterpret_cast<uint8_t***>(g_base + G_NPC_QUEST_STATE_GOT_VMA);
            if (st_got != nullptr && *st_got != nullptr)
                state = (**st_got)[quest_id];
        }
        out += ",\"state\":" + std::to_string(static_cast<int>(state));
        out += ",\"options\":[{\"id\":\"complete\",\"label\":\"完成任务\"},"
               "{\"id\":\"close\",\"label\":\"关闭\"}]}";
        return out;
    }
    if (choice_count > 0 || task_count > 0) {
        std::string out = "{\"type\":\"npc\"";
        // displayed：区分「UI 对话框已显示」（popup 栈顶 npc 面板）vs「数据已建立但未渲染」
        // （如 start_interact 前的数据态、路过不可交互装饰物残留的 NEAR_NPC 数据）。
        out += ",\"displayed\":" +
               std::string(data_popup_top_vma() == F_PANEL_NPC_ENTER ? "true" : "false");
        void* near_npc = *reinterpret_cast<void**>(g_base + G_PLAYER_NEAR_NPC_VMA);
        if (near_npc != nullptr && fn_get_name != nullptr) {
            char* nm = fn_get_name(near_npc);
            if (nm != nullptr) out += ",\"speaker\":\"" + json_escape(nm) + "\"";
        }
        char* desc = *reinterpret_cast<char**>(g_base + G_NPCTASKLIST_DESCTEXT_VMA);
        if (desc != nullptr && desc[0] != 0) {
            out += ",\"text\":\"" + json_escape(desc) + "\"";
        }
        out += ",\"options\":[";
        if (choice_count > 0) {
            void** texts = reinterpret_cast<void**>(g_base + G_UICHOICE_ITEMTEXT_VMA);
            for (int i = 0; i < choice_count && i < 6; ++i) {
                if (i > 0) out += ",";
                char* t = reinterpret_cast<char*>(texts[i]);
                out += "{\"id\":\"" + std::to_string(i) + "\",\"label\":" +
                       (t != nullptr ? "\"" + json_escape(t) + "\"" : "\"\"") + "}";
            }
        } else {
            out += "{\"id\":\"next\",\"label\":\"下一句\"}";
        }
        out += "]}";
        return out;
    }
    // 面板态（v0.5.6 U1）：无弹窗/剧情/死亡/NPC 对话时，若栈顶是面板则报面板类型 + 动作。
    // save_slot 额外暴露 save（存档落盘，data_op_save 官方链）；其余面板仅 close（panel/close 官方流程3）。
    const char* pname = data_top_panel_name();
    if (pname != nullptr) {
        std::string out = "{\"type\":\"" + std::string(pname) + "\",\"options\":[";
        if (strcmp(pname, "save_slot") == 0)
            out += "{\"id\":\"save\",\"label\":\"存档\"},";
        out += "{\"id\":\"close\",\"label\":\"关闭\"}]}";
        return out;
    }
    return "{\"type\":\"none\",\"options\":[]}";
}


std::string data_shop_items_json() {
    if (g_base == 0 || fn_item_get_buy_price == nullptr || fn_get_bit == nullptr || fn_get_cumulate_count == nullptr) return "{\"items\":[]}";
    uint8_t* sale_list = reinterpret_cast<uint8_t*>(*(reinterpret_cast<void**>(g_base + G_DEALSYSTEM_SALE_LIST_VMA)));
    if (sale_list == nullptr) return "{\"items\":[]}";
    std::string s = "{\"items\":[";
    bool first = true;
    for (int i = 0; i < 48; ++i) {
        uint8_t* slot = sale_list + i * 16;
        uint64_t flags = *reinterpret_cast<uint64_t*>(slot);
        if (flags & 1) continue;  // bit0=空/已售
        void* item = *reinterpret_cast<void**>(slot + 8);
        if (item == nullptr) continue;
        uint16_t iflags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
        uint32_t category = fn_get_bit(iflags, 15, 6);
        uint32_t count = fn_get_cumulate_count(item);
        int price = fn_item_get_buy_price(item);
        if (!first) s += ",";
        first = false;
        s += "{\"slot\":" + std::to_string(i) + ",\"category\":" + std::to_string(category) +
             ",\"count\":" + std::to_string(count) + ",\"price\":" + std::to_string(price) + "}";
    }
    s += "]}";
    return s;
}


namespace {

Snapshot take_snapshot() {
    Snapshot s{};
    s.money = (fn_get_money != nullptr) ? fn_get_money() : -1;
    s.x = s.y = -1;
    s.inv_count = inventory_count();
    for (int i = 0; i < 3; ++i) {
        void* ch = (fn_get_member != nullptr) ? fn_get_member(i) : nullptr;
        s.hp[i] = s.mp[i] = s.level[i] = -1;
        s.exp[i] = -1;
        if (ch != nullptr) {
            uint8_t* b = reinterpret_cast<uint8_t*>(ch);
            s.hp[i] = *reinterpret_cast<int32_t*>(b + C_HP);
            s.mp[i] = *reinterpret_cast<int32_t*>(b + C_MP);
            s.level[i] = reinterpret_cast<int8_t*>(ch)[C_LEVEL];
            s.exp[i] = (fn_get_exp != nullptr) ? fn_get_exp(ch) : -1;
        }
    }
    void* lead = (fn_get_member != nullptr) ? fn_get_member(0) : nullptr;
    if (lead != nullptr) {
        uint8_t* b = reinterpret_cast<uint8_t*>(lead);
        s.x = *reinterpret_cast<int16_t*>(b + C_POS_X);
        s.y = *reinterpret_cast<int16_t*>(b + C_POS_Y);
    }
    return s;
}

void emit(std::string& out, bool& first, const char* type, int role, int64_t a, int64_t b) {
    if (!first) out += ",";
    first = false;
    out += "{\"type\":\"" + std::string(type) + "\"";
    if (role >= 0) out += ",\"role\":" + std::to_string(role);
    out += ",\"old\":" + std::to_string(a);
    out += ",\"new\":" + std::to_string(b);
    out += "}";
}

}  // namespace

std::string data_events_json() {
    Snapshot cur = take_snapshot();
    std::lock_guard<std::mutex> lock(g_events_mtx);
    static Snapshot last;
    static bool has_last = false;
    std::string s = "{\"events\":[";
    bool first = true;
    if (!has_last) {
        has_last = true;
        last = cur;
        s += "]}";
        return s;
    }
    if (cur.money >= 0 && last.money >= 0 && cur.money != last.money)
        emit(s, first, "money", -1, last.money, cur.money);
    if (cur.inv_count >= 0 && last.inv_count >= 0 && cur.inv_count != last.inv_count)
        emit(s, first, "inventory", -1, last.inv_count, cur.inv_count);
    if (cur.x >= 0 && last.x >= 0 && (cur.x != last.x || cur.y != last.y))
        emit(s, first, "move", -1, 0, 0);
    for (int i = 0; i < 3; ++i) {
        if (cur.hp[i] >= 0 && last.hp[i] >= 0 && cur.hp[i] != last.hp[i])
            emit(s, first, "hp", i, last.hp[i], cur.hp[i]);
        if (cur.mp[i] >= 0 && last.mp[i] >= 0 && cur.mp[i] != last.mp[i])
            emit(s, first, "mp", i, last.mp[i], cur.mp[i]);
        if (cur.level[i] >= 0 && last.level[i] >= 0 && cur.level[i] != last.level[i])
            emit(s, first, "level_up", i, last.level[i], cur.level[i]);
        if (cur.exp[i] >= 0 && last.exp[i] >= 0 && cur.exp[i] != last.exp[i])
            emit(s, first, "exp", i, last.exp[i], cur.exp[i]);
    }
    last = cur;
    s += "]}";
    return s;
}

