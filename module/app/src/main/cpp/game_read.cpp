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
    s += ",\"class_idx\":" + std::to_string(static_cast<int>(reinterpret_cast<int8_t*>(ch)[C_CLASS]));
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
    // v0.4.38 移动修复：优先读游戏主控角色 PLAYER_pActivePlayer（G_PLAYER_ACTIVE_VMA，CHAR_MoveAsPath 驱动的真实对象）。
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
bool item_is_equip(void* item) {
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
    // INVEN_pItem(G_INVEN_VMA)：背包槽数组，6 袋 × 0x80 步长，每槽 8B 物品指针。
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
        // 瓦片通行查询（P0#3：MAP_IsBlocking 反汇编确认，GOT G_TILE_GOT_VMA 双层解引用，y*64+x 索引，bit3=阻挡）
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

// units/enemies/interactives 共用构建器（v0.5.35 过滤下沉 native）：
// mode 0=全量（units，含 char_loc）1=敌人（enemies，type==1）2=可交互（interactives，type==2 且 interactable==true）
static std::string build_units_json_impl(int mode, bool include_charloc) {
    // CHARSYSTEM 角色对象池：*(G_CHAR_POOL_VMA) 指向英雄对象，对象按 C_OBJ_SIZE 步长连续排列
    // （frida 实测 2026-08-05：31 有效单位 = 3 队伍 + 怪物 + NPC，坐标与玩家同像素坐标系）。
    // 有效性：type 0-2、status<=2、坐标 0-1500（未激活槽哨兵值 2048/16992/status>2，frida 实测排除）。
    // status: 0=队伍 1=城镇NPC/佣兵 2=怪物/召唤物。
    // type==2 为装饰/场景单位（路障/宝箱/火把/火堆等）。v0.4.25 曾过滤 type>1，本版取消过滤：
    // 输出 func_display（npc+0xa u16，NPCSYSTEM_CheckFunctionDisplay 入参）+ interactable（是否可交互），
    // 供消费者区分可交互装饰物（路障/宝箱）与纯装饰（火把）。
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
            for (int i = 0; i < C_CHARSYSTEM_POOL_SLOTS; ++i) {
                uint8_t* obj = pool + i * C_OBJ_SIZE;
                int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
                int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
                int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
                uint8_t status = obj[C_STATUS];
                if (type < 0 || type > 2) continue;
                if (status > 2) continue;
                if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) continue;
                if (mode == 1 && type != 1) continue;  // enemies：仅怪物/NPC
                if (mode == 2) {                        // interactives：仅可交互装饰物
                    if (type != 2) continue;
                    uint16_t fd = *reinterpret_cast<uint16_t*>(obj + 0x0a);
                    if (fn_check_function_display == nullptr || fn_check_function_display(fd) == 2) continue;
                }
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
                if (type == 2) {
                    uint16_t fd = *reinterpret_cast<uint16_t*>(obj + 0x0a);
                    s += ",\"func_display\":" + std::to_string(fd);
                    if (fn_check_function_display != nullptr) {
                        s += ",\"interactable\":" +
                             std::string(fn_check_function_display(fd) != 2 ? "true" : "false");
                    }
                }
                if (bfs_ok) {
                    int utx = x >> 4, uty = y >> 4;
                    // 单位自身 tile 被 nav_unit_blocks 标记阻挡恒不可达，distance 取「能紧贴该对象的相邻可达格」最小路径长度
                    int best = -1;
                    const int ndx[4] = {1, -1, 0, 0};
                    const int ndy[4] = {0, 0, 1, -1};
                    for (int k = 0; k < 4; ++k) {
                        int nx = utx + ndx[k], ny = uty + ndy[k];
                        if (nx >= 0 && nx < NAV_W && ny >= 0 && ny < NAV_H) {
                            int d = depth_map[ny * NAV_W + nx];
                            if (d >= 0 && (best < 0 || d < best)) best = d;
                        }
                    }
                    if (best >= 0) {
                        s += ",\"distance\":" + std::to_string(best);
                    } else {
                        // 紧贴位置全部不可达：回退单次 BFS 取 nearestDistance
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
        s += "]";  // 闭合 units 数组
        if (include_charloc) {
            // CHARLOC 位置登记池（CHARLOCSYSTEM，10B/条：+0 f0, +2 x u16, +4 y u16）——P0#2 逆向产出
            // 注：+0 字段语义待确认（data-sources §2.6 标注为地图ID，CHARLOCSYSTEM_Add 反汇编为 a0）
            uint8_t* cl_pool = *reinterpret_cast<uint8_t**>(g_base + G_CHARLOC_POOL_VMA);
            uint16_t cl_count = *reinterpret_cast<uint16_t*>(g_base + G_CHARLOC_COUNT_VMA);
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
    }
    s += "}";
    return s;
}

std::string build_units_json() { return build_units_json_impl(0, true); }

std::string build_enemies_json() { return build_units_json_impl(1, false); }

std::string build_interactives_json() { return build_units_json_impl(2, false); }

std::string build_drops_json() {
    // 掉落物数组：G_DROP_COUNT_GOT_VMA 单层计数（int8）+ G_DROP_ARRAY_GOT_VMA 双层数组（0x20 步长）。
    // 实体：+0x0 掉落物对象指针 +0x8 x +0xa y +0x18 标志（bit1=拾取中）。
    std::string s = "{\"drops\":[";
    if (g_base != 0) {
        int8_t* cnt = *reinterpret_cast<int8_t**>(g_base + G_DROP_COUNT_GOT_VMA);
        int n = (cnt != nullptr) ? static_cast<int>(*cnt) : 0;
        uint8_t* arr = *reinterpret_cast<uint8_t**>(*reinterpret_cast<void**>(g_base + G_DROP_ARRAY_GOT_VMA));
        int emitted = 0;
        for (int i = 0; arr != nullptr && i < n; ++i) {
            uint8_t* e = arr + i * 0x20;
            int16_t x = *reinterpret_cast<int16_t*>(e + 0x8);
            int16_t y = *reinterpret_cast<int16_t*>(e + 0xa);
            if (emitted > 0) s += ",";
            s += "{\"x\":" + std::to_string(x);
            s += ",\"y\":" + std::to_string(y) + "}";
            ++emitted;
        }
    }
    s += "]}";
    return s;
}

std::string build_gamestate_json() {
    bool story_active = data_story_active();

    // v0.5.42：screen 统一判定（data_ui_screen：主菜单细分/教学/弹窗/剧情/对话框/面板），
    // 完全基于 popup 栈顶 + 状态机，替代旧内联判定与 dialog_active 布尔（数据残留误报修复）。
    const char* screen = data_ui_screen();

    // 帧计数：FPS 系统每帧 +1（0x3075f0 u64，FPS_getTotalFrameCount 官方读取；G_FRAME_COUNT_VMA 为实测启用源）
    uint64_t frame = 0;
    if (g_base != 0) {
        uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
        uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
        if (cnt != nullptr) frame = *cnt;
    }

    std::string result = "{\"screen\":\"" + std::string(screen) + "\",\"frame\":" + std::to_string(frame);
    // v0.5.42：dialog_active 移除——screen 已精确表达 UI 占据（dialog_*/panel_* 前缀）
    // dialog 字段 = data_dialog_content_json 完整输出（popup/story/npc/wipeout/npc_quest/面板态 全类型，
    // 与 /api/ui/dialog 端点一致）
    std::string dcontent = data_dialog_content_json();
    if (dcontent.size() > 1 && dcontent[0] == '{') {
        result += ",\"dialog\":" + dcontent;
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

    // 帧计数：FPS 系统每帧 +1（0x3075f0 u64，FPS_getTotalFrameCount 官方读取；G_FRAME_COUNT_VMA 为实测启用源）
    uint64_t frame = 0;
    if (g_base != 0) {
        uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
        uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
        if (cnt != nullptr) frame = *cnt;
    }
    s += "\"frame\":" + std::to_string(frame);

    // v0.5.42：snapshot 的 screen 与 /api/ui 统一（data_ui_screen 完整枚举），
    // 替代旧简化三态（main_menu/world/loading）——AI 可精确判断当前界面
    s += ",\"screen\":";
    s += "\"" + std::string(data_ui_screen()) + "\"";

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
