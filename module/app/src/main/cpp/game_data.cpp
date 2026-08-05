#include "game_data.h"

#include "game_access.h"
#include "game_symbols.h"

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

void append_item_attrs(std::string& s, void* item);

void json_append_int(std::string& out, int64_t v) {
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    out.append(buf, n);
}

std::string member_json(void* ch) {
    std::string s = "{";
    s += "\"type\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_TYPE]));
    uint16_t name_id = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(ch) + C_NAME_ID);
    s += ",\"nameId\":" + std::to_string(name_id);
    s += ",\"level\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_LEVEL]));
    s += ",\"hp\":" + std::to_string(*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_HP));
    s += ",\"mp\":" + std::to_string(*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_MP));
    if (fn_get_attr != nullptr) {
        s += ",\"maxHp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_HP));
        s += ",\"maxMp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_MP));
    }
    if (fn_get_exp != nullptr) {
        s += ",\"exp\":" + std::to_string(fn_get_exp(ch));
        s += ",\"expNext\":" + std::to_string(fn_get_next_exp(ch));
    }
    s += ",\"stats\":{";
    bool first = true;
    for (int a = 0; a < 32; ++a) {
        int32_t v = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_ATTR + a * 4);
        if (!first) s += ",";
        s += "\"" + std::to_string(a) + "\":" + std::to_string(v);
        first = false;
    }
    s += "}";
    if (fn_get_stat != nullptr) {
        s += ",\"mainStats\":[";
        for (int a = 0; a < 5; ++a) {
            if (a > 0) s += ",";
            s += std::to_string(fn_get_stat(ch, a));
        }
        s += "]";
    }
    if (fn_get_status_point != nullptr) {
        s += ",\"statusPoint\":" + std::to_string(fn_get_status_point(ch));
    }
    s += ",\"equipment\":[";
    for (int slot = 0; slot < C_EQUIP_SLOTS; ++slot) {
        void* item = nullptr;
        if (fn_get_equip != nullptr) item = fn_get_equip(ch, slot);
        if (slot > 0) s += ",";
        if (item == nullptr) {
            s += "null";
        } else {
            uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
            s += "{\"slot\":" + std::to_string(slot);
            s += ",\"typeFlags\":" + std::to_string(flags);
            if (fn_get_bit != nullptr) {
                s += ",\"category\":" + std::to_string(fn_get_bit(flags, 15, 6));
            }
            if (fn_get_rarity != nullptr) {
                s += ",\"rarity\":" + std::to_string(fn_get_rarity(item));
            }
            append_item_attrs(s, item);
            s += "}";
        }
    }
    s += "]}";
    return s;
}

void append_item_attrs(std::string& s, void* item) {
    uint8_t* it = reinterpret_cast<uint8_t*>(item);
    if (fn_get_damage != nullptr) {
        s += ",\"damage\":" + std::to_string(fn_get_damage(item));
    }
    if (fn_get_defense != nullptr) {
        s += ",\"defense\":" + std::to_string(fn_get_defense(item));
    }
    s += ",\"magicRate\":" + std::to_string(it[I_MAGIC_RATE]);
    s += ",\"socket\":" + std::to_string(it[I_SOCKET]);
    s += ",\"enchant\":" + std::to_string(*reinterpret_cast<uint16_t*>(it + I_ENCHANT));
    s += ",\"options\":[";
    uint8_t* opt = *reinterpret_cast<uint8_t**>(it + I_OPTION_LIST);
    bool ofirst = true;
    int ocount = 0;
    while (opt != nullptr && ocount < 32) {
        if (!ofirst) s += ",";
        s += std::to_string(*reinterpret_cast<int16_t*>(opt + O_VALUE));
        ofirst = false;
        opt = *reinterpret_cast<uint8_t**>(opt + O_NEXT);
        ++ocount;
    }
    s += "]";
}

void append_position(std::string& s, void* member) {    if (member != nullptr) {
        s += ",\"x\":" + std::to_string(
            *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(member) + C_POS_X));
        s += ",\"y\":" + std::to_string(
            *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(member) + C_POS_Y));
    } else {
        s += ",\"x\":-1,\"y\":-1";
    }
}

void* lead_member() {
    return fn_get_member != nullptr ? fn_get_member(0) : nullptr;
}

}  // namespace

std::string data_player_json() {
    std::string s = "{";
    s += "\"money\":" + std::to_string(fn_get_money != nullptr ? fn_get_money() : -1);
    s += ",\"mapId\":" + std::to_string(g_map_id != nullptr ? *reinterpret_cast<uint16_t*>(g_map_id) : -1);
    append_position(s, lead_member());
    s += ",\"activeQuest\":" + std::to_string(g_active_quest != nullptr ? *reinterpret_cast<uint16_t*>(g_active_quest) : -1);
    s += ",\"mainMercenarySlot\":" + std::to_string(g_main_merc_slot != nullptr ? *reinterpret_cast<uint8_t*>(g_main_merc_slot) : -1);
    s += ",\"partyCount\":" + std::to_string(fn_get_party_size != nullptr ? fn_get_party_size() : 3);
    s += "}";
    return s;
}

std::string data_party_json() {
    std::string s = "[";
    for (int i = 0; i < 3; ++i) {
        void* ch = (fn_get_member != nullptr) ? fn_get_member(i) : nullptr;
        if (i > 0) s += ",";
        if (ch == nullptr) {
            s += "null";
        } else {
            s += member_json(ch);
        }
    }
    s += "]";
    return s;
}

std::string data_inventory_json() {
    // INVEN_pItem(0x7131c0)：背包槽数组，6 袋 × 0x80 步长，每槽 8B 物品指针。
    // 每袋 16 槽（6×16=96，与真机实测 slotCount 总和一致）。
    // 空槽=0 指针；物品 +0x08 类型位域(u16)、+0x10 数量位域(u32)。
    constexpr int BAG_COUNT = 6;
    constexpr size_t BAG_STRIDE = 0x80;
    constexpr int SLOTS_PER_BAG = 16;
    std::string s = "{\"bags\":[";
    for (int b = 0; b < BAG_COUNT; ++b) {
        if (b > 0) s += ",";
        s += "{\"bag\":" + std::to_string(b) + ",\"items\":[";
        int filled = 0;
        if (g_inven != nullptr) {
            uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * BAG_STRIDE;
            for (int j = 0; j < SLOTS_PER_BAG; ++j) {
                void* item = *reinterpret_cast<void**>(bag_slots + j * 8);
                if (item == nullptr) continue;
                uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
                if (filled > 0) s += ",";
                s += "{\"slot\":" + std::to_string(j);
                s += ",\"typeFlags\":" + std::to_string(flags);
                if (fn_get_bit != nullptr) {
                    s += ",\"category\":" + std::to_string(fn_get_bit(flags, 15, 6));
                    uint32_t count_field = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT);
                    s += ",\"count\":" + std::to_string(fn_get_bit(static_cast<int>(count_field), 31, 25));
                }
                if (fn_get_rarity != nullptr) {
                    s += ",\"rarity\":" + std::to_string(fn_get_rarity(item));
                }
                append_item_attrs(s, item);
                s += "}";
                ++filled;
            }
        }
        s += "],\"capacity\":16,\"slotCount\":" + std::to_string(filled) + "}";
    }
    s += "]}";
    return s;
}

std::string data_map_json() {
    std::string s = "{";
    s += "\"mapId\":" + std::to_string(g_map_id != nullptr ? *reinterpret_cast<uint16_t*>(g_map_id) : -1);
    append_position(s, lead_member());
    s += "}";
    return s;
}

std::string data_units_json() {
    // CHARSYSTEM 角色对象池：*(G_CHAR_POOL_VMA) 指向英雄对象，对象按 C_OBJ_SIZE 步长连续排列
    // （frida 实测 2026-08-05：31 有效单位 = 3 队伍 + 怪物 + NPC，坐标与玩家同像素坐标系）。
    // 有效性：type 0-2、status<=2、坐标 0-1500（未激活槽哨兵值 2048/16992/status>2，frida 实测排除）。
    // status: 0=队伍 1=城镇NPC/佣兵 2=怪物/召唤物。
    constexpr int POOL_SLOTS = 128;
    std::string s = "{\"units\":[";
    if (g_base != 0) {
        uint8_t* pool = *reinterpret_cast<uint8_t**>(g_base + G_CHAR_POOL_VMA);
        if (pool != nullptr) {
            int emitted = 0;
            for (int i = 0; i < POOL_SLOTS; ++i) {
                uint8_t* obj = pool + i * C_OBJ_SIZE;
                int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
                int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
                int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
                uint8_t status = obj[C_STATUS];
                if (type < 0 || type > 2) continue;
                if (status > 2) continue;
                if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) continue;
                if (emitted > 0) s += ",";
                s += "{\"slot\":" + std::to_string(i);
                s += ",\"x\":" + std::to_string(x);
                s += ",\"y\":" + std::to_string(y);
                s += ",\"type\":" + std::to_string(type);
                s += ",\"status\":" + std::to_string(status);
                s += "}";
                ++emitted;
            }
        }
    }
    s += "]}";
    return s;
}

std::string data_ui_json() {
    std::string s = "{";
    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    s += "\"state\":" + std::to_string(state);
    s += ",\"stateName\":\"";
    switch (state) {
        case 4: s += "main_menu"; break;
        case 5: s += "in_game"; break;
        default: s += "unknown"; break;
    }
    s += "\"";
    s += ",\"inGame\":" + std::string(state == 5 ? "true" : "false");
    s += ",\"gamestate\":" + std::to_string(g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : -1);
    s += ",\"initstate\":" + std::to_string(g_initstate != nullptr ? *reinterpret_cast<uint8_t*>(g_initstate) : -1);
    s += "}";
    return s;
}

std::string data_skills_json() {
    // 每角色技能：+0x2A0 链表（节点 action_id/level/next）、+0x2B0 解锁位图、
    // +0x280 当前技能、+0x328 剩余技能点。链表节点步长由 next(+0x18) 驱动。
    std::string s = "[";
    for (int i = 0; i < 3; ++i) {
        void* ch = (fn_get_member != nullptr) ? fn_get_member(i) : nullptr;
        if (i > 0) s += ",";
        if (ch == nullptr) {
            s += "null";
            continue;
        }
        uint8_t* base_ch = reinterpret_cast<uint8_t*>(ch);
        s += "{\"role\":" + std::to_string(i);
        s += ",\"skills\":[";
        uint8_t* node = *reinterpret_cast<uint8_t**>(base_ch + C_SKILL_LIST);
        bool first = true;
        int count = 0;
        while (node != nullptr && count < 64) {
            if (!first) s += ",";
            s += "{\"actionId\":" + std::to_string(*reinterpret_cast<uint16_t*>(node + S_ACTION_ID));
            s += ",\"level\":" + std::to_string(node[S_LEVEL]);
            s += "}";
            first = false;
            node = *reinterpret_cast<uint8_t**>(node + S_NEXT);
            ++count;
        }
        s += "]";
        s += ",\"unlockBitmap\":" + std::to_string(*reinterpret_cast<uint16_t*>(base_ch + C_SKILL_BMP));
        uint8_t* active = *reinterpret_cast<uint8_t**>(base_ch + C_ACTIVE_SKILL);
        s += ",\"activeSkillId\":" + std::to_string(active != nullptr ? *reinterpret_cast<uint16_t*>(active + S_ACTION_ID) : -1);
        s += ",\"skillPoints\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_SKILL_POINTS]));
        s += "}";
    }
    s += "]";
    return s;
}

void* find_char_by_merc_slot(int slot) {
    // 用游戏函数 CHARSYSTEM_FindAsMercenarySlot（遍历大池含未上场佣兵）
    return fn_find_merc_slot != nullptr ? fn_find_merc_slot(slot) : nullptr;
}

std::string json_escape(const char* s) {
    std::string out;
    for (; s != nullptr && *s != '\0'; ++s) {
        unsigned char c = static_cast<unsigned char>(*s);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string data_mercenaries_json() {
    // 佣兵槽：*(*(G_MERC_SLOTLIST_GOT_VMA)) 槽数组（20B/槽），flags bit0=占用 bit1=在队伍。
    // 关联角色：角色池 +0x352==slot；名称 CHAR_GetName、等级 +0x0E、坐标 +0x02/+0x04。
    std::string s = "[";
    if (g_base != 0) {
        uintptr_t got = *reinterpret_cast<uintptr_t*>(g_base + G_MERC_SLOTLIST_GOT_VMA);
        uint8_t* slots = got != 0 ? *reinterpret_cast<uint8_t**>(got) : nullptr;
        int8_t max_slots = *reinterpret_cast<int8_t*>(g_base + G_MERC_MAX_VMA);
        if (slots != nullptr && max_slots > 0) {
            int emitted = 0;
            for (int i = 0; i < max_slots && i < 128; ++i) {
                uint8_t* slot = slots + i * M_SLOT_SIZE;
                uint8_t flags = slot[M_FLAGS];
                if ((flags & 0x01) == 0) continue;
                if (slot[M_TYPE] > 2) continue;
                if (emitted > 0) s += ",";
                s += "{\"slot\":" + std::to_string(i);
                s += ",\"type\":" + std::to_string(slot[M_TYPE]);
                s += ",\"flags\":" + std::to_string(flags);
                s += ",\"inParty\":" + std::string((flags & 0x02) ? "true" : "false");
                void* ch = find_char_by_merc_slot(i);
                if (ch != nullptr) {
                    if (fn_get_name != nullptr) {
                        char* nm = fn_get_name(ch);
                        s += ",\"name\":\"" + json_escape(nm) + "\"";
                    }
                    s += ",\"level\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_LEVEL]));
                    append_position(s, ch);
                } else {
                    s += ",\"name\":null";
                }
                s += "}";
                ++emitted;
            }
        }
    }
    s += "]";
    return s;
}

std::string data_path_json(int tx, int ty) {
    // CHAR_SearchPath(hero, tx, ty, 1) 计算角色到目标的路径，结果存角色 +0x2F0 PATHLIST
    // （仅计算存储，不触发移动）。节点：+0x00 u16 网格x、+0x02 u16 网格y、+0x08 next；网格×8=像素。
    std::string s = "{\"target\":{\"x\":" + std::to_string(tx) + ",\"y\":" + std::to_string(ty) + "}";
    void* hero = lead_member();
    s += ",\"start\":{";
    if (hero != nullptr) {
        s += "\"x\":" + std::to_string(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X));
        s += ",\"y\":" + std::to_string(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y));
    } else {
        s += "\"x\":-1,\"y\":-1";
    }
    s += "}";
    int ret = 0;
    if (hero != nullptr && fn_search_path != nullptr) {
        ret = fn_search_path(hero, tx, ty, 1);
    }
    s += ",\"found\":" + std::string(ret != 0 ? "true" : "false");
    s += ",\"path\":[";
    if (hero != nullptr) {
        uint8_t* node = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(hero) + C_PATH_LIST);
        bool first = true;
        int count = 0;
        while (node != nullptr && count < 128) {
            int gx = *reinterpret_cast<uint16_t*>(node + 0x00);
            int gy = *reinterpret_cast<uint16_t*>(node + 0x02);
            if (!first) s += ",";
            s += "{\"x\":" + std::to_string(gx * 8) + ",\"y\":" + std::to_string(gy * 8) + "}";
            first = false;
            node = *reinterpret_cast<uint8_t**>(node + 0x08);
            ++count;
        }
    }
    s += "]}";
    return s;
}

int data_active_quest() {
    if (g_active_quest == nullptr) return -1;
    return *reinterpret_cast<uint16_t*>(g_active_quest);
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
