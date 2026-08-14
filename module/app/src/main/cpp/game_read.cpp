// game_read.cpp —— 由 game_data.cpp 拆分生成（纯搬代码，零逻辑变更）

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
#include <vector>

#define MOVE_TAG "Inotia4Move"
#define MOVE_LOG(...) __android_log_print(ANDROID_LOG_INFO, MOVE_TAG, __VA_ARGS__)
#include "game_read.h"
#include "game_nav.h"
#include "game_tiles.h"
#include "game_misc.h"
#include "game_nav.h"
#include "game_state.h"
#include "game_json.h"

std::string member_json(void* ch) {
    std::string s = "{";
    s += "\"type\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_TYPE]));
    uint16_t name_id = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(ch) + C_NAME_ID);
    s += ",\"name_id\":" + std::to_string(name_id);
    s += ",\"level\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_LEVEL]));
    s += ",\"hp\":" + std::to_string(*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_HP));
    s += ",\"mp\":" + std::to_string(*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_MP));
    if (fn_get_attr != nullptr) {
        s += ",\"max_hp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_HP));
        s += ",\"max_mp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_MP));
    }
    if (fn_get_exp != nullptr) {
        s += ",\"exp\":" + std::to_string(fn_get_exp(ch));
        if (fn_get_next_exp != nullptr) {
            s += ",\"exp_next\":" + std::to_string(fn_get_next_exp(ch));
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
        s += ",\"main_stats\":[";
        for (int a = 0; a < 5; ++a) {
            if (a > 0) s += ",";
            s += std::to_string(fn_get_stat(ch, a));
        }
        s += "]";
    }
    if (fn_get_stat_base != nullptr) {
        s += ",\"base_stats\":[";
        for (int a = 0; a < 5; ++a) {
            if (a > 0) s += ",";
            s += std::to_string(fn_get_stat_base(ch, a));
        }
        s += "]";
    }
    if (fn_get_stat_bonus != nullptr) {
        s += ",\"bonus_stats\":[";
        for (int a = 0; a < 5; ++a) {
            if (a > 0) s += ",";
            s += std::to_string(fn_get_stat_bonus(ch, a));
        }
        s += "]";
    }
    if (fn_get_status_point != nullptr) {
        s += ",\"status_point\":" + std::to_string(fn_get_status_point(ch));
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
            s += ",\"type_flags\":" + std::to_string(flags);
            s += ",\"raw_rarity\":" + std::to_string((flags >> 2) & 0x0F);
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
    s += ",\"magic_rate\":" + std::to_string(it[I_MAGIC_RATE]);
    // v0.4.64 位域拆解（docs/systems/inventory.md §2.4 反汇编确认）
    uint8_t socket = it[I_SOCKET];
    s += ",\"socket\":" + std::to_string(socket);
    s += ",\"socket_filled\":" + std::to_string((socket >> 0) & 0x0F);
    s += ",\"socket_total\":" + std::to_string((socket >> 4) & 0x0F);
    uint16_t enchant = *reinterpret_cast<uint16_t*>(it + I_ENCHANT);
    s += ",\"enchant\":" + std::to_string(enchant);
    s += ",\"chaos\":" + std::string((enchant & 1) ? "true" : "false");
    s += ",\"enchant_id\":" + std::to_string((enchant >> 11) & 0x1F);
    s += ",\"enchant_level\":" + std::to_string((enchant >> 6) & 0x1F);
    uint32_t cnt = *reinterpret_cast<uint32_t*>(it + I_COUNT);
    s += ",\"chaos_level\":" + std::to_string((cnt >> 0) & 0xFF);
    s += ",\"chaos_rate\":" + std::to_string((cnt >> 8) & 0xFF);
    // options = 词缀值数组（兼容旧字段）；optionIds = 词缀索引数组（节点 +0x00 低 7 位，与 options 对齐）
    uint8_t* opt = *reinterpret_cast<uint8_t**>(it + I_OPTION_LIST);
    bool ofirst = true;
    int ocount = 0;
    s += ",\"options\":[";
    while (opt != nullptr && ocount < 32) {
        if (!ofirst) s += ",";
        s += std::to_string(*reinterpret_cast<int16_t*>(opt + O_VALUE));
        ofirst = false;
        opt = *reinterpret_cast<uint8_t**>(opt + O_NEXT);
        ++ocount;
    }
    s += "]";
    uint8_t* opt2 = *reinterpret_cast<uint8_t**>(it + I_OPTION_LIST);
    ofirst = true;
    ocount = 0;
    s += ",\"option_ids\":[";
    while (opt2 != nullptr && ocount < 32) {
        if (!ofirst) s += ",";
        s += std::to_string(*reinterpret_cast<uint16_t*>(opt2 + O_INDEX) & 0x7F);
        ofirst = false;
        opt2 = *reinterpret_cast<uint8_t**>(opt2 + O_NEXT);
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
    // v0.4.38 移动修复：优先读游戏主控角色 PLAYER_pActivePlayer（0x728fc0，CHAR_MoveAsPath 驱动的真实对象）。
    // 旧实现 PARTY_GetMember(0) 返回队伍槽 0 对象，其坐标是占位值（真机实测固定 240,296），
    // 用它做 BFS 起点错误 → CHAR_Move 全部判阻挡（返回 1）→ 导航任务立即终止、角色不动。
    if (g_player_active != nullptr) return *reinterpret_cast<void**>(g_player_active);
    return fn_get_member != nullptr ? fn_get_member(0) : nullptr;
}

std::string build_player_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{";
    s += "\"money\":" + std::to_string(fn_get_money != nullptr ? fn_get_money() : -1);
    s += ",\"map_id\":" + std::to_string(current_map_id());
    append_position(s, lead_member());
    s += ",\"active_quest\":" + std::to_string(g_active_quest != nullptr ? *reinterpret_cast<uint16_t*>(g_active_quest) : -1);
    s += ",\"main_mercenary_slot\":" + std::to_string(g_main_merc_slot != nullptr ? *reinterpret_cast<uint8_t*>(g_main_merc_slot) : -1);
    s += ",\"party_count\":" + std::to_string(fn_get_party_size != nullptr ? fn_get_party_size() : 3);
    s += "}";
    return s;
}

std::string build_party_json() {
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

// v0.5.12 ⑤ 装备判定：ITEMCLASSBASE 记录 +6 bit0=1 可堆叠 / 0 不可堆叠（装备）。与 ITEM_GetCumulateCount 同源。
static bool item_is_equip(void* item) {
    if (g_base == 0 || item == nullptr) return false;
    uint8_t* it = reinterpret_cast<uint8_t*>(item);
    uint16_t flags = *reinterpret_cast<uint16_t*>(it + I_TYPE);
    int category = (flags >> 6) & 0x3FF;
    uint8_t* class_data = *reinterpret_cast<uint8_t**>(*reinterpret_cast<void**>(g_base + G_ITEMCLASS_DATA_GOT_VMA));
    uint8_t* size_ptr = *reinterpret_cast<uint8_t**>(g_base + G_ITEMCLASS_SIZE_GOT_VMA);
    if (class_data == nullptr || size_ptr == nullptr) return false;
    uint8_t stride = *size_ptr;
    if (stride == 0) return false;
    return (class_data[category * stride + 6] & 1) == 0;
}

std::string build_inventory_json() {
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
                s += ",\"type_flags\":" + std::to_string(flags);
                s += ",\"raw_rarity\":" + std::to_string((flags >> 2) & 0x0F);
                if (fn_get_bit != nullptr) {
                    s += ",\"category\":" + std::to_string(fn_get_bit(flags, 15, 6));
                }
                if (fn_get_cumulate_count != nullptr) {
                    // v0.5.12 ⑤ count 语义：ITEM_GetCumulateCount 按类型区分——可堆叠类返回 bit25-31，
                    // 不可堆叠类（装备）返回 1（旧实现裸读位域导致装备 count=100）
                    s += ",\"count\":" + std::to_string(fn_get_cumulate_count(item));
                }
                s += std::string(",\"equip\":") + (item_is_equip(item) ? "true" : "false");
                if (fn_get_rarity != nullptr) {
                    s += ",\"rarity\":" + std::to_string(fn_get_rarity(item));
                }
                append_item_attrs(s, item);
                s += "}";
                ++filled;
            }
        }
        s += "],\"capacity\":16,\"slot_count\":" + std::to_string(filled) + "}";
    }
    s += "]}";
    return s;
}

std::string build_map_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{";
    s += "\"map_id\":" + std::to_string(current_map_id());
    append_position(s, lead_member());
        // 瓦片通行查询（P0#3：MAP_IsBlocking 反汇编确认，GOT *(0x2f3f48) 双层解引用，y*64+x 索引，bit3=阻挡）
        // P0#瓦片矩阵（2026-08-12）：统一走 nav_tiles() —— 静态数据优先，缺失回退内存
        {
            const uint8_t* tiles = nav_tiles();
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

// v0.4.62 P0：完整瓦片矩阵导出（64×64=4096B，base64 编码）
// 用途：P0 瓦片入静态数据采集——客户端一次性拿整图，无需 4096 次单 tile 查询
// P0#瓦片矩阵（2026-08-12）：静态数据就绪时返回静态矩阵（与运行时一致），否则回退内存
std::string build_tiles_json() {
    const uint8_t* tiles = nav_tiles();
    if (tiles == nullptr) return "{\"error\":\"no tiles\"}";
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t n = 64 * 64;
    std::string enc;
    enc.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = tiles[i] | (i + 1 < n ? uint32_t(tiles[i + 1]) << 8 : 0)
                                | (i + 2 < n ? uint32_t(tiles[i + 2]) << 16 : 0);
        enc += b64[(v >> 18) & 0x3F];
        enc += b64[(v >> 12) & 0x3F];
        enc += i + 1 < n ? b64[(v >> 6) & 0x3F] : '=';
        enc += i + 2 < n ? b64[v & 0x3F] : '=';
    }
    std::string s = "{\"map_id\":" + std::to_string(current_map_id());
    s += static_tiles_ready() ? ",\"src\":\"static\"" : ",\"src\":\"mem\"";
    s += ",\"size\":64,\"encoding\":\"base64\",\"tiles\":\"" + enc + "\"}";
    return s;
}

std::string build_units_json() {
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
            int hero_tx = -1, hero_ty = -1;
            void* hero = lead_member();
            if (hero != nullptr) {
                hero_tx = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X) >> 4;
                hero_ty = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y) >> 4;
            }
            // v0.4.60 多目标 BFS：单次遍历得全图可达深度，单位查表 O(1)（替代每单位一次 BFS）
            std::vector<int> depth_map;
            bool bfs_ok = hero_tx >= 0 && nav_bfs_multi(hero_tx, hero_ty, depth_map);
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
                if (bfs_ok) {
                    int utx = x >> 4, uty = y >> 4;
                    int d = (utx >= 0 && utx < NAV_W && uty >= 0 && uty < NAV_H)
                                ? depth_map[uty * NAV_W + utx] : -1;
                    if (d >= 0) {
                        s += ",\"distance\":" + std::to_string(d);
                    } else {
                        // 不可达：回退单次 BFS 取 nearestDistance（保持原语义）
                        NavPath np;
                        if (nav_bfs(hero_tx, hero_ty, utx, uty, np)) {
                            s += ",\"distance\":-1,\"nearest_distance\":" + std::to_string(np.distance);
                        } else {
                            s += ",\"distance\":-1";
                        }
                    }
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
            s += ",\"char_loc\":[";
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

std::string build_gamestate_json() {
    uint16_t state = g_state != nullptr ? *reinterpret_cast<uint16_t*>(g_state) : 0xFFFF;
    uint16_t prev = g_prev_state != nullptr ? *reinterpret_cast<uint16_t*>(g_prev_state) : 0xFFFF;
    uint32_t gs = g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
    uint8_t init = g_initstate != nullptr ? *reinterpret_cast<uint8_t*>(g_initstate) : 0;
    uint8_t popup_on = g_popup_on != nullptr ? *reinterpret_cast<uint8_t*>(g_popup_on) : 0;
    bool story_active = data_story_active();

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
                    if (vma == F_PANEL_SAVE_SLOT_ENTER) panel = "save_slot";
                    else if (vma == F_PANEL_CHAR_SELECT_ENTER) panel = "character_select";
                    else if (vma == F_PANEL_DAILY_REWARD_ENTER) panel = "daily_reward";
                    else if (vma == F_PANEL_OPTIONS_ENTER) panel = "options";
                    else if (vma == F_PANEL_SETTINGS_ENTER) panel = "settings";
                }
            }
        }
        screen = panel ? panel : "main_menu";
    } else if (state == 5) {
        if (tutorial_state() == 6) {
            // v0.4.44：药水教学激活（残血暂停）——obj170==6 时游戏暂停移动/按键
            screen = "tutorial_pause";
        } else if (story_active) {
            screen = "story";
        } else if (popup_on) {
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
                            case F_PANEL_CHARACTER_INFO_ENTER: panel = "character_info"; break;
                            case F_PANEL_CHOICE_ENTER: panel = "choice"; break;
                            case F_PANEL_INVENTORY_ENTER: panel = "inventory"; break;
                            case F_PANEL_INPUT_COUNT_ENTER: panel = "input_count"; break;
                            case F_PANEL_MERCENARY_ENTER: panel = "mercenary"; break;
                            case F_PANEL_CRAFT_ENTER: panel = "craft"; break;
                            case F_PANEL_NPC_ENTER: panel = "npc"; break;
                            case F_PANEL_NPC_QUEST_ENTER: panel = "npc_quest"; break;
                            case F_PANEL_NPC_REST_ENTER: panel = "npc_rest"; break;
                            case F_PANEL_NPC_REVIVE_ENTER: panel = "npc_revive"; break;
                            case F_PANEL_OPTIONS_ENTER: panel = "options"; break;
                            case F_PANEL_QUESTS_ENTER: panel = "quests"; break;
                            case F_PANEL_SAVE_SLOT_ENTER: panel = "save_slot"; break;
                            case F_PANEL_CHAR_SELECT_ENTER: panel = "character_select"; break;
                            case F_PANEL_SHORTCUT_ENTER: panel = "shortcut"; break;
                            case F_PANEL_SKILLS_ENTER: panel = "skills"; break;
                            case F_PANEL_SHOP_ENTER: panel = "shop"; break;
                            case F_PANEL_SETTINGS_ENTER: panel = "settings"; break;
                            case F_PANEL_WIPEOUT_ENTER: panel = "wipeout"; break;
                            case F_PANEL_WORLD_MAP_ENTER: panel = "world_map"; break;
                            case F_PANEL_IN_APP_ENTER:
                            case F_PANEL_UNK1_ENTER:
                            case F_PANEL_UNK2_ENTER:
                            case F_PANEL_UNK3_ENTER:
                            case F_PANEL_UNK4_ENTER:
                            case F_PANEL_UNK5_ENTER: panel = "in_app"; break;
                            case F_PANEL_DAILY_REWARD_ENTER: panel = "daily_reward"; break;
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
                         ",\"dialog_active\":" + (popup_on ? "true" : "false");
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
        result += ",\"dialog\":{\"text\":\"" + esc + "\",\"has_ok\":" + (has_ok ? "true" : "false") +
                  ",\"has_cancel\":" + (has_cancel ? "true" : "false") + ",\"buttons\":" + buttons + "}";
    }
    if (story_active) {
        result += ",\"story\":" + data_story_json();
    }
    result += "}";
    return result;
}

std::string build_skills_json() {
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
            s += "{\"action_id\":" + std::to_string(*reinterpret_cast<uint16_t*>(node + S_ACTION_ID));
            s += ",\"level\":" + std::to_string(node[S_LEVEL]);
            if (fn_get_act_max_level != nullptr) {
                s += ",\"max_level\":" + std::to_string(fn_get_act_max_level(ch, *reinterpret_cast<uint16_t*>(node + S_ACTION_ID)));
            }
            s += "}";
            first = false;
            node = *reinterpret_cast<uint8_t**>(node + S_NEXT);
            ++count;
        }
        s += "]";
        s += ",\"unlock_bitmap\":" + std::to_string(*reinterpret_cast<uint16_t*>(base_ch + C_SKILL_BMP));
        uint8_t* active = *reinterpret_cast<uint8_t**>(base_ch + C_ACTIVE_SKILL);
        s += ",\"active_skill_id\":" + std::to_string(active != nullptr ? *reinterpret_cast<uint16_t*>(active + S_ACTION_ID) : -1);
        s += ",\"skill_points\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_SKILL_POINTS]));
        s += "}";
    }
    s += "]";
    return s;
}

std::string build_mercenaries_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    // 佣兵槽：*(*(G_MERC_SLOTLIST_GOT_VMA)) 槽数组（20B/槽），flags bit0=占用 bit1=在队伍。
    // 关联角色：角色池 +0x352==slot；名称 CHAR_GetName、等级 +0x0E、坐标 +0x02/+0x04。
    std::string s = "[";
    if (g_base != 0) {
        uintptr_t got = *reinterpret_cast<uintptr_t*>(g_base + G_MERC_SLOTLIST_GOT_VMA);
        uint8_t* slots = got != 0 ? *reinterpret_cast<uint8_t**>(got) : nullptr;
        uintptr_t max_got = *reinterpret_cast<uintptr_t*>(g_base + G_MERC_MAX_GOT_VMA);
        int8_t max_slots = max_got != 0 ? *reinterpret_cast<int8_t*>(max_got) : 0;
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
                s += ",\"in_party\":" + std::string((flags & 0x02) ? "true" : "false");
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

std::string build_snapshot_json() {
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
    s += ",\"map_id\":" + std::to_string(current_map_id());
    void* hero = lead_member();
    if (hero != nullptr) {
        s += ",\"x\":" + std::to_string(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X));
        s += ",\"y\":" + std::to_string(*reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y));
    } else {
        s += ",\"x\":-1,\"y\":-1";
    }
    s += ",\"main_mercenary_slot\":" + std::to_string(g_main_merc_slot != nullptr ? *reinterpret_cast<uint8_t*>(g_main_merc_slot) : -1);
    s += ",\"party_count\":" + std::to_string(fn_get_party_size != nullptr ? fn_get_party_size() : 3);

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
            s += ",\"max_hp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_HP));
            s += ",\"max_mp\":" + std::to_string(fn_get_attr(ch, ATTR_MAX_MP));
        }
        if (fn_get_stat != nullptr) {
            s += ",\"main_stats\":[";
            for (int a = 0; a < 5; ++a) {
                if (a > 0) s += ",";
                s += std::to_string(fn_get_stat(ch, a));
            }
            s += "]";
        }
        if (fn_get_name != nullptr) {
            char* nm = fn_get_name(ch);
            s += ",\"name\":\"" + json_escape(nm) + "\"";
        }
        s += "}";
    }
    s += "]";

    s += "}";
    return s;
}


// 佣兵槽→角色指针（CHARSYSTEM_FindAsMercenarySlot 遍历大池含未上场佣兵）
void* find_char_by_merc_slot(int slot) {
    return fn_find_merc_slot != nullptr ? fn_find_merc_slot(slot) : nullptr;
}
