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

void append_position(std::string& s, void* member) {
    if (member != nullptr) {
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
        "\"state\":%u,\"prevState\":%u,\"gamestate\":%u,\"initstate\":%u,"
        "\"popupOn\":%u,\"menuDraw\":%u,"
        "\"popupStack\":[",
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
    s += "],\"popupStackHex\":\"";
    if (g_popup_stack) {
        uint8_t* stk = reinterpret_cast<uint8_t*>(g_popup_stack);
        for (int i = 0; i < 32; ++i) {
            snprintf(buf, sizeof(buf), "%02x", stk[i]);
            s += buf;
        }
    }
    s += "\"";

    if (g_base != 0) {
        int32_t i32type = *reinterpret_cast<int32_t*>(g_base + 0x712518);
        int32_t i32disp  = *reinterpret_cast<int32_t*>(g_base + 0x712510);
        s += ",\"popupType\":" + std::to_string(i32type);
        s += ",\"popupDispType\":" + std::to_string(i32disp);

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

        r8(0x728ed8, "partyMenuIndex");
        ru8(0x7125c8, "questMenuState");
        ru8(0x712628, "storeBuyType");
        ru8(0x712630, "storeSelectedClass");
        ru8(0x711c90, "helpState");
        ru8(0x7135a9, "mainmenuSelectedClass");
        ru8(0x7135aa, "mainmenuSaveSlotType");
        ru8(0x302d80, "choiceFocusIndex");
        ru8(0x712600, "shortcutPage");
        ru16(0x7125c0, "questMenuMainListSize");
        ru16(0x7125f8, "questMenuSubListSize");
        ru32(0x3070d8, "popupFpCancelLo");
    }

    s += "}";
    return s;
}

std::string data_gamestate_json() {
    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    uint16_t prev = g_prev_state != nullptr ? *reinterpret_cast<uint16_t*>(g_prev_state) : 0xFFFF;
    uint32_t gs = g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
    uint8_t init = g_initstate != nullptr ? *reinterpret_cast<uint8_t*>(g_initstate) : 0;
    uint8_t popup_on = g_popup_on != nullptr ? *reinterpret_cast<uint8_t*>(g_popup_on) : 0;

    const char* screen = "loading";
    if (state == 4) {
        screen = "main_menu";
    } else if (state == 5) {
        if (popup_on) {
            screen = "dialog";
        } else {
            const char* panel = nullptr;
            if (g_popup_stack != nullptr && g_base != 0) {
                uint8_t* stk = reinterpret_cast<uint8_t*>(g_popup_stack);
                uint32_t count = *reinterpret_cast<uint32_t*>(stk + 8);
                if (count > 0 && count <= 27) {
                    uint64_t data = *reinterpret_cast<uint64_t*>(stk + 0x18);
                    if (data != 0) {
                        uint8_t* top = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data)) + (count - 1) * 0x40;
                        uintptr_t enter = *reinterpret_cast<uintptr_t*>(top + 0x10);
                        uintptr_t vma = enter > g_base ? enter - g_base : 0;
                        switch (vma) {
                            case 0x148950: panel = "character_info"; break;
                            case 0x14a664: panel = "choice"; break;
                            case 0x14a8b0: panel = "inventory"; break;
                            case 0x14ad98: panel = "input_count"; break;
                            case 0x14af14: panel = "mercenary"; break;
                            case 0x14b330: panel = "craft"; break;
                            case 0x14b5dc: panel = "npc"; break;
                            case 0x14b858: panel = "npc_quest"; break;
                            case 0x14ba98: panel = "npc_rest"; break;
                            case 0x14bb48: panel = "npc_revive"; break;
                            case 0x14be20: panel = "options"; break;
                            case 0x14c218: panel = "quests"; break;
                            case 0x14c720: panel = "save_slot"; break;
                            case 0x14d670: panel = "character_select"; break;
                            case 0x14df04: panel = "shortcut"; break;
                            case 0x14f194: panel = "skills"; break;
                            case 0x14f4b8: panel = "shop"; break;
                            case 0x14fb38: panel = "settings"; break;
                            case 0x1506d8: panel = "wipeout"; break;
                            case 0x150f48: panel = "world_map"; break;
                            case 0x15e054:
                            case 0x15e3dc:
                            case 0x15e740:
                            case 0x15eac8:
                            case 0x15ee70:
                            case 0x15f1f8: panel = "in_app"; break;
                            case 0x16f050: panel = "daily_reward"; break;
                            default: panel = "ui_panel"; break;
                        }
                    }
                }
            }
            screen = panel ? panel : "world";
        }
    }

    std::string result = "{\"screen\":\"" + std::string(screen) + "\",\"dialogActive\":" + (popup_on ? "true" : "false");
    if (popup_on && g_base != 0) {
        std::string dtext;
        uint8_t* pt = *reinterpret_cast<uint8_t**>(g_base + 0x3070b8);
        if (pt != nullptr) {
            for (int i = 0; i < 256 && pt[i] != 0; ++i) dtext += static_cast<char>(pt[i]);
        }
        bool has_ok = *reinterpret_cast<uint64_t*>(g_base + 0x3070e0) != 0;
        bool has_cancel = *reinterpret_cast<uint64_t*>(g_base + 0x3070d8) != 0;
        std::string esc;
        for (char c : dtext) {
            if (c == '"' || c == '\\') esc += '\\';
            else if (c == '\n') esc += "\\n";
            else if (c == '\r') esc += "\\r";
            else if (c == '\t') esc += "\\t";
            else esc += c;
        }
        result += ",\"dialog\":{\"text\":\"" + esc + "\",\"hasOk\":" + (has_ok ? "true" : "false") +
                  ",\"hasCancel\":" + (has_cancel ? "true" : "false") + "}";
    }
    result += "}";
    return result;
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

std::string data_snapshot_json() {
    std::string s = "{";

    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    s += "\"screen\":";
    s += "\"" + std::string(state == 4 ? "main_menu" : (state == 5 ? "world" : "loading")) + "\"";

    s += ",\"money\":" + std::to_string(fn_get_money != nullptr ? fn_get_money() : -1);
    s += ",\"mapId\":" + std::to_string(g_map_id != nullptr ? *reinterpret_cast<uint16_t*>(g_map_id) : -1);
    void* hero = lead_member();
    if (hero != nullptr) {
        s += ",\"x\":" + std::to_string(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X));
        s += ",\"y\":" + std::to_string(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y));
    } else {
        s += ",\"x\":-1,\"y\":-1";
    }
    s += ",\"mainMercenarySlot\":" + std::to_string(g_main_merc_slot != nullptr ? *reinterpret_cast<uint8_t*>(g_main_merc_slot) : -1);
    s += ",\"partyCount\":" + std::to_string(fn_get_party_size != nullptr ? fn_get_party_size() : 3);

    s += ",\"party\":[";
    for (int i = 0; i < 3; ++i) {
        void* ch = (fn_get_member != nullptr) ? fn_get_member(i) : nullptr;
        if (i > 0) s += ",";
        if (ch == nullptr) {
            s += "null";
            continue;
        }
        uint8_t* b = reinterpret_cast<uint8_t*>(ch);
        int ch_type = static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_TYPE]);
        int level = static_cast<int>(b[C_LEVEL]);
        s += "{\"type\":" + std::to_string(ch_type);
        s += ",\"level\":" + std::to_string(level);
        s += ",\"hp\":" + std::to_string(*reinterpret_cast<int32_t*>(b + C_HP));
        s += ",\"mp\":" + std::to_string(*reinterpret_cast<int32_t*>(b + C_MP));
        if (fn_get_attr != nullptr) {
            s += ",\"maxHp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_HP));
            s += ",\"maxMp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_MP));
        }
        if (fn_get_stat != nullptr) {
            s += ",\"mainStats\":[";
            for (int a = 0; a < 5; ++a) {
                if (a > 0) s += ",";
                s += std::to_string(fn_get_stat(ch, a));
            }
            s += "]";
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
                if (fn_get_bit != nullptr) {
                    s += ",\"category\":" + std::to_string(fn_get_bit(flags, 15, 6));
                }
                if (fn_get_rarity != nullptr) {
                    s += ",\"rarity\":" + std::to_string(fn_get_rarity(item));
                }
                s += "}";
            }
        }
        s += "]";
        if (fn_get_name != nullptr) {
            char* nm = fn_get_name(ch);
            s += ",\"name\":\"" + json_escape(nm) + "\"";
        }
        s += "}";
    }
    s += "]";

    s += ",\"mercenaries\":[";
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

    s += "}";
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

// ============================================================
// 写操作层（v0.3.0，2026-08-05 逆向实现）
// 调用前检查 STATE_nState==5（游戏中）；直接调用游戏函数
// （函数内部连续执行无阻塞点，读操作已真机验证跨线程安全）。
// 返回 JSON：{"ok":true,...} 或 {"ok":false,"error":"..."}
// ============================================================

namespace {

std::string op_ok() { return "{\"ok\":true}"; }

std::string op_err(const char* msg) {
    return std::string("{\"ok\":false,\"error\":\"") + msg + "\"}";
}

bool game_in_world() {
    return g_state != nullptr && *reinterpret_cast<uint16_t*>(g_state) == 5;
}

void* member_or_null(int role) {
    return (fn_get_member != nullptr && role >= 0 && role < 3) ? fn_get_member(role) : nullptr;
}

void* find_inventory_item(int category) {
    if (g_inven == nullptr || fn_get_bit == nullptr) return nullptr;
    for (int b = 0; b < 6; ++b) {
        uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * 0x80;
        for (int j = 0; j < 16; ++j) {
            void* item = *reinterpret_cast<void**>(bag_slots + j * 8);
            if (item == nullptr) continue;
            uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
            if (fn_get_bit(flags, 15, 6) == category) return item;
        }
    }
    return nullptr;
}

int inventory_count() {
    if (g_inven == nullptr) return -1;
    int n = 0;
    for (int b = 0; b < 6; ++b) {
        uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * 0x80;
        for (int j = 0; j < 16; ++j) {
            if (*reinterpret_cast<void**>(bag_slots + j * 8) != nullptr) ++n;
        }
    }
    return n;
}

}  // namespace

std::string data_op_set_money(int64_t money) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_set_money == nullptr) return op_err("symbol not resolved");
    fn_set_money(money);
    return op_ok();
}

std::string data_op_add_money(int64_t delta) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_add_money == nullptr) return op_err("symbol not resolved");
    int r = fn_add_money(delta);
    return r ? op_ok() : op_err("add money failed");
}

std::string data_op_minus_money(int64_t delta) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_minus_money == nullptr) return op_err("symbol not resolved");
    int r = fn_minus_money(delta);
    return r ? op_ok() : op_err("insufficient money");
}

std::string data_op_set_experience(int role, int64_t exp) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_exp == nullptr) return op_err("symbol not resolved");
    fn_set_exp(ch, static_cast<int32_t>(exp));
    return op_ok();
}

std::string data_op_add_experience(int role, int64_t delta) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_add_exp == nullptr) return op_err("symbol not resolved");
    int r = fn_add_exp(ch, static_cast<int32_t>(delta), 1);
    return r ? op_ok() : op_err("add exp failed");
}

std::string data_op_set_status_point(int role, int32_t points) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_status_point == nullptr) return op_err("symbol not resolved");
    fn_set_status_point(ch, points);
    return op_ok();
}

std::string data_op_set_auto_attack(int role, int32_t onoff) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_auto_attack == nullptr) return op_err("symbol not resolved");
    fn_set_auto_attack(ch, onoff ? 1 : 0);
    return op_ok();
}

std::string data_op_equip(int role, int bag, int slot) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (g_inven == nullptr || fn_equip_item == nullptr || fn_can_equip == nullptr)
        return op_err("symbol not resolved");
    if (bag < 0 || bag >= 6 || slot < 0 || slot >= 16) return op_err("bad slot");
    uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + bag * 0x80;
    void* item = *reinterpret_cast<void**>(bag_slots + slot * 8);
    if (item == nullptr) return op_err("slot empty");
    if (!fn_can_equip(ch, item)) return op_err("cannot equip");
    // CHAR_EquipItem 在目标槽已被占用时返回 0；先查槽位，占用则自动脱下旧装备再穿。
    if (fn_find_equip_slot != nullptr && fn_get_equip_item != nullptr && fn_unequip != nullptr) {
        int target = fn_find_equip_slot(ch, item);
        if (target >= 0 && target < C_EQUIP_SLOTS) {
            void* occupied = fn_get_equip_item(ch, target);
            if (occupied != nullptr) {
                fn_unequip(ch, target);
            }
        }
    }
    int r = fn_equip_item(ch, item);
    return r ? op_ok() : op_err("equip failed");
}

std::string data_op_unequip(int role, int32_t equip_slot) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_unequip == nullptr) return op_err("symbol not resolved");
    if (equip_slot < 0 || equip_slot >= C_EQUIP_SLOTS) return op_err("bad slot");
    int r = fn_unequip(ch, equip_slot);
    return r ? op_ok() : op_err("unequip failed");
}

std::string data_op_switch_player(int32_t slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_set_active_player == nullptr) return op_err("symbol not resolved");
    if (slot < 0 || slot > 2) return op_err("bad slot");
    int r = fn_set_active_player(slot);
    return r ? op_ok() : op_err("switch failed");
}

std::string data_op_teleport(int32_t map_id, int32_t x, int32_t y) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_change_map == nullptr || fn_set_position == nullptr)
        return op_err("symbol not resolved");
    if (map_id > 0) {
        fn_change_map(map_id, x, y, 0);
    } else if (fn_set_position != nullptr) {
        fn_set_position(x, y);
    }
    return op_ok();
}

std::string data_op_remove_item(int32_t category) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_remove_item == nullptr) return op_err("symbol not resolved");
    int r = fn_remove_item(category);
    return r ? op_ok() : op_err("item not found");
}

std::string data_op_learn_action(int role, int32_t action_id, int32_t level) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_learn_action == nullptr) return op_err("symbol not resolved");
    fn_learn_action(ch, action_id, level);
    return op_ok();
}

// ============================================================
// 合法操作层（v0.3.1，玩家游戏内可做的事）
// 签名见 docs/notes/control-capability.md §5.1
// ============================================================

namespace {

void* inventory_item_at(int bag, int slot) {
    if (g_inven == nullptr) return nullptr;
    if (bag < 0 || bag >= 6 || slot < 0 || slot >= 16) return nullptr;
    uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + bag * 0x80;
    return *reinterpret_cast<void**>(bag_slots + slot * 8);
}

}  // namespace

std::string data_op_move(int32_t x, int32_t y) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(0);
    if (ch == nullptr) return op_err("role not found");
    if (fn_search_path == nullptr || fn_move_as_path == nullptr)
        return op_err("symbol not resolved");
    int found = fn_search_path(ch, x, y, 1);
    if (!found) return op_err("no path");
    // 玩家控制态下 MoveAsPath 要求 +0x278 目标非空否则返回 0（frida 实测 +0x2e2=7 时失败）；
    // 清零控制态后 AI 路径可走，但游戏主循环不会自动续走，需循环调用走完全程。
    uint8_t* ctrl = reinterpret_cast<uint8_t*>(ch) + C_CTRL_STATE;
    uint8_t saved = *ctrl;
    *ctrl = 0;
    for (int i = 0; i < 512; ++i) {
        void** path_head = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ch) + C_PATH_LIST);
        if (*path_head == nullptr) break;
        if (!fn_move_as_path(ch)) break;
    }
    *ctrl = saved;
    return op_ok();
}

std::string data_op_use_item(int bag, int slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_consume_item == nullptr) return op_err("symbol not resolved");
    void* item = inventory_item_at(bag, slot);
    if (item == nullptr) return op_err("slot empty");
    if (fn_is_use != nullptr) {
        uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
        int category = fn_get_bit(flags, 15, 6);
        if (!fn_is_use(category)) return op_err("item not usable");
    }
    fn_consume_item(item);
    return op_ok();
}

std::string data_op_discard_item(int bag, int slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_remove_item_direct == nullptr) return op_err("symbol not resolved");
    if (inventory_item_at(bag, slot) == nullptr) return op_err("slot empty");
    fn_remove_item_direct(bag, slot);
    if (inventory_item_at(bag, slot) != nullptr) return op_err("discard failed");
    return op_ok();
}

std::string data_op_sell_item(int bag, int slot, int64_t price) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_remove_item_direct == nullptr || fn_add_money == nullptr)
        return op_err("symbol not resolved");
    if (inventory_item_at(bag, slot) == nullptr) return op_err("slot empty");
    fn_remove_item_direct(bag, slot);
    if (inventory_item_at(bag, slot) != nullptr) return op_err("sell failed");
    fn_add_money(price);
    return op_ok();
}

std::string data_op_include_party(int mercenary_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_include_party == nullptr || fn_get_party_size == nullptr || fn_get_member == nullptr)
        return op_err("symbol not resolved");
    void* ch = find_char_by_merc_slot(mercenary_slot);
    if (ch == nullptr) return op_err("mercenary not found");
    // 前置校验（避免触发游戏弹窗）：目标已在队则不调游戏函数，其次检查满员
    for (int i = 0; i < 3; ++i) {
        if (fn_get_member(i) == ch) return op_err("already in party");
    }
    if (fn_get_party_size() >= 3) return op_err("party full");
    int r = fn_include_party(ch);
    return r ? op_ok() : op_err("party full or include failed");
}

std::string data_op_exclude_party(int mercenary_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_exclude_party == nullptr || fn_get_member == nullptr)
        return op_err("symbol not resolved");
    void* ch = find_char_by_merc_slot(mercenary_slot);
    if (ch == nullptr) return op_err("mercenary not found");
    // 前置校验（避免触发游戏弹窗/破坏剧情）：主控或任务特殊 NPC 不能离队
    if (fn_get_member(0) == ch) return op_err("cannot exclude leader");
    if (fn_is_special_npc != nullptr && fn_is_special_npc(ch)) return op_err("cannot exclude quest npc");
    int r = fn_exclude_party(ch);
    return r ? op_ok() : op_err("exclude failed");
}

// ============================================================
// 事件流（/api/events，轮询差异检测，零 hook）
// 每次调用对比上次快照生成事件；无后台线程、无 inline hook。
// ============================================================

namespace {

struct Snapshot {
    int64_t money;
    int16_t x, y;
    int32_t hp[3], mp[3], level[3];
    int64_t exp[3];
    int inv_count;
};

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
