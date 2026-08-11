#include "game_data.h"

#include "game_access.h"
#include "game_symbols.h"

#include <android/log.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

#define MOVE_TAG "Inotia4Move"
#define MOVE_LOG(...) __android_log_print(ANDROID_LOG_INFO, MOVE_TAG, __VA_ARGS__)

// json_escape 定义于全局作用域（~L510），前向声明放全局，供匿名 namespace 内 member_json 使用
std::string json_escape(const char* s);

// game_in_world 检查是否处于游戏中（state==5），定义在全局作用域供 data_*_json/data_op_* 使用
bool game_in_world() {
    return g_state != nullptr && *reinterpret_cast<uint16_t*>(g_state) == 5;
}

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
        if (fn_get_next_exp != nullptr) {
            s += ",\"expNext\":" + std::to_string(fn_get_next_exp(ch));
        }
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
    if (fn_get_stat_base != nullptr) {
        s += ",\"baseStats\":[";
        for (int a = 0; a < 5; ++a) {
            if (a > 0) s += ",";
            s += std::to_string(fn_get_stat_base(ch, a));
        }
        s += "]";
    }
    if (fn_get_stat_bonus != nullptr) {
        s += ",\"bonusStats\":[";
        for (int a = 0; a < 5; ++a) {
            if (a > 0) s += ",";
            s += std::to_string(fn_get_stat_bonus(ch, a));
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
    s += "]";
    if (fn_get_name != nullptr) {
        s += ",\"name\":\"" + json_escape(fn_get_name(ch)) + "\"";
    }
    s += "}";
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
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
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
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
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
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
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
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{";
    s += "\"mapId\":" + std::to_string(g_map_id != nullptr ? *reinterpret_cast<uint16_t*>(g_map_id) : -1);
    append_position(s, lead_member());
        // 瓦片通行查询（P0#3：MAP_IsBlocking 反汇编确认，GOT *(0x2f3f48) 双层解引用，y*64+x 索引，bit3=阻挡）
        if (g_base != 0) {
            uint8_t* tiles = *reinterpret_cast<uint8_t**>(g_base + G_TILE_GOT_VMA);
            if (tiles != nullptr) {
            uint8_t* lead = reinterpret_cast<uint8_t*>(lead_member());
            if (lead != nullptr) {
                int px = *reinterpret_cast<int16_t*>(lead + C_POS_X);
                int py = *reinterpret_cast<int16_t*>(lead + C_POS_Y);
                int tx = px >> 4, ty = py >> 4;
                if (tx >= 0 && ty >= 0) {
                    int idx = ty * TILE_ROW_STRIDE + tx;
                    s += ",\"tile\":{\"tx\":" + std::to_string(tx) + ",\"ty\":" + std::to_string(ty);
                    s += ",\"blocking\":" + std::string(((tiles[idx] & TILE_BLOCK_BIT) ? "true" : "false")) + "}";
                }
            }
            // v0.4.25 出口区域：扫描瓦片网格 bit7=1 的 tile（GAMEPLAY_CheckMapLink 同网格同索引）
            //   —— 出口供移动/切图使用（模块 move/walk 已按此触发 GoMapLink）
            {
                std::string ex;
                for (int yy = 0; yy < 64; ++yy) {
                    for (int xx = 0; xx < 64; ++xx) {
                        if (tiles[yy * TILE_ROW_STRIDE + xx] & 0x80) {
                            if (!ex.empty()) ex += ",";
                            ex += "{\"tx\":" + std::to_string(xx) + ",\"ty\":" + std::to_string(yy);
                            ex += ",\"px\":" + std::to_string(xx << 4) + ",\"py\":" + std::to_string(yy << 4) + "}";
                        }
                    }
                }
                s += ",\"exits\":[" + ex + "]";
            }
        }
    }
    s += "}";
    return s;
}

std::string data_units_json() {
    // CHARSYSTEM 角色对象池：*(G_CHAR_POOL_VMA) 指向英雄对象，对象按 C_OBJ_SIZE 步长连续排列
    // （frida 实测 2026-08-05：31 有效单位 = 3 队伍 + 怪物 + NPC，坐标与玩家同像素坐标系）。
    // 有效性：type 0-2、status<=2、坐标 0-1500（未激活槽哨兵值 2048/16992/status>2，frida 实测排除）。
    // status: 0=队伍 1=城镇NPC/佣兵 2=怪物/召唤物。
    // v0.4.25：type==2 为装饰/场景单位（火把/火堆/地图出口/士兵/商人，frida 实测 map 3080 全部 type=2）
    //   —— units 仅保留可交互/战斗单位（type 0-1），装饰物过滤（出口改由 map exits 字段提供）。
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
                if (type < 0 || type > 1) continue;  // v0.4.25 过滤 type==2 装饰物
                if (status > 2) continue;
                if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) continue;
                if (emitted > 0) s += ",";
                s += "{\"slot\":" + std::to_string(i);
                s += ",\"x\":" + std::to_string(x);
                s += ",\"y\":" + std::to_string(y);
                s += ",\"type\":" + std::to_string(type);
                s += ",\"status\":" + std::to_string(status);
                s += ",\"level\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_LEVEL]));
                s += ",\"hp\":" + std::to_string(*reinterpret_cast<int32_t*>(obj + C_HP));
                s += ",\"mp\":" + std::to_string(*reinterpret_cast<int32_t*>(obj + C_MP));
                if (fn_get_name != nullptr) {
                    char* nm = fn_get_name(obj);
                    s += ",\"name\":" + (nm != nullptr ? "\"" + json_escape(nm) + "\"" : "null");
                } else {
                    s += ",\"name\":null";
                }
                s += "}";
                ++emitted;
            }
        }
        // CHARLOC 位置登记池（CHARLOCSYSTEM，10B/条：+0 f0, +2 x u16, +4 y u16）——P0#2 逆向产出
        // 注：+0 字段语义待确认（data-sources §2.6 标注为地图ID，CHARLOCSYSTEM_Add 反汇编为 a0）
        uint8_t* cl_pool = *reinterpret_cast<uint8_t**>(g_base + G_CHARLOC_POOL_VMA);
        uint16_t cl_count = *reinterpret_cast<uint16_t*>(g_base + G_CHARLOC_COUNT_VMA);
        s += "]";  // 闭合 units 数组
        if (cl_pool != nullptr && cl_count > 0 && cl_count <= 512) {
            s += ",\"charLoc\":[";
            for (int i = 0; i < cl_count; ++i) {
                uint8_t* loc = cl_pool + i * CHARLOC_SIZE;
                if (i > 0) s += ",";
                s += "{\"f0\":" + std::to_string(static_cast<int>(loc[0]));
                s += ",\"x\":" + std::to_string(*reinterpret_cast<uint16_t*>(loc + 2));
                s += ",\"y\":" + std::to_string(*reinterpret_cast<uint16_t*>(loc + 4)) + "}";
            }
            s += "]";
        }
    }
    s += "}";
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
        int32_t i32type = *reinterpret_cast<int32_t*>(g_base + G_POPUP_TYPE_VMA);
        int32_t i32disp  = *reinterpret_cast<int32_t*>(g_base + G_POPUP_DISPTYPE_VMA);
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

int64_t data_frame_count() {
    if (g_base == 0) return -1;
    // [0x2f5648] GOT 槽：先解引用取 u64 指针，再读计数
    uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
    uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
    return cnt != nullptr ? static_cast<int64_t>(*cnt) : -1;
}

std::string data_gamestate_json() {
    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    uint16_t prev = g_prev_state != nullptr ? *reinterpret_cast<uint16_t*>(g_prev_state) : 0xFFFF;
    uint32_t gs = g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
    uint8_t init = g_initstate != nullptr ? *reinterpret_cast<uint8_t*>(g_initstate) : 0;
    uint8_t popup_on = g_popup_on != nullptr ? *reinterpret_cast<uint8_t*>(g_popup_on) : 0;

    const char* screen = "loading";
    if (state == 4) {
        // state==4（主菜单）：读 popup 栈区分标题屏/存档选择/职业选择（v0.4.18 修复）
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
                    if (vma == 0x14c720) panel = "save_slot";
                    else if (vma == 0x14d670) panel = "character_select";
                    else if (vma == 0x16f050) panel = "daily_reward";
                    else if (vma == 0x14be20) panel = "options";
                    else if (vma == 0x14fb38) panel = "settings";
                }
            }
        }
        screen = panel ? panel : "main_menu";
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

    // 帧计数：FPS 系统每帧 +1（0x3075f0 u64，FPS_getTotalFrameCount 官方读取）
    uint64_t frame = 0;
    if (g_base != 0) {
        uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
        uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
        if (cnt != nullptr) frame = *cnt;
    }

    std::string result = "{\"screen\":\"" + std::string(screen) + "\",\"frame\":" + std::to_string(frame) +
                         ",\"dialogActive\":" + (popup_on ? "true" : "false");
    if (popup_on && g_base != 0) {
        std::string dtext;
        uint8_t* pt = *reinterpret_cast<uint8_t**>(g_base + G_POPUP_TEXT_VMA);
        if (pt != nullptr) {
            for (int i = 0; i < 256 && pt[i] != 0; ++i) dtext += static_cast<char>(pt[i]);
        }
        bool has_ok = *reinterpret_cast<uint64_t*>(g_base + G_POPUP_FPOK_VMA) != 0;
        bool has_cancel = *reinterpret_cast<uint64_t*>(g_base + G_POPUP_FPCANCEL_VMA) != 0;
        std::string esc;
        for (char c : dtext) {
            if (c == '"' || c == '\\') esc += '\\';
            else if (c == '\n') esc += "\\n";
            else if (c == '\r') esc += "\\r";
            else if (c == '\t') esc += "\\t";
            else esc += c;
        }
        // 按钮文本：按钮绘制 ID 指向资源表（ControlButton_SetDrawID），读内存需深挖控件树；
        // 此处按弹窗类型推导固定文本（v0.3.12 真机验证：出售弹窗 popupType=1=是/否、保存成功 popupType=0 无按钮）
        int32_t ptype = *reinterpret_cast<int32_t*>(g_base + G_POPUP_TYPE_VMA);
        const char* buttons = "[]";
        if (ptype == 1) buttons = "[\"是\",\"否\"]";
        else if (has_ok) buttons = "[\"确认\"]";
        result += ",\"dialog\":{\"text\":\"" + esc + "\",\"hasOk\":" + (has_ok ? "true" : "false") +
                  ",\"hasCancel\":" + (has_cancel ? "true" : "false") + ",\"buttons\":" + buttons + "}";
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
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
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

    // 帧计数：FPS 系统每帧 +1（0x3075f0 u64，FPS_getTotalFrameCount 官方读取）
    uint64_t frame = 0;
    if (g_base != 0) {
        uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
        uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
        if (cnt != nullptr) frame = *cnt;
    }
    s += "\"frame\":" + std::to_string(frame);

    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    s += ",\"screen\":";
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
// 调用前检查 STATE_nState==5（游戏中）——简化假设（为减少开发难度与测试广度，
// 非逐操作实证的硬性要求；如需支持非 world 状态可去除后多状态验证，见 control-capability §0）；
// 直接调用游戏函数（函数内部连续执行无阻塞点，读操作已真机验证跨线程安全）。
// 返回 JSON：{"ok":true,...} 或 {"ok":false,"error":"..."}
// ============================================================

namespace {

std::string op_ok() { return "{\"ok\":true}"; }

std::string op_err(const char* msg) {
    return std::string("{\"ok\":false,\"error\":\"") + msg + "\"}";
}

// 前向声明（定义在文件后部匿名 namespace）
void* inventory_item_at(int bag, int slot);

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

std::string data_op_add_item(int32_t category, int32_t count) {
    if (!game_in_world()) return op_err("not in game");
    if (count <= 0) return op_err("bad count");
    if (fn_create_item == nullptr || fn_inven_save_item == nullptr || fn_inven_find_save_slot == nullptr)
        return op_err("symbol not resolved");
    // ITEMSYSTEM_CreateItem 创建物品对象（MakeItem 带 search_tbl 校验会失败，CreateItem 无此限制）
    void* item = fn_create_item(category, 0, 0, 0);
    if (item == nullptr) return op_err("create item failed");
    // 数量位域 [item+0x10] bit25-31 语义：0=不可堆叠、100=装备、1~99=可堆叠（上限99）。
    // CreateItem 已设好标记值（装备=100/不可堆叠=0），只在可堆叠物品上覆盖为请求数量。
    uint32_t cf = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT);
    int base_count = fn_get_bit(static_cast<int>(cf), 31, 25);
    if (count > 1 && (base_count < 1 || base_count > 99))
        return op_err("item not stackable");
    if (count > 1 && base_count >= 1 && base_count <= 99) {
        if (count > 99) count = 99;
        cf &= ~(0x7F800000u);
        cf |= (static_cast<uint32_t>(count) << 25);
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT) = cf;
    }
    int save_slot = fn_inven_find_save_slot(item, 0);
    if (save_slot <= 0) return op_err("inventory full");
    if (!fn_inven_save_item(item, nullptr)) return op_err("save item failed");
    return op_ok();
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

std::string data_op_set_level(int role, int32_t level) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_level == nullptr) return op_err("symbol not resolved");
    if (level < 1 || level > 255) return op_err("level 1-255");
    // CHAR_SetLevel 完整结算：写 [ch+0xe] + 重算 nextExp + InitializeFromLevel + 升级加能力点/技能点 + 回满血蓝
    int ok = fn_set_level(ch, level);
    return ok ? op_ok() : op_err("level down not allowed");
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

// 合法加点（v0.4.5）：属性+1 / 能力点-1（游戏 StatDivide 语义，绕过 UI 面板缓冲直接操作角色）
std::string data_op_add_stat(int role, int32_t attr) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_get_status_point == nullptr || fn_get_stat_main == nullptr || fn_set_stat_main == nullptr ||
        fn_set_status_point == nullptr)
        return op_err("symbol not resolved");
    if (attr < 0 || attr > 4) return op_err("bad attr");
    int32_t points = fn_get_status_point(ch);
    if (points <= 0) return op_err("no status point");
    int32_t cur = fn_get_stat_main(ch, attr);
    fn_set_stat_main(ch, attr, cur + 1);
    fn_set_status_point(ch, points - 1);
    return op_ok();
}

// 合法属性重置（v0.4.6）：属性归零 + 能力点按等级还原（游戏 CHAR_InitializeStatus 语义，用户确认保持合法）
std::string data_op_stat_reset(int role) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_initialize_status == nullptr) return op_err("symbol not resolved");
    fn_char_initialize_status(ch);
    return op_ok();
}

// 合法技能重置（v0.4.11）：移除技能链表非基础技能 + 技能点按职业还原（CHAR_InitializeSkill 语义，与 stat-reset 同级合法）
std::string data_op_skill_reset(int role) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_initialize_skill == nullptr) return op_err("symbol not resolved");
    fn_char_initialize_skill(ch);
    return op_ok();
}

// 释放技能（v0.4.12 修正）：CHAR_SetActionID 第 3 参是目标对象指针非 level（技能动作读目标坐标算朝向）。
// 用 CHAR_GetEnemyTarget 取游戏正规目标，无目标安全返回（规避两次崩溃：DrawFocus 野指针 / SetAction 读 level 当目标）
std::string data_op_cast(int role, int32_t action_id) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_get_enemy_target == nullptr || fn_char_set_action_id == nullptr)
        return op_err("symbol not resolved");
    // 校验技能已学（遍历 +0x2A0 技能链表）
    uint8_t* node = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(ch) + C_SKILL_LIST);
    bool learned = false;
    int count = 0;
    while (node != nullptr && count < 64) {
        if (*reinterpret_cast<uint16_t*>(node + S_ACTION_ID) == static_cast<uint16_t>(action_id)) {
            learned = true;
            break;
        }
        node = *reinterpret_cast<uint8_t**>(node + S_NEXT);
        ++count;
    }
    if (!learned) return op_err("skill not learned");
    // 获取合法敌人目标（无目标不释放）
    void* target = fn_char_get_enemy_target(ch, 0, 0);
    if (target == nullptr) return op_err("no target");
    fn_char_set_action_id(ch, action_id, target);
    return op_ok();
}

// 放弃任务（v0.4.15）：QUESTSYSTEM_Find 按 questId 找槽 → RemoveSlot 删除（通用实现，替代硬编码 489 的 RefuseReview）
std::string data_op_quest_quit(int32_t quest_id) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_questsystem_find == nullptr || fn_questsystem_remove_slot == nullptr)
        return op_err("symbol not resolved");
    int slot = fn_questsystem_find(quest_id);
    if (slot < 0) return op_err("quest not found");
    int r = fn_questsystem_remove_slot(slot);
    return r ? op_ok() : op_err("quest quit failed");
}

// 手动保存（v0.4.16）：SAVE_Save() 无参静默保存（内部校验 + 全量序列化，无弹窗）
std::string data_op_save() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_save == nullptr) return op_err("symbol not resolved");
    int r = fn_save();
    return r ? op_ok() : op_err("save failed");
}

// 回到主菜单（v0.4.18）：GAMESTATE_SetState(4) 游戏正规状态切换（任意界面→main_menu，无弹窗/无 UI 依赖）
std::string data_op_main_menu() {
    if (fn_gamestate_set_state == nullptr) return op_err("symbol not resolved");
    fn_gamestate_set_state(4);
    return op_ok();
}

// 直接进入指定存档槽（v0.4.18）：复现官方 SaveSlot_SlotButtonExe 链（UI_SetPopupProcessInfo(4,0) + 清标志 + GAME_StartResumeGame(slot)）
std::string data_op_enter_slot(int32_t slot) {
    bool in_world = g_state != nullptr && *reinterpret_cast<uint16_t*>(g_state) == 5;
    if (in_world) return op_err("already in game");
    if (fn_save_get_save_slot == nullptr || fn_ui_set_popup_process_info == nullptr ||
        fn_game_start_resume_game == nullptr || fn_save_create_save_slot == nullptr)
        return op_err("symbol not resolved");
    if (slot < 0 || slot > 2) return op_err("bad slot");
    // 先初始化槽区（SAVE_CreateSaveSlot 循环加载 3 槽存档到内存），否则 b0/b2 全 0 误判空槽
    fn_save_create_save_slot();
    void* slot_struct = fn_save_get_save_slot(slot);
    if (slot_struct == nullptr) return op_err("bad slot");
    uint8_t b0 = *reinterpret_cast<uint8_t*>(slot_struct);
    uint8_t b2 = *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(slot_struct) + 2);
    if (b0 == 0 && b2 == 0) return op_err("slot empty");
    fn_ui_set_popup_process_info(4, 0);
    uint8_t** flag_ptr = reinterpret_cast<uint8_t**>(g_base + 0x2f6000 + 0x8);
    if (*flag_ptr != nullptr) **flag_ptr = 0;
    int r = fn_game_start_resume_game(slot);
    return r ? op_ok() : op_err("enter slot failed");
}

// 恢复被阻断的 Hive 支付流程（v0.4.19）：
// Java hook 阻断 SelectTarget.iapSelectTarget 后调用。进档链中 UIPlay_CallInAppShopProc(0xc7b64)
// 会置 [0x2f6000+0xc48]=0（HUD 绘制总开关）+ 弹 daily_reward(0x1a) + 触发支付；支付被 hook 跳过
// 后开关不恢复导致 world 无 HUD 卡死。这里模拟支付流程结束：复位每日奖励触发标志、
// 恢复 HUD 开关、清掉 daily_reward 面板，使 GAMESTATE_DrawPlay 正常绘制 HUD。
std::string data_recover_after_hive_block() {
    if (g_base == 0) return op_err("base not ready");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    if (fn_networkstore_set_state == nullptr) return op_err("symbol not resolved");
    uint32_t** daily_trigger = reinterpret_cast<uint32_t**>(g_base + 0x2f5000 + 0xff8);
    if (*daily_trigger != nullptr) **daily_trigger = 1;
    uint8_t** hud_gate = reinterpret_cast<uint8_t**>(g_base + 0x2f6000 + 0xc48);
    if (*hud_gate != nullptr) **hud_gate = 1;
    fn_networkstore_set_state(0);
    fn_ui_set_popup_process_info(4, 0);
    return op_ok();
}

// 存档槽信息（v0.4.18）：读 3 槽存在标志 + 主控角色等级（SAVESLOT_GetHero 取英雄 → +0xe 等级）
std::string data_save_slots_json() {
    if (fn_save_get_save_slot == nullptr || fn_saveslot_get_hero == nullptr)
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
            s += ",\"heroLevel\":" + std::to_string(level) + ",\"heroIndex\":" + std::to_string(hero_idx);
        }
        s += "}";
    }
    s += "]}";
    return s;
}

// NPC 交互（v0.4.13）：PLAYER_DoCheckNearNPC 设 PLAYER_pNearNPC → UINpc_InitNPC() 建对话
std::string data_op_npc_interact() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_player_check_near_npc == nullptr || fn_uinpc_init == nullptr)
        return op_err("symbol not resolved");
    fn_player_check_near_npc();
    void* near_npc = *reinterpret_cast<void**>(g_base + G_PLAYER_NEAR_NPC_VMA);
    if (near_npc == nullptr) return op_err("no npc nearby");
    uint8_t r = fn_uinpc_init();
    return r ? op_ok() : op_err("interact failed");
}

// 对话选项数据（读 UICHOICE 全局，供 GET 端点返回候选）
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

// 对话下一步（v0.4.13）：NPCTASKLIST_MakeDlg 返回下一句文本
std::string data_op_npc_dialog_next() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_npctasklist_make_dlg == nullptr) return op_err("symbol not resolved");
    char* text = fn_npctasklist_make_dlg();
    if (text == nullptr) return op_err("no dialog");
    return "{\"ok\":true,\"text\":\"" + json_escape(text) + "\"}";
}

// 对话选项选择（v0.4.13）：写 NPCTASKLIST_nIndex → UINpc_ExeCurrentNpcTask
std::string data_op_npc_dialog_select(int index) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_uinpc_exe_current_task == nullptr) return op_err("symbol not resolved");
    uint8_t count = *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_COUNT_VMA);
    if (index < 0 || index >= count) return op_err("bad index");
    *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_INDEX_VMA) = static_cast<uint8_t>(index);
    fn_uinpc_exe_current_task();
    return op_ok();
}

// 镶嵌宝石（v0.4.6）：宝石从背包镶入装备插槽（ITEMSYSTEM_PutJewel），成功后手动消耗宝石物品防刷
std::string data_op_jewel(int role, int bag, int slot, int equip_slot) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_put_jewel == nullptr || fn_is_jewel == nullptr || fn_remove_item_direct == nullptr)
        return op_err("symbol not resolved");
    if (bag < 0 || bag >= 6 || slot < 0 || slot >= 16) return op_err("bad slot");
    if (equip_slot < 0 || equip_slot >= C_EQUIP_SLOTS) return op_err("bad equip slot");
    void* jewel = inventory_item_at(bag, slot);
    if (jewel == nullptr) return op_err("jewel not found");
    void* equip = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ch) + C_EQUIP + equip_slot * 8);
    if (equip == nullptr) return op_err("equip slot empty");
    int r = fn_put_jewel(equip, jewel);
    if (r != 0) return r == 2 ? op_err("no socket") : op_err("not jewel");
    // PutJewel 不消耗宝石物品本身，镶嵌成功后手动删除背包中的宝石（防刷宝石）
    fn_remove_item_direct(bag, slot);
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

// 战斗 AI 技能开关（v0.4.10）：写 [ch+0x3a0] bit0-2（CHAR_GetSkillUsage 语义）
std::string data_op_set_skill_usage(int role, int32_t onoff) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_skill_usage == nullptr) return op_err("symbol not resolved");
    fn_set_skill_usage(ch, onoff ? 1 : 0);
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
    if (fn_remove_item == nullptr || fn_get_bit == nullptr) return op_err("symbol not resolved");
    // 按类别删第一个匹配物品（INVEN_RemoveItem 按 item 指针删，需先按类别定位）
    for (int b = 0; b < 6; ++b) {
        for (int j = 0; j < 16; ++j) {
            void* item = inventory_item_at(b, j);
            if (item == nullptr) continue;
            uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
            if (fn_get_bit(flags, 15, 6) == category) {
                int r = fn_remove_item(item);
                return r ? op_ok() : op_err("item not found");
            }
        }
    }
    return op_err("item not found");
}

// 商店商品列表（v0.4.14）：遍历 DEALSYSTEM_pSaleList（48 槽×16B，GOT 0x2f3000+0x490 指向表基址）
std::string data_shop_items_json() {
    if (g_base == 0 || fn_item_get_buy_price == nullptr || fn_get_bit == nullptr) return "{\"items\":[]}";
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
        uint32_t count = fn_get_bit(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT), 31, 25);
        int price = fn_item_get_buy_price(item);
        if (!first) s += ",";
        first = false;
        s += "{\"slot\":" + std::to_string(i) + ",\"category\":" + std::to_string(category) +
             ",\"count\":" + std::to_string(count) + ",\"price\":" + std::to_string(price) + "}";
    }
    s += "]}";
    return s;
}

// 商店购买（v0.4.14）：商品定位（按槽）→ 金币校验 → INVEN_FindSaveSlot + INVEN_SaveItem 存入 → 扣款
std::string data_op_shop_buy(int32_t slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_item_get_buy_price == nullptr || fn_get_money == nullptr || fn_minus_money == nullptr ||
        fn_inven_find_save_slot == nullptr || fn_inven_save_item == nullptr)
        return op_err("symbol not resolved");
    if (slot < 0 || slot >= 48) return op_err("bad slot");
    uint8_t* sale_list = reinterpret_cast<uint8_t*>(*(reinterpret_cast<void**>(g_base + G_DEALSYSTEM_SALE_LIST_VMA)));
    if (sale_list == nullptr) return op_err("no shop");
    uint8_t* slot_ptr = sale_list + slot * 16;
    uint64_t flags = *reinterpret_cast<uint64_t*>(slot_ptr);
    if (flags & 1) return op_err("item sold out");
    void* item = *reinterpret_cast<void**>(slot_ptr + 8);
    if (item == nullptr) return op_err("item not found");
    int price = fn_item_get_buy_price(item);
    int64_t money = fn_get_money();
    if (money < price) return op_err("not enough money");
    int save_slot = fn_inven_find_save_slot(item, 0);
    if (save_slot <= 0) return op_err("inventory full");
    if (!fn_inven_save_item(item, nullptr)) return op_err("buy failed");
    fn_minus_money(price);
    return "{\"ok\":true,\"price\":" + std::to_string(price) + "}";
}

std::string data_op_learn_action(int role, int32_t action_id, int32_t level) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_learn_action == nullptr) return op_err("symbol not resolved");
    fn_learn_action(ch, action_id, level);
    return op_ok();
}

std::string data_op_party_swap(int32_t a, int32_t b) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_party_swap == nullptr) return op_err("symbol not resolved");
    if (a < 0 || a > 2 || b < 0 || b > 2) return op_err("bad slot");
    fn_party_swap(a, b);
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

// 背包快照 diff：开箱/解封后新增物品或堆叠数量增加的槽位
// 返回 JSON 数组 [{"bag":b,"slot":j,"category":c,"count":n},...]
// 新指针出现=新物品（count 位域可能为 0——不可堆叠物品无数量字段，此时报 1）；同槽同指针且 count 增加=堆叠合并
std::string inventory_gained_json(void* const* before) {
    std::string s;
    int n = 0;
    for (int b = 0; b < 6; ++b) {
        uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * 0x80;
        for (int j = 0; j < 16; ++j) {
            void* item = *reinterpret_cast<void**>(bag_slots + j * 8);
            if (item == nullptr) continue;
            uint32_t cf = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT);
            int count = fn_get_bit(static_cast<int>(cf), 31, 25);
            void* old = before[b * 16 + j];
            if (old == item) {
                uint32_t of = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(old) + I_COUNT);
                count -= fn_get_bit(static_cast<int>(of), 31, 25);
                if (count <= 0) continue;
            } else if (old != nullptr) {
                continue;  // 同槽不同指针：旧物品被消耗/替换，非新增
            }
            // 数量位域归一化：0=不可堆叠、100=装备 → 报 1 件；1~99 为实际堆叠数
            if (count == 0 || count == 100) count = 1;
            if (n > 0) s += ",";
            uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
            s += "{\"bag\":" + std::to_string(b);
            s += ",\"slot\":" + std::to_string(j);
            s += ",\"category\":" + std::to_string(fn_get_bit(flags, 15, 6));
            s += ",\"count\":" + std::to_string(count) + "}";
            ++n;
        }
    }
    return s;
}

// ---- 通用帧任务管理器（v0.4.26）----
// 官方路径由游戏主循环每帧驱动 1 次移动（PressKeyPlay→CHAR_Move / CHAR_Process→MoveAsPath）。
// 模块同步循环瞬时走完全程导致"闪现"（无逐帧动画）。通用管理器：单后台线程按游戏帧率
// （~16.9fps ≈ 59ms/帧）遍历任务列表，每帧调 1 次任务回调，回调返回 false 自动移除。
// 可注册任意逐帧操作（move/walk/自动战斗/跟随），单任务语义（注册即替换旧任务）。
struct FrameTask {
    bool (*fn)(void*);  // 返回 true 继续，false 完成
    void* ctx;          // 任务上下文（角色指针/方向/剩余帧等）
    int id;
};

std::mutex g_task_mtx;
std::vector<FrameTask> g_tasks;
std::thread g_task_thread;
std::atomic<bool> g_task_running{false};
std::atomic<bool> g_task_stop{false};
int g_task_next_id = 1;

void task_thread_fn();

// 注册帧任务，返回任务 id（0=失败）
int frame_task_register(bool (*fn)(void*), void* ctx) {
    std::lock_guard<std::mutex> lock(g_task_mtx);
    if (fn == nullptr || !game_in_world()) return 0;
    g_tasks.clear();  // 单任务语义：注册即替换
    FrameTask t{fn, ctx, g_task_next_id++};
    g_tasks.push_back(t);
    if (!g_task_running.load()) {
        if (g_task_thread.joinable()) g_task_thread.join();  // 等旧线程完全退出
        g_task_running.store(true);
        g_task_thread = std::thread(task_thread_fn);
    }
    return t.id;
}

// 注销帧任务（id<=0 注销全部）
void frame_task_unregister(int id) {
    std::lock_guard<std::mutex> lock(g_task_mtx);
    if (id <= 0) {
        g_tasks.clear();
        return;
    }
    for (auto it = g_tasks.begin(); it != g_tasks.end(); ++it) {
        if (it->id == id) { g_tasks.erase(it); return; }
    }
}

// 停止所有任务并等待线程退出（walk_stop/新任务注册时由 register 内部调用）
void stop_all_tasks() {
    g_task_stop.store(true);
    if (g_task_thread.joinable()) g_task_thread.join();
    g_task_stop.store(false);
    std::lock_guard<std::mutex> lock(g_task_mtx);
    g_tasks.clear();
}

void task_thread_fn() {
    const auto frame = std::chrono::milliseconds(59);
    int step = 0;
    for (;;) {
        if (g_task_stop.load()) break;
        std::vector<FrameTask> snapshot;
        {
            std::lock_guard<std::mutex> lock(g_task_mtx);
            if (g_tasks.empty()) break;
            snapshot = g_tasks;
        }
        for (const FrameTask& t : snapshot) {
            bool cont = t.fn(t.ctx);
            MOVE_LOG("task %d step %d cont=%d", t.id, step++, cont);
            if (!cont) frame_task_unregister(t.id);
        }
        std::this_thread::sleep_for(frame);
    }
    g_task_running.store(false);
}

// 每步切图出口检测（命中→GoMapLink 切图，终止任务）
bool map_link_check(void* ch) {
    if (fn_go_map_link_by_char == nullptr || ch == nullptr) return false;
    int16_t px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_X);
    int16_t py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_Y);
    return fn_go_map_link_by_char(ch, px >> 4, py >> 4) != 0;
}

// ---- move 任务回调：每帧 MoveAsPath 走 1 步 ----
// ctx = 角色指针（void*）
bool move_task_tick(void* ctx) {
    void* ch = ctx;
    if (fn_move_as_path == nullptr || ch == nullptr) return false;
    // 玩家控制态下 MoveAsPath 要求 +0x278 目标非空否则返回 0；
    // 清零控制态让 AI 路径可走（模块线程单驱动，无双竞争）
    uint8_t* ctrl = reinterpret_cast<uint8_t*>(ch) + C_CTRL_STATE;
    uint8_t saved = *ctrl;
    *ctrl = 0;
    bool ok = fn_move_as_path(ch);
    *ctrl = saved;
    void** path_head = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ch) + C_PATH_LIST);
    if (!ok || *path_head == nullptr) return false;  // 走完/失败
    return !map_link_check(ch);  // 命中出口→切图，终止
}

// ---- walk 任务上下文与回调：每帧 CHAR_Move(flag=0) 走 1 步，累计 60 帧 ----
struct WalkCtx {
    void* ch;
    int dir;
    int remaining;
};

bool walk_task_tick(void* ctx) {
    WalkCtx* w = static_cast<WalkCtx*>(ctx);
    if (fn_char_move == nullptr || w == nullptr) return false;
    // flag=0：CHAR_Move 内部自动 MAP_SetFocus 跟随摄像机。
    // 返回值：0=正常走一步（成功），非 0=撞墙/阻挡（反汇编 e98dc mov w20,#0x1）
    if (fn_char_move(w->ch, w->dir, 8, 0)) return false;  // 撞墙/不可走
    return --w->remaining > 0 && !map_link_check(w->ch);  // 走完 60 帧或切图终止
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
    // 注册帧任务：每帧（59ms）MoveAsPath 走 1 步，逐帧移动（v0.4.26）
    if (frame_task_register(move_task_tick, ch) == 0) return op_err("move start failed");
    return op_ok();
}

std::string data_op_walk(int32_t direction) {
    if (!game_in_world()) return op_err("not in game");
    if (direction < 0 || direction > 3) return op_err("bad direction");
    void* ch = member_or_null(0);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_move == nullptr) return op_err("symbol not resolved");
    // 注册帧任务：每帧（59ms）CHAR_Move(flag=0) 走 1 步累计 60 帧（v0.4.26）
    // 单任务语义：WalkCtx 用静态实例（同时只有一个任务），注册即重置
    static WalkCtx walk_ctx;
    walk_ctx.ch = ch;
    walk_ctx.dir = direction;
    walk_ctx.remaining = 60;
    if (frame_task_register(walk_task_tick, &walk_ctx) == 0) return op_err("walk start failed");
    return op_ok();
}

std::string data_op_walk_stop() {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(0);
    if (ch == nullptr) return op_err("role not found");
    stop_all_tasks();
    if (fn_char_remove_path == nullptr) return op_err("symbol not resolved");
    fn_char_remove_path(ch);
    return op_ok();
}

std::string data_op_dialog_ok() {
    if (g_base == 0) return op_err("libgame not loaded");
    if (*reinterpret_cast<uint8_t*>(g_base + G_POPUP_ON_VMA) == 0) return op_err("no dialog");
    reinterpret_cast<void (*)()>(g_base + F_BUTTON_OK_EXE_VMA)();
    return op_ok();
}

std::string data_op_dialog_cancel() {
    if (g_base == 0) return op_err("libgame not loaded");
    if (*reinterpret_cast<uint8_t*>(g_base + G_POPUP_ON_VMA) == 0) return op_err("no dialog");
    reinterpret_cast<void (*)()>(g_base + F_BUTTON_CANCEL_EXE_VMA)();
    return op_ok();
}

std::string data_op_use_item(int bag, int slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_get_bit == nullptr) return op_err("symbol not resolved");
    void* item = inventory_item_at(bag, slot);
    if (item == nullptr) return op_err("slot empty");
    uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
    int category = fn_get_bit(flags, 15, 6);
    // 解封/开箱/骰子类物品 fn_is_use 返回 0（IsUseAfterConfirm 判定集不含这些类别），
    // 但它们有自己的独立使用路径，不受 fn_is_use 限制
    bool sealed_or_box = (fn_is_sealed != nullptr && fn_is_sealed(category)) ||
                         (fn_is_item_box != nullptr && fn_is_item_box(category)) ||
                         (fn_is_dice != nullptr && fn_is_dice(category));
    if (!sealed_or_box && fn_is_use != nullptr && !fn_is_use(category))
        return op_err("item not usable");

    void* leader = member_or_null(0);

    // 按 UIEquip_SetDescMenu 按钮判定链分派（权威：反汇编 UI 按钮显隐逻辑）
    // 骰子（0x34-0x38）— 两段式：掷骰只生成 pending 返回变化量（不应用），接受/拒绝由 dice-accept/dice-reject 端点处理
    if (fn_is_dice != nullptr && fn_is_dice(category)) {
        if (fn_status_dice_roll == nullptr || fn_get_stat_base == nullptr)
            return op_err("symbol not resolved");
        if (leader == nullptr) return op_err("no leader");
        // 有未确认结果时拒绝再掷（flag bit0，ButtonRollExe 置位/Create+Apply 复位）
        uint8_t* flag = *reinterpret_cast<uint8_t**>(g_base + G_STATUSDICE_FLAG_GOT_VMA);
        if (flag != nullptr && (*flag & 1u)) return op_err("dice result pending, accept or reject first");
        int8_t char_idx = *reinterpret_cast<int8_t*>(reinterpret_cast<uint8_t*>(leader) + 0xd);
        int type = category - 0x34;
        if (char_idx < 0 || char_idx > 5) return op_err("bad char");
        if (type < 0 || type > 4) return op_err("bad dice type");
        int base[5];
        for (int i = 0; i < 5; ++i) base[i] = fn_get_stat_base(leader, i);
        if (!fn_status_dice_roll(char_idx, type)) return op_err("dice roll failed");
        int8_t* pending = *reinterpret_cast<int8_t**>(g_base + G_STATUSDICE_PENDING_GOT_VMA);
        if (pending == nullptr) return op_err("dice result missing");
        // 无 pending 时掷骰即消耗（原版 ButtonRollExe 语义），置 flag 待确认
        if (fn_consume_item != nullptr) fn_consume_item(item);
        if (flag != nullptr) *flag |= 1u;
        std::string s = "{\"ok\":true,\"base\":[";
        for (int i = 0; i < 5; ++i) {
            if (i > 0) s += ",";
            s += std::to_string(base[i]);
        }
        s += "],\"pending\":[";
        for (int i = 0; i < 5; ++i) {
            if (i > 0) s += ",";
            s += std::to_string(pending[i]);
        }
        s += "],\"delta\":[";
        for (int i = 0; i < 5; ++i) {
            if (i > 0) s += ",";
            s += std::to_string(pending[i] - base[i]);
        }
        s += "]}";
        return s;
    }
    // 解封（0x3a6-0x3ab）— ITEMSYSTEM_ReleaseSealed 独立路径，成功后手动消耗
    if (fn_is_sealed != nullptr && fn_is_sealed(category) && fn_release_sealed != nullptr) {
        void* before[96] = {nullptr};
        for (int b = 0; b < 6; ++b)
            for (int j = 0; j < 16; ++j)
                before[b * 16 + j] = inventory_item_at(b, j);
        int ok = fn_release_sealed(category);
        if (ok) {
            if (fn_consume_item != nullptr) fn_consume_item(item);
            return "{\"ok\":true,\"gained\":[" + inventory_gained_json(before) + "]}";
        }
        return op_err("release sealed failed");
    }
    // 开箱（0x3ef-0x3f1）— ITEMSYSTEM_OpenItemBox 独立路径，成功后手动消耗
    if (fn_is_item_box != nullptr && fn_is_item_box(category) && fn_open_item_box != nullptr) {
        void* before[96] = {nullptr};
        for (int b = 0; b < 6; ++b)
            for (int j = 0; j < 16; ++j)
                before[b * 16 + j] = inventory_item_at(b, j);
        int ok = fn_open_item_box(category);
        if (ok) {
            if (fn_consume_item != nullptr) fn_consume_item(item);
            return "{\"ok\":true,\"gained\":[" + inventory_gained_json(before) + "]}";
        }
        return op_err("open box failed");
    }

    // 其余类型：CHAR_UseItemEx — 药水/卷轴/技能书/配方书/佣兵卡/增益/超药水/打包物
    //   内部成功时已调 INVEN_ConsumeItem；失败（CD/状态不符）不消耗
    if (leader == nullptr) return op_err("no leader");
    if (fn_char_use_item_ex == nullptr) return op_err("symbol not resolved");
    int ok = fn_char_use_item_ex(leader, item, 0);
    return ok ? op_ok() : op_err("on cooldown");
}

// 骰子两段式第二步——接受：应用 pending 到 leader 基础属性（复刻 STATUSDICE_Apply 前两步，跳过确认弹窗）
std::string data_op_dice_accept() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_set_stat_base == nullptr || fn_get_stat_base == nullptr)
        return op_err("symbol not resolved");
    uint8_t* flag = *reinterpret_cast<uint8_t**>(g_base + G_STATUSDICE_FLAG_GOT_VMA);
    if (flag == nullptr || !(*flag & 1u)) return op_err("no dice result pending");
    void* leader = member_or_null(0);
    if (leader == nullptr) return op_err("no leader");
    int8_t* pending = *reinterpret_cast<int8_t**>(g_base + G_STATUSDICE_PENDING_GOT_VMA);
    if (pending == nullptr) return op_err("dice result missing");
    // 骰子已在掷骰时消耗，accept 只应用结果不重复消耗
    int base[5];
    for (int i = 0; i < 5; ++i) base[i] = fn_get_stat_base(leader, i);
    for (int i = 0; i < 5; ++i) fn_set_stat_base(leader, i, pending[i]);
    if (flag != nullptr) *flag &= ~1u;
    std::string s = "{\"ok\":true,\"base\":[";
    for (int i = 0; i < 5; ++i) {
        if (i > 0) s += ",";
        s += std::to_string(base[i]);
    }
    s += "],\"applied\":[";
    for (int i = 0; i < 5; ++i) {
        if (i > 0) s += ",";
        s += std::to_string(pending[i]);
    }
    s += "],\"delta\":[";
    for (int i = 0; i < 5; ++i) {
        if (i > 0) s += ",";
        s += std::to_string(pending[i] - base[i]);
    }
    s += "]}";
    return s;
}

// 骰子两段式第二步——拒绝：丢弃 pending（清 flag），不应用、不消耗（骰子已在掷时消耗，与游戏一致）
std::string data_op_dice_reject() {
    if (!game_in_world()) return op_err("not in game");
    uint8_t* flag = *reinterpret_cast<uint8_t**>(g_base + G_STATUSDICE_FLAG_GOT_VMA);
    if (flag == nullptr || !(*flag & 1u)) return op_err("no dice result pending");
    *flag &= ~1u;
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

std::string data_op_sell_item(int bag, int slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_remove_item_direct == nullptr || fn_add_money == nullptr || fn_item_get_price == nullptr)
        return op_err("symbol not resolved");
    void* item = inventory_item_at(bag, slot);
    if (item == nullptr) return op_err("slot empty");
    // 合法出售（v0.4.20）：ITEM_GetPrice 返回原始价格，出售价 = 原价 / 5
    // （改版币制：5 铜 = 1 银，出售价 = 真实价格 ÷ 5）
    int64_t base_price = fn_item_get_price(item);
    int64_t price = base_price / 5;
    fn_remove_item_direct(bag, slot);
    if (inventory_item_at(bag, slot) != nullptr) return op_err("sell failed");
    fn_add_money(price);
    return "{\"ok\":true,\"price\":" + std::to_string(price) + "}";
}

std::string data_op_move_item(int bag, int slot, int count, int to_bag, int to_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_inven_move_item == nullptr) return op_err("symbol not resolved");
    void* item = inventory_item_at(bag, slot);
    if (item == nullptr) return op_err("slot empty");
    if (count <= 0) return op_err("bad count");
    if (to_bag < 0 || to_bag >= 6 || to_slot < 0 || to_slot >= 16) return op_err("bad target");
    if (bag == to_bag && slot == to_slot) return op_err("same slot");
    int r = fn_inven_move_item(item, count, to_bag, to_slot);
    // 返回 1=成功（mov w1,#0x1），0/失败返回空——按目标槽是否有物品判定
    return r ? op_ok() : op_err("move failed");
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

// 合法佣兵遣散（v0.4.8）：MERCENARYSYSTEM_Release 清角色佣兵槽关联 + 重置槽结构（与 exclude 同级：主控/任务NPC 不可遣散）
std::string data_op_discharge(int mercenary_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_mercenary_release == nullptr || fn_get_member == nullptr)
        return op_err("symbol not resolved");
    void* ch = find_char_by_merc_slot(mercenary_slot);
    if (ch == nullptr) return op_err("mercenary not found");
    if (fn_get_member(0) == ch) return op_err("cannot discharge leader");
    if (fn_is_special_npc != nullptr && fn_is_special_npc(ch)) return op_err("cannot discharge quest npc");
    fn_mercenary_release(mercenary_slot);
    return op_ok();
}

// 取出佣兵装备（v0.4.8）：对佣兵角色调 CHAR_UnequipItemToInven（与队伍成员 unequip 同底层函数）
std::string data_op_withdraw(int mercenary_slot, int32_t equip_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_unequip == nullptr) return op_err("symbol not resolved");
    void* ch = find_char_by_merc_slot(mercenary_slot);
    if (ch == nullptr) return op_err("mercenary not found");
    if (equip_slot < 0 || equip_slot >= C_EQUIP_SLOTS) return op_err("bad slot");
    int r = fn_unequip(ch, equip_slot);
    return r ? op_ok() : op_err("withdraw failed");
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

// ---- combat 操作（v0.4.2）：attack / stop ----

// 按角色池 slot 解析对象指针（与 data_units_json 同源过滤：type 0-2、status<=2、坐标 0-1500）
void* pool_slot_obj(int slot) {
    if (g_base == 0 || slot < 0 || slot >= 128) return nullptr;
    uint8_t* pool = *reinterpret_cast<uint8_t**>(g_base + G_CHAR_POOL_VMA);
    if (pool == nullptr) return nullptr;
    uint8_t* obj = pool + slot * C_OBJ_SIZE;
    int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
    int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
    int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
    uint8_t status = obj[C_STATUS];
    if (type < 0 || type > 2) return nullptr;
    if (status > 2) return nullptr;
    if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) return nullptr;
    return obj;
}

std::string data_op_attack(int role, int target_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_char_set_target == nullptr || fn_char_set_action_id == nullptr)
        return op_err("symbol not resolved");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    void* target = pool_slot_obj(target_slot);
    if (target == nullptr) return op_err("target not found");
    fn_char_set_target(ch, target);
    fn_char_set_action_id(ch, 5, target);
    return op_ok();
}

std::string data_op_stop_combat(int role) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_char_stop_combat == nullptr) return op_err("symbol not resolved");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    fn_char_stop_combat(ch);
    return op_ok();
}

// ---- OP: 角色属性直写 ----

std::string data_op_set_hp(int role, int32_t hp) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (hp < 0) hp = 0;
    int32_t max_hp = fn_get_attr != nullptr ? fn_get_attr(ch, 0x1e) : 32767;
    if (hp > max_hp) hp = max_hp;
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_HP) = hp;
    return op_ok();
}

std::string data_op_set_mp(int role, int32_t mp) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (mp < 0) mp = 0;
    int32_t max_mp = fn_get_attr != nullptr ? fn_get_attr(ch, 0x1f) : 32767;
    if (mp > max_mp) mp = max_mp;
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_MP) = mp;
    return op_ok();
}

std::string data_op_set_attr(int role, int attr_index, int32_t value) {
    if (!game_in_world()) return op_err("not in game");
    if (attr_index < 0 || attr_index > 4) return op_err("bad attr");
    if (fn_set_stat_base == nullptr) return op_err("symbol not resolved");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    // CHAR_SetStatBase 直接写"基础属性"（[ch+0x250+i] s8），装备/分配/动态加成不受影响
    // 总属性 = 基础 + 分配(main) + 加成(bonus) + 动态(equip/skill/buff)
    fn_set_stat_base(ch, attr_index, value);
    return op_ok();
}
