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

// json_escape 定义于全局作用域（~L510），前向声明放全局，供匿名 namespace 内 member_json 使用
std::string json_escape(const char* s);

// game_in_world 检查是否处于游戏中（state==5），定义在全局作用域供 data_*_json/data_op_* 使用
bool game_in_world() {
    return g_state != nullptr && *reinterpret_cast<uint16_t*>(g_state) == 5;
}

// 药水教学激活检测（v0.4.41）：[0x2f5000+0x170] 指向教学状态对象，头部值 6 = 药水教学激活
// （frida 实测：hp 低触发 6 → 触摸用药水回满 → 2。教学激活时游戏劫持按键禁移动）
int tutorial_state() {
    if (g_base == 0) return 0;
    uint8_t* obj = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_OBJ_GOT_VMA);
    if (obj == nullptr) return 0;
    return static_cast<int>(*reinterpret_cast<uint64_t*>(obj));
}

// 教学激活时取消教学（v0.4.43 修正）：复现官方 CHAR_ProcessShortcut 0xec340 教学完成链
// （逆向：用药水成功后 obj170==6 → Tutorialgetstate 轮转状态 + 写回 + 置 3 个教学标志；
//  仅补血（v0.4.41）不触发完成判定，obj170 停留 6 移动仍被拒。frida 复现 4 步链实测 obj170 6→1 移动解锁）
void tutorial_cancel() {
    if (g_base == 0) return;
    // 复现 0xec340：Tutorialgetstate 轮转 + 写回 obj170 + 关闭 3 个教学标志
    uint8_t* obj = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_OBJ_GOT_VMA);
    if (obj == nullptr) return;
    // v0.4.50：直接写 obj170=0（无教学态）而非依赖 Tutorialgetstate 返回值——轮转结果依赖
    // 教学槽位历史，进档后旧槽残留导致轮转返回 6，教学无法退出（真机实测 v0.4.49 复现）
    *reinterpret_cast<uint64_t*>(obj) = 0;
    uint8_t* bb8 = *reinterpret_cast<uint8_t**>(g_base + 0x2f6000 + 0xbb8);
    if (bb8 != nullptr) *bb8 = 0;
    uint8_t* f170 = *reinterpret_cast<uint8_t**>(g_base + 0x2f3000 + 0x170);
    if (f170 != nullptr) *f170 = 1;
    uint8_t* ee0 = *reinterpret_cast<uint8_t**>(g_base + 0x2f6000 + 0xee0);
    if (ee0 != nullptr) *ee0 = 0;
}

// move/walk/interact 教学前置检查：教学激活时拒绝操作（返回错误码），避免破坏游戏状态
const char* tutorial_block_error() {
    return tutorial_state() == 6 ? "tutorial active, heal first" : nullptr;
}

namespace {

// ============================================================
// 自研导航（v0.4.29）：基于瓦片矩阵 BFS，替代 CHAR_SearchPath
// （游戏寻路不能绕远路——backlog P1；支持复杂路径/可达性/最近可达点）
// ============================================================
constexpr int NAV_W = 64;
constexpr int NAV_H = 64;
constexpr int NAV_MAX_DIRS = 2048;

// 方向编码（与 walk 端点一致）：0=下 1=左 2=上 3=右
static const int NAV_DX[4] = {0, -1, 0, 1};
static const int NAV_DY[4] = {1, 0, -1, 0};

struct NavPath {
    bool found = false;
    int distance = -1;
    int nearest_x = -1, nearest_y = -1, nearest_dist = -1;
    int dirs[NAV_MAX_DIRS];
    int dir_count = 0;
};

const uint8_t* nav_tiles() {
    if (g_base == 0) return nullptr;
    return *reinterpret_cast<const uint8_t**>(g_base + G_TILE_GOT_VMA);
}

void* lead_member();

bool nav_blocked(const uint8_t* tiles, int tx, int ty) {
    if (tx < 0 || tx >= NAV_W || ty < 0 || ty >= NAV_H) return true;
    return (tiles[ty * NAV_W + tx] & TILE_BLOCK_BIT) != 0;
}

// 单位占用标记：扫角色池，非玩家单位所在 tile 视为阻挡（v0.4.29 改进）
// 仅静态地形无法表达动态碰撞——士兵/NPC 站在路径上会挡路，需绕行
void nav_unit_blocks(bool* blocks) {
    for (int i = 0; i < NAV_W * NAV_H; ++i) blocks[i] = false;
    if (g_base == 0) return;
    uint8_t* pool = *reinterpret_cast<uint8_t**>(g_base + G_CHAR_POOL_VMA);
    if (pool == nullptr) return;
    void* hero = lead_member();
    for (int i = 0; i < 128; ++i) {
        uint8_t* obj = pool + i * C_OBJ_SIZE;
        int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
        int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
        int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
        uint8_t status = obj[C_STATUS];
        // v0.4.39 修复：type==2（装饰/场景单位，火把/木桶等）也纳入阻挡——
        // CHAR_Move 的 CHARSYSTEM_GetCharacterBlock(0xddaac) 把它们当阻挡（ret=2），
        // BFS 若排除 type=2 会规划穿过装饰物的路径 → 走到面前被 ret=2 卡死（真机实测 280,328 卡死）。
        if (type < 0 || type > 2) continue;
        if (status > 2) continue;
        if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) continue;
        if (hero != nullptr && obj == hero) continue;
        int tx = x >> 4, ty = y >> 4;
        if (tx >= 0 && tx < NAV_W && ty >= 0 && ty < NAV_H) blocks[ty * NAV_W + tx] = true;
    }
}

// BFS：起点 (sx,sy) → 目标 (tx,ty)（tile 坐标，4 方向）
// 目标不可达时 nearest = 离目标曼哈顿最近的可达 tile；目标越界 nearest=-1
// v0.4.45：use_units=false 时无视动态单位（全局规划只走静态地形，避免绕远路）；
//   max_steps>0 时限制搜索深度（撞墙后短途局部绕行用，含单位）。
bool nav_bfs(int sx, int sy, int tx, int ty, NavPath& out, bool use_units = true, int max_steps = 0) {
    const uint8_t* tiles = nav_tiles();
    if (tiles == nullptr) return false;
    if (sx < 0 || sx >= NAV_W || sy < 0 || sy >= NAV_H) return false;
    if (nav_blocked(tiles, sx, sy)) {
        for (int d = 0; d < 4; ++d) {
            int nx = sx + NAV_DX[d], ny = sy + NAV_DY[d];
            if (!nav_blocked(tiles, nx, ny)) { sx = nx; sy = ny; break; }
        }
    }
    bool unit_blocks[NAV_W * NAV_H];
    if (use_units) nav_unit_blocks(unit_blocks);
    std::vector<int> prev(NAV_W * NAV_H);
    std::vector<int> depth(NAV_W * NAV_H);
    std::vector<int> queue(NAV_W * NAV_H);
    for (int i = 0; i < NAV_W * NAV_H; ++i) prev[i] = -1;
    auto tidx = [](int x, int y) { return y * NAV_W + x; };
    int head = 0, tail = 0;
    int start = tidx(sx, sy);
    prev[start] = start;
    depth[start] = 0;
    queue[tail++] = start;
    bool target_in = (tx >= 0 && tx < NAV_W && ty >= 0 && ty < NAV_H);
    int target = tidx(tx, ty);
    int best_manhattan = INT32_MAX, best_tile = -1, best_depth = -1;
    int found_target = -1;
    while (head < tail) {
        int cur = queue[head++];
        int cx = cur % NAV_W, cy = cur / NAV_W;
        int dcur = depth[cur];
        if (max_steps > 0 && dcur >= max_steps) continue;
        if (target_in) {
            int manh = (cx > tx ? cx - tx : tx - cx) + (cy > ty ? cy - ty : ty - cy);
            if (manh < best_manhattan) { best_manhattan = manh; best_tile = cur; best_depth = dcur; }
        }
        if (cur == target) { found_target = cur; break; }
        for (int d = 0; d < 4; ++d) {
            int nx = cx + NAV_DX[d], ny = cy + NAV_DY[d];
            if (nx < 0 || nx >= NAV_W || ny < 0 || ny >= NAV_H) continue;
            int ni = tidx(nx, ny);
            if (prev[ni] != -1 || nav_blocked(tiles, nx, ny)) continue;
            if (use_units && unit_blocks[ni]) continue;
            prev[ni] = cur;
            depth[ni] = dcur + 1;
            queue[tail++] = ni;
        }
    }
    int end_tile = -1;
    if (found_target != -1) end_tile = found_target;
    else if (best_tile != -1) end_tile = best_tile;
    if (end_tile == -1) return false;
    out.found = (found_target != -1);
    out.distance = depth[end_tile];
    out.nearest_x = best_tile == -1 ? -1 : best_tile % NAV_W;
    out.nearest_y = best_tile == -1 ? -1 : best_tile / NAV_W;
    out.nearest_dist = best_depth;
    {
        int rev[NAV_MAX_DIRS];
        int rev_count = 0;
        int cur = end_tile;
        while (cur != start && rev_count < NAV_MAX_DIRS) {
            int p = prev[cur];
            int dx = (cur % NAV_W) - (p % NAV_W);
            int dy = (cur / NAV_W) - (p / NAV_W);
            int d = -1;
            for (int k = 0; k < 4; ++k) {
                if (NAV_DX[k] == dx && NAV_DY[k] == dy) { d = k; break; }
            }
            if (d < 0) break;
            rev[rev_count++] = d;
            cur = p;
        }
        for (int i = 0; i < rev_count; ++i) out.dirs[rev_count - 1 - i] = rev[i];
        out.dir_count = rev_count;
    }
    return true;
}

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
    // v0.4.38 移动修复：优先读游戏主控角色 PLAYER_pActivePlayer（0x728fc0，CHAR_MoveAsPath 驱动的真实对象）。
    // 旧实现 PARTY_GetMember(0) 返回队伍槽 0 对象，其坐标是占位值（真机实测固定 240,296），
    // 用它做 BFS 起点错误 → CHAR_Move 全部判阻挡（返回 1）→ 导航任务立即终止、角色不动。
    if (g_player_active != nullptr) return *reinterpret_cast<void**>(g_player_active);
    return fn_get_member != nullptr ? fn_get_member(0) : nullptr;
}

}  // namespace

std::string data_player_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{";
    s += "\"money\":" + std::to_string(fn_get_money != nullptr ? fn_get_money() : -1);
    s += ",\"mapId\":" + std::to_string(current_map_id());
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
    s += "\"mapId\":" + std::to_string(current_map_id());
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
            int hero_tx = -1, hero_ty = -1;
            void* hero = lead_member();
            if (hero != nullptr) {
                hero_tx = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X) >> 4;
                hero_ty = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y) >> 4;
            }
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
                if (hero_tx >= 0) {
                    NavPath np;
                    if (nav_bfs(hero_tx, hero_ty, x >> 4, y >> 4, np)) {
                        s += ",\"distance\":" + std::to_string(np.found ? np.distance : -1);
                        if (!np.found) s += ",\"nearestDistance\":" + std::to_string(np.distance);
                    } else {
                        s += ",\"distance\":-1";
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

// ---- 剧情对话（EVTSYSTEM）状态检测（v0.4.27）----
// 剧情对话激活 = GAMESTATE_nState==1（Event 状态）。
// ⚠️ 不能用 EVTSYSTEM_nState!=0/pText!=NULL 单独判定：剧情结束后这些值残留
// （frida 实测：剧情结束后 gst=0/evtNState=1/pText=NULL 残留，误判为剧情中）。
bool data_story_active() {
    if (g_base == 0) return false;
    uint32_t gs = g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
    return gs == 1;
}

// 剧情对话内容 JSON：说话人（pTeller→CHAR_GetName）、当前句文本（pText UTF-8，NUL 截断）、进度（nIndex/nDataCount）
// 弹窗栈顶面板 VMA（enter 指针减基址；无栈/栈空/栈异常返回 0）
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

std::string data_gamestate_json() {
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
    if (story_active) {
        result += ",\"story\":" + data_story_json();
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
    s += ",\"mapId\":" + std::to_string(current_map_id());
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
    // v0.4.29 自研 BFS（替代 CHAR_SearchPath：游戏寻路不能绕远路）
    // 返回：inMap/found/distance/path（tile 中心像素）/nearest（不可达时最近可达点）
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    void* hero = lead_member();
    if (hero == nullptr) return "{\"error\":\"no player\"}";
    int px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y);
    std::string s = "{\"target\":{\"x\":" + std::to_string(tx) + ",\"y\":" + std::to_string(ty) + "}";
    s += ",\"start\":{\"x\":" + std::to_string(px) + ",\"y\":" + std::to_string(py) + "}";
    s += ",\"inMap\":" + std::string((tx >= 0 && tx < NAV_W * 16 && ty >= 0 && ty < NAV_H * 16) ? "true" : "false");
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

int data_active_quest() {
    if (g_active_quest == nullptr) return -1;
    return *reinterpret_cast<uint16_t*>(g_active_quest);
}

// 当前委托任务列表（v0.4.38）：读 QUESTSYSTEM 任务槽数组（GOT 双层解引用，12B/槽 +0 questId u16）。
// 与 QUESTSYSTEM_Find(0x122914)/QUESTSYSTEM_CopySlot(0x122974) 同源，列表含槽号与 questId。
std::string data_quest_list_json() {
    if (!game_in_world()) return "{\"error\":\"not in game\"}";
    std::string s = "{\"quests\":[";
    if (g_base != 0) {
        uint8_t* cnt_ptr = *reinterpret_cast<uint8_t**>(g_base + G_QUEST_SLOT_COUNT_VMA);
        uint8_t* slots_ptr = *reinterpret_cast<uint8_t**>(g_base + G_QUEST_SLOTS_GOT_VMA);
        if (cnt_ptr != nullptr && slots_ptr != nullptr) {
            uint8_t cnt = *cnt_ptr;
            uint8_t* slots = *reinterpret_cast<uint8_t**>(slots_ptr);
            if (slots != nullptr && cnt > 0 && cnt <= 20) {
                for (int i = 0; i < cnt; ++i) {
                    if (i > 0) s += ",";
                    uint16_t qid = *reinterpret_cast<uint16_t*>(slots + i * 0xC);
                    s += "{\"slot\":" + std::to_string(i) + ",\"questId\":" + std::to_string(qid) + "}";
                }
            }
        }
    }
    s += "]}";
    return s;
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
    if (!r) return op_err("enter slot failed");
    // v0.4.49：进档后清理残留教学暂停——obj170=6（药水教学）是持久状态，
    // 回主菜单（GAMESTATE_SetState(4)）与 GAME_StartResumeGame 均不清理，
    // API 进档后若仍为 6 会残留 tutorial_pause 卡住移动。手动进档无此问题
    // （用户从正常世界态回主菜单时 obj170 已非 6）。进档即复位教学。
    if (tutorial_state() == 6) tutorial_cancel();
    return op_ok();
}

// 面板关闭（v0.4.32）：走官方流程3（UI_SetPopupProcessInfo(3,0) → 主循环 UI_PopupProcess → POPUPSTATE_Pop）。
// 不直接调 POPUPSTATE_Pop（v0.4.5 崩溃：HTTP 线程同步 Pop 破坏 popup 状态机时序），
// 由游戏主循环在帧内处理弹窗栈出栈，官方 ButtonBackExe（SystemMenu_ButtonBackExe/CharacterInfo_ButtonBackExe 等）
// 均复现此链：SOUNDSYSTEM_Play + UI_SetPopupProcessInfo(3,0) + HUD 开关恢复 [0x2f6000+0xc48]=1。
std::string data_op_panel_close() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    // 前置：栈顶必须是面板（enter 匹配 PANELS），非空弹窗栈或弹窗（G_POPUP_ON）不处理
    if (g_base == 0 || g_popup_stack == nullptr) return op_err("libgame not ready");
    uint8_t* stk = reinterpret_cast<uint8_t*>(g_popup_stack);
    uint32_t count = *reinterpret_cast<uint32_t*>(stk + 8);
    if (count == 0 || count > 27) return op_err("no panel open");
    uint64_t data = *reinterpret_cast<uint64_t*>(stk + 0x18);
    if (data == 0) return op_err("no panel open");
    uint8_t* top = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data)) + (count - 1) * 0x40;
    uintptr_t enter = *reinterpret_cast<uintptr_t*>(top + 0x10);
    uintptr_t vma = enter > g_base ? enter - g_base : 0;
    bool is_panel = false;
    switch (vma) {
        case 0x148950: case 0x14a664: case 0x14a8b0: case 0x14ad98:
        case 0x14af14: case 0x14b330: case 0x14b5dc: case 0x14b858:
        case 0x14ba98: case 0x14bb48: case 0x14be20: case 0x14c218:
        case 0x14c720: case 0x14d670: case 0x14df04: case 0x14f194:
        case 0x14f4b8: case 0x14fb38: case 0x1506d8: case 0x150f48:
        case 0x15e054: case 0x15e3dc: case 0x15e740: case 0x15eac8:
        case 0x15ee70: case 0x15f1f8: case 0x16f050:
            is_panel = true;
            break;
        default: break;
    }
    if (!is_panel) return op_err("top of stack is not a panel");
    // 官方 ButtonBackExe 链：SOUNDSYSTEM_Play(0) + 流程3 + HUD 开关恢复
    fn_ui_set_popup_process_info(3, 0);
    uint8_t** hud_gate = reinterpret_cast<uint8_t**>(g_base + 0x2f6000 + 0xc48);
    if (hud_gate != nullptr && *hud_gate != nullptr) **hud_gate = 1;
    return op_ok();
}

// 面板打开（v0.4.32）：走官方流程1（UI_SetPopupProcessInfo(1, state_id) → 主循环 UI_PopupProcess → POPUPSTATE_Push）。
// state_id 通过扫描 g_sPopupStateList（GOT 0x2f3000+0x4f0 指向，27 条 × 64B）按 enter 指针匹配面板 VMA 得到，
// 与官方打开面板的调用链一致（流程1 = POPUPSTATE_Push + SetClearDrawFlag）。
std::string data_op_panel_open(const std::string& panel) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    if (g_base == 0) return op_err("libgame not ready");
    // 面板名 → enter VMA（与 data_gamestate_json 的 PANELS 映射一致）
    uintptr_t target = 0;
    if (panel == "character_info") target = 0x148950;
    else if (panel == "choice") target = 0x14a664;
    else if (panel == "inventory") target = 0x14a8b0;
    else if (panel == "input_count") target = 0x14ad98;
    else if (panel == "mercenary") target = 0x14af14;
    else if (panel == "craft") target = 0x14b330;
    else if (panel == "npc") target = 0x14b5dc;
    else if (panel == "npc_quest") target = 0x14b858;
    else if (panel == "npc_rest") target = 0x14ba98;
    else if (panel == "npc_revive") target = 0x14bb48;
    else if (panel == "options") target = 0x14be20;
    else if (panel == "quests") target = 0x14c218;
    else if (panel == "save_slot") target = 0x14c720;
    else if (panel == "character_select") target = 0x14d670;
    else if (panel == "shortcut") target = 0x14df04;
    else if (panel == "skills") target = 0x14f194;
    else if (panel == "shop") target = 0x14f4b8;
    else if (panel == "settings") target = 0x14fb38;
    else if (panel == "wipeout") target = 0x1506d8;
    else if (panel == "world_map") target = 0x150f48;
    else if (panel == "in_app") target = 0x15e054;
    else if (panel == "daily_reward") target = 0x16f050;
    else return op_err("unknown panel");
    // 面板可开白名单（v0.4.34 真机实测收紧）：仅允许不依赖外部上下文的独立面板。
    // 崩溃记录（全部 SIGSEGV，tombstone 已验证）：
    //   options      → GAMELOADER_DrawBackGround→GRPX_DrawPart（主菜单/GAMELOADER 场景专属）
    //   craft/shop   → CHAR_GetName 空指针（需 NPC 交互对象 [0x2f6000+0xc20]→[x0] 就绪）
    //   input_count  → ControlObject_GetActive 空控件（需 inventory 物品数量输入上下文）
    // 语义不正确的面板（v0.4.34 移除）：
    //   choice       → 游戏内由事件/剧情驱动的选择框，API 打开语义不符
    //   world_map    → 由游戏内事件（如保存点）驱动的世界地图，API 打开语义不符
    //   wipeout      → 角色死亡时游戏自动打开，非用户可操作面板
    // 其余未实证面板（npc 系列/shortcut/in_app 等）同样拒绝，避免 API 直接 Push 崩溃。
    bool openable = (panel == "character_info" || panel == "inventory" ||
                     panel == "mercenary" || panel == "quests" || panel == "settings" ||
                     panel == "skills");
    if (!openable) return op_err("panel requires in-game context");
    // 扫描 state list 找 enter == g_base+target 的 state id
    uint8_t* list = *reinterpret_cast<uint8_t**>(g_base + G_POPUP_STATE_LIST_GOT_VMA);
    if (list == nullptr) return op_err("state list not ready");
    int state_id = -1;
    for (int i = 0; i < 27; ++i) {
        uintptr_t enter = *reinterpret_cast<uintptr_t*>(list + i * 0x40 + 0x10);
        if (enter == g_base + target) { state_id = i; break; }
    }
    if (state_id < 0) return op_err("panel state not found");
    fn_ui_set_popup_process_info(1, state_id);
    return op_ok();
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
    // v0.4.52：NearNPC 空时模拟触摸交互链——扫描角色池找玩家面前/附近的 type==2 可交互物
    // （路障/宝箱）。官方触摸链不经过 NearNPC 24px 检测，直接检测点击处对象；玩家距路障 32px
    // 超阈值但触摸可交互（b54 真机：手动点击成功建立路障对话）。这里放宽到 3 格（48px）且朝向匹配。
    if (near_npc == nullptr && g_base != 0) {
        void* hero = lead_member();
        if (hero != nullptr) {
            int hx = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_X);
            int hy = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(hero) + C_POS_Y);
            int hdir = *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(hero) + 0x6);
            uint8_t* pool = *reinterpret_cast<uint8_t**>(g_base + G_CHAR_POOL_VMA);
            if (pool != nullptr) {
                int best_dist = 60, best_slot = -1;
                for (int i = 0; i < 128; ++i) {
                    uint8_t* obj = pool + i * C_OBJ_SIZE;
                    int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
                    int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
                    int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
                    if (type != 2) continue;
                    if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500) continue;
                    int dx = x - hx, dy = y - hy;
                    int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                    if (dist > 60) continue;
                    // 朝向匹配：dir 0=下 1=左 2=上 3=右（dx/dy 符号对应）
                    bool facing = (hdir == 3 && dx > 0) || (hdir == 1 && dx < 0) ||
                                  (hdir == 0 && dy > 0) || (hdir == 2 && dy < 0);
                    if (!facing) continue;
                    if (dist < best_dist) { best_dist = dist; best_slot = i; }
                }
                if (best_slot >= 0) {
                    void* obj = pool + best_slot * C_OBJ_SIZE;
                    *reinterpret_cast<void**>(g_base + G_PLAYER_NEAR_NPC_VMA) = obj;
                    uint8_t r = fn_uinpc_init();
                    return r ? op_ok() : op_err("interact failed");
                }
            }
        }
        if (fn_evtsystem_do_check_all_event == nullptr) return op_err("no npc nearby");
        fn_evtsystem_do_check_all_event(2);
        return op_ok();
    }
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

// ---- 统一对话 API（v0.4.27，interact/get-content/select 三端点）----
// get-content 返回当前对话上下文（剧情对话/NPC 对话/弹窗），type 区分来源，options 给出可选动作。
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
    // wipeout 死亡面板（v0.4.35）：栈顶 enter == 0x1506d8 时优先于 NPC 对话报告
    uintptr_t top_vma = data_popup_top_vma();
    if (top_vma == 0x1506d8) {
        std::string out = "{\"type\":\"wipeout\",\"options\":["
                          "{\"id\":\"revive\",\"label\":\"复活\"},"
                          "{\"id\":\"special_revive\",\"label\":\"特殊复活\"},"
                          "{\"id\":\"game_over\",\"label\":\"游戏结束\"}]}";
        return out;
    }
    // NPC 任务完成面板（v0.4.55）：栈顶 enter == 0x14b858（npc_quest）时报告任务完成态，
    // 选项 complete=完成任务（UINpcQuest_ButtonOKExe 官方链）close=关闭面板（panel/close）。
    if (top_vma == 0x14b858) {
        std::string out = "{\"type\":\"npc_quest\"";
        int quest_id = -1;
        if (g_base != 0) {
            uint8_t** idx_ptr = reinterpret_cast<uint8_t**>(g_base + G_NPC_QUEST_IDX_GOT_VMA);
            if (idx_ptr != nullptr && *idx_ptr != nullptr)
                quest_id = *reinterpret_cast<int16_t*>(*idx_ptr);
        }
        out += ",\"questId\":" + std::to_string(quest_id);
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
    return "{\"type\":\"none\",\"options\":[]}";
}

// 剧情对话推进（v0.4.38）：走官方剧情确认按钮 Event_ButtonOKExe。
// 修正历史错误：v0.4.37 前直接改 states[sceneIdx]=-1 模拟键盘 key==0x2 路径，
// 但游戏内按钮走 Event_ButtonOKExe（读 [0x2f4000+0xf0]→[obj+0x10] 键码→EVTSYSTEM_PressKey），
// 直接改数组绕过 SetState/场景销毁 → 事件系统状态错乱 → 剧情后玩家无法控制（真机实测）。
std::string story_next() {
    if (g_base == 0) return op_err("not in game");
    if (fn_event_button_ok_exe == nullptr) return op_err("symbol not resolved");
    fn_event_button_ok_exe();
    return op_ok();
}

// 剧情对话跳过（v0.4.38）：走官方剧情跳过按钮 Event_ButtonSkipExe。
// 修正历史错误：v0.4.37 前模拟键盘 key==0x2d 路径（states[sceneIdx]=cur<=5?6:-1），
// 游戏内 skip 按钮走 Event_ButtonSkipExe（读 [obj+0x40] 键码→EVTSYSTEM_PressKey→
// EVTSYSTEM_SetState(7)+INSTANTSYSTEM_DestroyType(2)），跳过段并销毁场景，命令流正确继续。
std::string story_skip() {
    if (g_base == 0) return op_err("not in game");
    if (fn_event_button_skip_exe == nullptr) return op_err("symbol not resolved");
    fn_event_button_skip_exe();
    return op_ok();
}

// 统一对话选择（v0.4.27）：action=next/skip/ok/cancel 或 index=N（NPC 选项）
// v0.4.38 校验：action 必须匹配当前对话态（get-content 五态），不匹配报错不静默透传 native。
std::string data_op_dialog_select(const std::string& action, int index) {
    if (!game_in_world()) return op_err("not in game");
    // 先按当前态确定合法 action 集合（与 data_dialog_content_json 判定一致：popup 最优先）
    if (g_popup_on != nullptr && *reinterpret_cast<uint8_t*>(g_popup_on)) {
        if (action != "ok" && action != "cancel") return op_err("no such option in popup");
    } else if (data_story_active()) {
        if (action != "next" && action != "skip") return op_err("no such option in story");
    } else if (data_popup_top_vma() == 0x1506d8) {
        if (action != "revive" && action != "special_revive" && action != "game_over")
            return op_err("no such option in wipeout");
    } else if (data_popup_top_vma() == 0x14b858) {
        // npc_quest 面板：仅接受 complete（完成任务）/ close（关闭面板）
        if (action != "complete" && action != "close") return op_err("no such option in npc_quest");
    } else if (g_base != 0 &&
               (*reinterpret_cast<uint8_t*>(g_base + G_UICHOICE_COUNT_VMA) > 0 ||
                *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_COUNT_VMA) > 0)) {
        if (action != "next" && index < 0) return op_err("no such option in npc");
    } else {
        return op_err("no dialog");
    }
    if (action == "next") {
        if (data_story_active()) return story_next();
        // 非 story 的 next（NPC/任务列表对话）：用与 get-content 同源的安全文本读取
        // （G_NPCTASKLIST_DESCTEXT_VMA 指向当前对话文本），避免 fn_npctasklist_make_dlg
        // 对非 NPC 对象（如路障交互）返回悬垂指针导致 json_escape 越界崩溃（v0.4.48 修复）
        char* desc = nullptr;
        if (g_base != 0) desc = *reinterpret_cast<char**>(g_base + G_NPCTASKLIST_DESCTEXT_VMA);
        if (desc == nullptr || desc[0] == 0) return op_err("no dialog");
        return "{\"ok\":true,\"text\":\"" + json_escape(desc) + "\"}";
    }
    if (action == "skip") {
        if (!data_story_active()) return op_err("no story");
        return story_skip();
    }
    if (action == "ok") return data_op_dialog_ok();
    if (action == "cancel") return data_op_dialog_cancel();
    // npc_quest 面板动作（v0.4.55）：complete=完成任务（UINpcQuest_ButtonOKExe 官方链：
    // questIdx→stateTbl 判定→完成态走 UI_SetPopupProcessInfo(3,0)+ChangeQuestState+DoCheckAllEvent），
    // close=复用面板关闭（panel/close 官方流程3）。
    if (data_popup_top_vma() == 0x14b858) {
        if (action == "complete") {
            if (fn_uinpc_quest_button_ok_exe == nullptr) return op_err("symbol not resolved");
            fn_uinpc_quest_button_ok_exe();
            return op_ok();
        }
        if (action == "close") return data_op_panel_close();
    }
    // wipeout 死亡面板动作（v0.4.35）：栈顶是 wipeout 面板时接受 revive/special_revive/game_over
    if (action == "revive" || action == "special_revive" || action == "game_over") {
        if (data_popup_top_vma() != 0x1506d8) return op_err("not in wipeout");
        if (action == "revive") {
            if (fn_wipeout_button_revive == nullptr) return op_err("symbol not resolved");
            fn_wipeout_button_revive();
        } else if (action == "special_revive") {
            if (fn_wipeout_button_special_revive == nullptr) return op_err("symbol not resolved");
            fn_wipeout_button_special_revive();
        } else {
            if (fn_wipeout_button_gameover == nullptr) return op_err("symbol not resolved");
            fn_wipeout_button_gameover();
        }
        return op_ok();
    }
    if (index >= 0) return data_op_npc_dialog_select(index);
    return op_err("bad action");
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
    // v0.4.37：剧情/切图（GAMESTATE_nState!=0）期间禁止注册新任务——此时注册会立即在
    // 后台线程调 CHAR_Move 与剧情/切图状态机竞争，破坏游戏控制态导致结束后卡死。
    if (g_gamestate != nullptr && *reinterpret_cast<uint32_t*>(g_gamestate) != 0) return 0;
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

// ---- walk 任务上下文与回调：每帧 CHAR_Move(flag=0) 走 1 步，累计 60 帧 ----
struct WalkCtx {
    void* ch;
    int dir;
    int remaining;
};

bool walk_task_tick(void* ctx) {
    WalkCtx* w = static_cast<WalkCtx*>(ctx);
    if (fn_char_move == nullptr || w == nullptr) return false;
    // v0.4.37：剧情/切图触发（GAMESTATE_nState!=0）立即自终止——避免后台线程继续
    // CHAR_Move 与剧情状态机竞争（真机实测：剧情结束后触摸无法移动、怪无法攻击）
    if (g_gamestate != nullptr && *reinterpret_cast<uint32_t*>(g_gamestate) != 0) return false;
    // flag=0：CHAR_Move 内部自动 MAP_SetFocus 跟随摄像机。
    // 返回值：0=正常走一步（成功），非 0=撞墙/阻挡（反汇编 e98dc mov w20,#0x1）
    // v0.4.40：CHAR_Move 不更新朝向（官方链=按键→SetDirection+Move 分开调），移动前先设朝向避免"飘逸"
    if (fn_char_set_direction != nullptr) fn_char_set_direction(w->ch, w->dir);
    if (fn_char_move(w->ch, w->dir, 8, 0)) return false;  // 撞墙/不可走
    return --w->remaining > 0 && !map_link_check(w->ch);  // 走完 60 帧或切图终止
}

// 导航任务上下文：方向序列逐帧 CHAR_Move 驱动（FrameTaskManager 单任务语义）
struct NavCtx {
    void* ch = nullptr;
    int dir_count = 0;
    int dir_idx = 0;
    int target_px = 0, target_py = 0;
    int target_tx = -1, target_ty = -1;
    bool face_target = false;
    int final_tx = -1, final_ty = -1;
    int replan_count = 0;
    int8_t dirs[NAV_MAX_DIRS];
};

bool nav_task_tick(void* ctx) {
    NavCtx* n = static_cast<NavCtx*>(ctx);
    if (n == nullptr || n->ch == nullptr || fn_char_move == nullptr) return false;
    // v0.4.37：剧情/切图触发（GAMESTATE_nState!=0）立即自终止——避免后台线程继续
    // CHAR_Move 与剧情状态机竞争（真机实测：剧情结束后触摸无法移动、怪无法攻击）
    if (g_gamestate != nullptr && *reinterpret_cast<uint32_t*>(g_gamestate) != 0) return false;
    uint8_t* ch = reinterpret_cast<uint8_t*>(n->ch);
    int px = *reinterpret_cast<int16_t*>(ch + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(ch + C_POS_Y);
    if ((px - n->target_px < 8 && n->target_px - px < 8) &&
        (py - n->target_py < 8 && n->target_py - py < 8)) {
        n->dir_idx++;
        if (n->dir_idx >= n->dir_count) {
            if (n->face_target && n->final_tx >= 0) {
                n->face_target = false;
                int dx = n->final_tx - (px >> 4), dy = n->final_ty - (py >> 4);
                int face_dir = (dx < 0) ? 1 : (dx > 0) ? 3 : (dy > 0) ? 0 : 2;
                if (fn_char_set_direction != nullptr) fn_char_set_direction(n->ch, face_dir);
                fn_char_move(n->ch, face_dir, 8, 0);
            }
            return false;
        }
        int d = n->dirs[n->dir_idx];
        n->target_px = (px >> 4) * 16 + 8 + NAV_DX[d] * 16;
        n->target_py = (py >> 4) * 16 + 8 + NAV_DY[d] * 16;
    }
    if (n->dir_idx < 0 || n->dir_idx >= n->dir_count) return false;
    // v0.4.40：CHAR_Move 不更新朝向，移动前先设朝向（nav 同样适用，避免路径移动"飘逸"）
    if (fn_char_set_direction != nullptr) fn_char_set_direction(n->ch, n->dirs[n->dir_idx]);
    if (fn_char_move(n->ch, n->dirs[n->dir_idx], 8, 0)) {
        // 撞墙/被动态单位阻挡 → 短距绕行（v0.4.53）：
        //   目标 = 原路径上障碍物后方首个可达格（沿原方向探测，曼哈顿距离 >16px），
        //   找到后从该格 BFS 到最终目标（替换 dirs），跳过被挡的中间格不回走。
        if (n->target_tx < 0 || n->replan_count >= 5) return false;
        int cpx = px >> 4, cpy = py >> 4;
        int resume_tx = -1, resume_ty = -1;
        if (n->dir_idx >= 0 && n->dir_idx < n->dir_count) {
            const uint8_t* tiles = nav_tiles();
            int nd = n->dirs[n->dir_idx];
            int sx = cpx, sy = cpy;
            // v0.4.54：绕行目标取"障碍后方第三个可到达格"（v0.4.53 取首个，可能紧贴障碍/
            // 单位格——障碍后方首个可达格可能过于近，绕行后仍被挡）。沿原方向探测 6 步，
            // 跳过前两个可到达格，取第 3 个；不足 3 个时回退最后一个，全无可达格走全量规划。
            int reachable = 0;
            int last_tx = -1, last_ty = -1;
            for (int step = 1; step <= 6 && tiles != nullptr; ++step) {
                int nx = sx + NAV_DX[nd] * step, ny = sy + NAV_DY[nd] * step;
                if (nx < 0 || nx >= NAV_W || ny < 0 || ny >= NAV_H) break;
                if (!nav_blocked(tiles, nx, ny)) {
                    int manh = (nx > sx ? nx - sx : sx - nx) + (ny > sy ? ny - sy : sy - ny);
                    if (manh * 16 > 16) {
                        last_tx = nx; last_ty = ny;
                        ++reachable;
                        if (reachable >= 3) { resume_tx = nx; resume_ty = ny; break; }
                    }
                }
            }
            if (resume_tx < 0) { resume_tx = last_tx; resume_ty = last_ty; }
        }
        // ① 从 resume 格到最终目标全量规划（跳过被挡段继续原方向）
        NavPath np;
        if (resume_tx >= 0 &&
            nav_bfs(resume_tx, resume_ty, n->target_tx, n->target_ty, np, true) &&
            np.dir_count > 0 && np.found) {
            n->replan_count++;
            n->dir_count = np.dir_count;
            n->dir_idx = -1;
            n->target_px = px;
            n->target_py = py;
            for (int i = 0; i < np.dir_count; ++i) n->dirs[i] = static_cast<int8_t>(np.dirs[i]);
            return !map_link_check(n->ch);
        }
        // ② 回退：从当前格到最终目标全量规划
        if (nav_bfs(cpx, cpy, n->target_tx, n->target_ty, np, true) &&
            np.dir_count > 0 && np.found) {
            n->replan_count++;
            n->dir_count = np.dir_count;
            n->dir_idx = -1;
            n->target_px = px;
            n->target_py = py;
            for (int i = 0; i < np.dir_count; ++i) n->dirs[i] = static_cast<int8_t>(np.dirs[i]);
            return !map_link_check(n->ch);
        }
        // 全量规划失败：终止任务（v0.4.51：不再多余尝试）
        return false;
    }
    n->replan_count = 0;
    return !map_link_check(n->ch);
}

}  // namespace

std::string data_op_move(int32_t x, int32_t y) {
    if (!game_in_world()) return op_err("not in game");
    // v0.4.41：药水教学激活时拒绝移动并取消挂起移动（游戏劫持按键禁移动，API 需同步）
    if (const char* tb = tutorial_block_error()) {
        stop_all_tasks();
        return op_err(tb);
    }
    void* ch = member_or_null(0);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_move == nullptr) return op_err("symbol not resolved");
    int px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_Y);
    // v0.4.29 自研 BFS 寻路（替代 CHAR_SearchPath：游戏寻路不能绕远路）
    // v0.4.45：全局规划无视动态单位（use_units=false）——只走静态地形，路径短；撞墙时局部短途绕行
    // v0.4.47：全局规划改回 use_units=true——use_units=false 路径穿过 type==2 单位（火把/装饰）
    //   导致 CHAR_Move 撞墙卡死（真机实测 tile(19,20) 目标 tile(24,20) 有 slot7 单位占据）。
    NavPath np;
    if (!nav_bfs(px >> 4, py >> 4, x >> 4, y >> 4, np, true) || np.dir_count == 0)
        return op_err("no path");
    // 注册导航帧任务：方向序列逐帧 CHAR_Move 驱动（NavCtx 静态实例，单任务语义）
    // 目标不可达（如 NPC 被视作阻挡）→ 走到最近可达点后转身面向目标（v0.4.29 改进）
    static NavCtx nav_ctx;
    nav_ctx.ch = ch;
    nav_ctx.dir_count = np.dir_count;
    nav_ctx.dir_idx = -1;
    nav_ctx.target_px = px;
    nav_ctx.target_py = py;
    nav_ctx.target_tx = x >> 4;
    nav_ctx.target_ty = y >> 4;
    nav_ctx.face_target = !np.found;
    nav_ctx.final_tx = x >> 4;
    nav_ctx.final_ty = y >> 4;
    nav_ctx.replan_count = 0;
    for (int i = 0; i < np.dir_count; ++i) nav_ctx.dirs[i] = static_cast<int8_t>(np.dirs[i]);
    if (frame_task_register(nav_task_tick, &nav_ctx) == 0) return op_err("move start failed");
    return op_ok();
}

std::string data_op_walk(int32_t direction) {
    if (!game_in_world()) return op_err("not in game");
    // v0.4.41：药水教学激活时拒绝移动并取消挂起移动（游戏劫持按键禁移动，API 需同步）
    if (const char* tb = tutorial_block_error()) {
        stop_all_tasks();
        return op_err(tb);
    }
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

// 交互/攻击键（v0.4.41）：复现官方攻击键链（GAMESTATE_PressKeyPlay 0x9d2e4 分支）。
// 官方：玩家行走中按攻击键 → CHAR_StartActionID(ch,0x2) → 正对目标时调 EVTSYSTEM_DoCheckAllEvent(2)
// → 遍历所有未激活事件 CheckCondition（含 quest_state_bits/靠近 NPC 等）→ 条件满足 SetReady 激活。
// 用于路障/宝箱等交互物（如影子丛林2 中上路障 = 事件 13 quest_state_bits=381，走近正对后触发）。
std::string data_op_interact() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_evtsystem_do_check_all_event == nullptr) return op_err("symbol not resolved");
    fn_evtsystem_do_check_all_event(2);
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
    // 用药成功且药水教学激活（obj170==6）→ 复现官方 0xec340 教学完成链（CHAR_ProcessShortcut 用药后检查）
    if (ok && tutorial_state() == 6) tutorial_cancel();
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
