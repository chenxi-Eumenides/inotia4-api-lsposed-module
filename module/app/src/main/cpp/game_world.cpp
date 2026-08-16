// game_world.cpp —— 世界域：剧情状态 + 自研 BFS 路径/距离/调试路径（parse 域）
// 由 game_misc.cpp 拆分生成（纯搬代码，零逻辑变更）。

#include "game_world.h"

#include "game_access.h"
#include "game_json.h"
#include "game_nav.h"
#include "game_state.h"
#include "game_ops_common.h"
#include "game_motion.h"
#include "game_tiles.h"

#include <android/log.h>
#include <cstdint>
#include <vector>

#define MOVE_TAG "Inotia4Move"
#define MOVE_LOG(...) __android_log_print(ANDROID_LOG_INFO, MOVE_TAG, __VA_ARGS__)

bool data_story_active() {
    if (g_base == 0) return false;
    uint32_t gs = g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
    return gs == 1;
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
                    if (obj[C_SITUATION] != 1) continue;
                    if (!pool_obj_valid(obj)) continue;
                    if (obj == hero) continue;
                    int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
                    int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
                    int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
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

// 当前 GAMESTATE_nState；g_gamestate 未解析视为 0（world）
static uint32_t gamestate_state() {
    return g_gamestate != nullptr ? *reinterpret_cast<uint32_t*>(g_gamestate) : 0;
}
// 复刻 CHAR_PickItemAll 数据路径：遍历掉落物数组 + 范围判断 + NOTIFIER_Add 排入主线程回调，
// 跳过其末尾的 SOUNDSYSTEM_Play 拾取音效（后台线程音频引擎句柄为空 → fault 0x0 崩溃）。
// 主线程 NOTIFIER_Process → CHAR_ActivePickupEvent(0xdd15c) 完成 INVEN_SaveItem 入库 + 移除掉落。
static void nav_pick_items(void* ch, int radius) {
    if (g_base == 0 || ch == nullptr || fn_mem_malloc == nullptr || fn_notifier_add == nullptr) return;
    int8_t* cnt = *reinterpret_cast<int8_t**>(g_base + G_DROP_COUNT_GOT_VMA);
    if (cnt == nullptr || *cnt <= 0) return;
    uint8_t* arr = *reinterpret_cast<uint8_t**>(*reinterpret_cast<void**>(g_base + G_DROP_ARRAY_GOT_VMA));
    if (arr == nullptr) return;
    void* cb = *reinterpret_cast<void**>(g_base + G_NOTIFIER_PICKUP_SLOT_VMA);
    if (cb == nullptr) return;
    int16_t px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_X);
    int16_t py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_Y);
    int seq = 0;
    for (int i = 0; i < static_cast<int>(*cnt); ++i) {
        uint8_t* e = arr + i * 0x20;
        int16_t ex = *reinterpret_cast<int16_t*>(e + 0x8);
        int16_t ey = *reinterpret_cast<int16_t*>(e + 0xa);
        if (ex < px - radius || ex > px + radius) continue;
        if (ey < py - radius || ey > py + radius) continue;
        if (e[0x18] & 0x2) continue;  // bit1 拾取中
        void* node = fn_mem_malloc(0x18);
        if (node == nullptr) continue;
        uint8_t* nd = reinterpret_cast<uint8_t*>(node);
        *reinterpret_cast<void**>(nd + 0x0) = ch;                                            // 玩家 char
        *reinterpret_cast<int32_t*>(nd + 0x8) = px;                                          // 玩家 x
        *reinterpret_cast<int32_t*>(nd + 0xc) = py;                                          // 玩家 y
        *reinterpret_cast<void**>(nd + 0x10) = *reinterpret_cast<void**>(e + 0x0);          // 掉落物对象
        e[0x18] |= 0x2;                                                                      // 拾取中标志
        fn_notifier_add(1, seq, cb, node);
        seq += 2;
    }
}
bool map_link_check(void* ch) {
    if (fn_go_map_link_by_char == nullptr || ch == nullptr) return false;
    int16_t px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_X);
    int16_t py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_Y);
    // v0.5.28 修复：GoMapLinkByChar 实际 4 参 (ch, tx, ty, use_dir)，use_dir=0 走 MAP_FindMapLinkNoDir
    // （只查出口 tile 坐标，不查角色朝向）。此前 3 参调用致 use_dir 为垃圾值 → CheckMapLink 走
    // MAP_FindMapLink 按朝向匹配 → 朝向不符返回 null → 出口 tile 概率性不切图（真机 (36,26)/(37,26)）。
    return fn_go_map_link_by_char(ch, px >> 4, py >> 4, 0) != 0;
}
struct WalkCtx {
    void* ch;
    int dir;
    int remaining;
    bool first_tick;   // v0.4.57：首帧缓冲标志（帧驱动下注册瞬间即执行第一步）
};
struct NavCtx {
    void* ch = nullptr;
    int dir_count = 0;
    int dir_idx = 0;
    int target_px = 0, target_py = 0;
    int target_tx = -1, target_ty = -1;
    bool face_target = false;
    int final_tx = -1, final_ty = -1;
    int replan_count = 0;
    int wait_frames = 0;   // v0.5.27：走到最近可达点后等待动态单位移开的帧计数
    int8_t dirs[NAV_MAX_DIRS];
    // v0.5.36：原始路径（首次 BFS 结果）——撞墙绕行后仅回到「未走过的后续格」才清零 replan_count
    int8_t orig_dirs[NAV_MAX_DIRS];
    int orig_dir_count = 0;
    int orig_sx = -1, orig_sy = -1;  // 原始路径起点 tile
    int orig_progress = 0;           // 原路径上已推进到的最大下标（0=起点，已走过）
};

// 玩家 tile 在原始路径上的下标（0=起点）；不在原路径上返回 -1
static int on_orig_path(const NavCtx* n, int tx, int ty) {
    int cx = n->orig_sx, cy = n->orig_sy;
    if (cx == tx && cy == ty) return 0;
    for (int i = 0; i < n->orig_dir_count; ++i) {
        cx += NAV_DX[n->orig_dirs[i]];
        cy += NAV_DY[n->orig_dirs[i]];
        if (cx == tx && cy == ty) return i + 1;
    }
    return -1;
}

bool walk_task_tick(void* ctx) {
    WalkCtx* w = static_cast<WalkCtx*>(ctx);
    if (fn_char_move == nullptr || w == nullptr) return false;
    // v0.4.37 起在剧情/切图态禁止后台线程继续 CHAR_Move（与状态机竞争会破坏控制态）。
    // P0 修复：剧情态(EVENT=1)由 CHAR_Move 内部触发，需暂停等剧情播放完再切图；
    // 切图态(MAP_CHANGE=3)路径已失效，立即终止。
    uint32_t gs = gamestate_state();
    if (gs == GAMESTATE_MAP_CHANGE) return false;
    if (gs != 0) return true;
    // v0.5.43：UI 占据（对话框/面板打开）时暂停移动帧任务（与剧情态语义对齐）
    if (ui_blocked() != nullptr) return true;
    // v0.4.57 首帧缓冲：帧驱动下注册瞬间即执行第一步，此时角色可能处于
    // 上一操作收尾状态（CHAR_Move 状态未复位）——首帧仅设朝向，下一帧才走（与 nav_task_tick 对齐）
    if (w->first_tick) {
        w->first_tick = false;
        if (fn_char_set_direction != nullptr) fn_char_set_direction(w->ch, w->dir);
        return true;
    }
    // flag=0：CHAR_Move 内部自动 MAP_SetFocus 跟随摄像机。
    // 返回值：0=正常走一步（成功），非 0=撞墙/阻挡（反汇编 e98dc mov w20,#0x1）
    // v0.4.40：CHAR_Move 不更新朝向（官方链=按键→SetDirection+Move 分开调），移动前先设朝向避免"飘逸"
    if (fn_char_set_direction != nullptr) fn_char_set_direction(w->ch, w->dir);
    if (fn_char_move(w->ch, w->dir, 8, 0)) return false;  // 撞墙/不可走
    // CHAR_Move 内部剧情检查可能已触发 SetReady→SetState(1)（剧情态）：跳过切图检查，
    // 让剧情先播放（对齐官方"先剧情后切图"），剧情结束后下一帧恢复再继续。
    if (gamestate_state() != 0) return true;
    // 对齐官方移动按键链 GAMESTATE_PressKeyPlay 0x9d10c：移动后拾取脚下掉落物（半径 0x18=24px）。
    nav_pick_items(w->ch, 0x18);
    return --w->remaining > 0 && !map_link_check(w->ch);  // 走完 60 帧或切图终止
}
bool nav_task_tick(void* ctx) {
    NavCtx* n = static_cast<NavCtx*>(ctx);
    if (n == nullptr || n->ch == nullptr || fn_char_move == nullptr) return false;
    // v0.4.37 起在剧情/切图态禁止后台线程继续 CHAR_Move（与状态机竞争会破坏控制态）。
    // P0 修复：剧情态(EVENT=1)由 CHAR_Move 内部触发，需暂停等剧情播放完再切图；
    // 切图态(MAP_CHANGE=3)路径已失效，立即终止。
    uint32_t gs = gamestate_state();
    if (gs == GAMESTATE_MAP_CHANGE) return false;
    if (gs != 0) return true;
    // v0.5.43：UI 占据（对话框/面板打开）时暂停导航帧任务（与剧情态语义对齐）
    if (ui_blocked() != nullptr) return true;
    uint8_t* ch = reinterpret_cast<uint8_t*>(n->ch);
    int px = *reinterpret_cast<int16_t*>(ch + C_POS_X);
    int py = *reinterpret_cast<int16_t*>(ch + C_POS_Y);
    // v0.5.27：等待重试态——走到最近可达点后等待动态单位移开，逐帧重试 BFS 接续
    if (n->wait_frames > 0) {
        NavPath retry;
        if (nav_bfs(px >> 4, py >> 4, n->target_tx, n->target_ty, retry, true) &&
            retry.dir_count > 0 && retry.found) {
            n->replan_count++;
            n->dir_count = retry.dir_count;
            n->dir_idx = -1;
            n->face_target = false;
            n->wait_frames = 0;
            n->target_px = px;
            n->target_py = py;
            for (int i = 0; i < retry.dir_count; ++i)
                n->dirs[i] = static_cast<int8_t>(retry.dirs[i]);
            MOVE_LOG("replan: wait-retry ok dirs=%d", retry.dir_count);
            return !map_link_check(n->ch);
        }
        ++n->wait_frames;
        if (n->wait_frames > 60) {
            n->wait_frames = 0;
            MOVE_LOG("replan: wait timeout at (%d,%d)", px >> 4, py >> 4);
            return false;
        }
        return true;   // 继续等待（保持帧任务存活）
    }
    if ((px - n->target_px < 8 && n->target_px - px < 8) &&
        (py - n->target_py < 8 && n->target_py - py < 8)) {
        n->dir_idx++;
        if (n->dir_idx >= n->dir_count) {
            if (n->face_target && n->final_tx >= 0) {
                // v0.5.27：走到最近可达点（目标被动态单位/静态阻挡）后，转身面向目标并
                // 进入等待重试态（wait_frames），等待动态单位（怪）移开后逐帧重试 BFS 接续，
                // 而非立即终止——修复"重规划卡死"（怪挡路时玩家停在原地不动、move_to 无法继续）。
                int dx = n->final_tx - (px >> 4), dy = n->final_ty - (py >> 4);
                int face_dir = (dx < 0) ? 1 : (dx > 0) ? 3 : (dy > 0) ? 0 : 2;
                if (fn_char_set_direction != nullptr) fn_char_set_direction(n->ch, face_dir);
                fn_char_move(n->ch, face_dir, 8, 0);
                n->face_target = false;
                n->wait_frames = 1;
                MOVE_LOG("replan: reached nearest (%d,%d), waiting", px >> 4, py >> 4);
                return true;
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
        // 撞墙/被动态单位阻挡 → 从当前格 BFS 绕行（排除撞墙格，见下 wall_tx/wall_ty）。
        if (n->target_tx < 0 || n->replan_count >= 5) {
            MOVE_LOG("replan: abort target=(%d,%d) replan=%d", n->target_tx, n->target_ty, n->replan_count);
            return false;
        }
        int cpx = px >> 4, cpy = py >> 4;
        // 撞墙格：撞墙方向的目标格——模块判可走但引擎 CHAR_Move 判阻挡（碰撞建模差异），
        // 重规划 BFS 临时排除它，避免规划又走同格反复撞墙（v0.5.27 变体根因缓解）。
        int wall_tx = -1, wall_ty = -1;
        int wall_dir = (n->dir_idx >= 0 && n->dir_idx < n->dir_count) ? n->dirs[n->dir_idx] : -1;
        if (wall_dir >= 0) { wall_tx = cpx + NAV_DX[wall_dir]; wall_ty = cpy + NAV_DY[wall_dir]; }
        MOVE_LOG("replan: hit wall at (%d,%d) dir=%d replan=%d wall=(%d,%d)", cpx, cpy,
                 wall_dir, n->replan_count, wall_tx, wall_ty);
        // v0.5.27：撞墙后从当前格 BFS 绕行（排除撞墙格）——废除 v0.4.53 的 resume 探测：
        // resume 格在障碍后方（玩家到不了），其路径起点与玩家位置错位，found=true 也会反复撞墙。
        // found=false（目标被动态单位封死）时不再直接终止——改用 nearest 最近可达点接续
        // （走到障碍物面前转身面向目标并等待重试），而非停在撞墙格不动（修复"重规划卡死"）。
        NavPath np;
        if (nav_bfs(cpx, cpy, n->target_tx, n->target_ty, np, true, 0, wall_tx, wall_ty) &&
            np.dir_count > 0) {
            n->replan_count++;
            n->dir_count = np.dir_count;
            n->dir_idx = -1;
            n->target_px = px;
            n->target_py = py;
            n->face_target = !np.found;
            for (int i = 0; i < np.dir_count; ++i) n->dirs[i] = static_cast<int8_t>(np.dirs[i]);
            MOVE_LOG("replan: cur-path ok dirs=%d found=%d nearest=(%d,%d)",
                     np.dir_count, np.found ? 1 : 0, np.nearest_x, np.nearest_y);
            return !map_link_check(n->ch);
        }
        // 全量规划失败：终止任务（v0.4.51：不再多余尝试）
        MOVE_LOG("replan: fail, terminate at (%d,%d)", cpx, cpy);
        return false;
    }
    // v0.5.36：成功走一步后仅当回到原始路径「未走过的后续格」才清零重试计数——
    // 绕行未回归原路径（或回到已走过的格子）时计数持续累积，防 A→B→A 撞墙振荡无限重规划
    // 注意用移动后坐标（px/py 为本帧移动前读取）
    int npx = *reinterpret_cast<int16_t*>(ch + C_POS_X);
    int npy = *reinterpret_cast<int16_t*>(ch + C_POS_Y);
    int oi = on_orig_path(n, npx >> 4, npy >> 4);
    if (oi > n->orig_progress) {
        n->orig_progress = oi;
        n->replan_count = 0;
    }
    // CHAR_Move 内部剧情检查可能已触发 SetReady→SetState(1)（剧情态）：跳过切图检查，
    // 让剧情先播放（对齐官方"先剧情后切图"），剧情结束后下一帧恢复再继续。
    if (gamestate_state() != 0) return true;
    // 对齐官方移动按键链 GAMESTATE_PressKeyPlay 0x9d10c：移动后拾取脚下掉落物（半径 0x18=24px）。
    nav_pick_items(n->ch, 0x18);
    return !map_link_check(n->ch);
}

std::string data_op_teleport(int32_t map_id, int32_t x, int32_t y) {
    if (!game_in_world()) return op_err("not in game");
    if (const char* ui = ui_blocked()) {
        std::string err = "ui occupied: ";
        err += ui;
        return op_err(err.c_str());
    }
    if (fn_change_map == nullptr || fn_set_position == nullptr)
        return op_err("symbol not resolved");
    if (map_id > 0) {
        fn_change_map(map_id, x, y, 0);
    } else if (fn_set_position != nullptr) {
        fn_set_position(x, y);
    }
    return op_ok();
}
std::string data_op_move(int32_t x, int32_t y) {
    if (!game_in_world()) return op_err("not in game");
    // v0.4.56：药水教学暂停自动取消（视为完成）后放行；早期 v0.4.41 在此拒绝
    if (const char* tb = tutorial_block_error()) {
        stop_all_tasks();
        return op_err(tb);
    }
    // v0.5.43：UI 占据（对话框/面板/教学）时世界操作阻塞——游戏输入被 UI 接管
    if (const char* ui = ui_blocked()) {
        stop_all_tasks();
        std::string err = "ui occupied: ";
        err += ui;
        return op_err(err.c_str());
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
    nav_ctx.wait_frames = 0;
    // v0.5.36：保存首次 BFS 原始路径，供 nav_task_tick 判断「回到原路径后续格」才清零重试计数
    nav_ctx.orig_dir_count = np.dir_count;
    nav_ctx.orig_sx = px >> 4;
    nav_ctx.orig_sy = py >> 4;
    nav_ctx.orig_progress = 0;
    for (int i = 0; i < np.dir_count; ++i) nav_ctx.orig_dirs[i] = static_cast<int8_t>(np.dirs[i]);
    for (int i = 0; i < np.dir_count; ++i) nav_ctx.dirs[i] = static_cast<int8_t>(np.dirs[i]);
    if (frame_task_register(nav_task_tick, &nav_ctx) == 0) return op_err("move start failed");
    return op_ok();
}
std::string data_op_walk(int32_t direction) {
    if (!game_in_world()) return op_err("not in game");
    // v0.4.56：药水教学暂停自动取消（视为完成）后放行；早期 v0.4.41 在此拒绝
    if (const char* tb = tutorial_block_error()) {
        stop_all_tasks();
        return op_err(tb);
    }
    // v0.5.43：UI 占据阻塞
    if (const char* ui = ui_blocked()) {
        stop_all_tasks();
        std::string err = "ui occupied: ";
        err += ui;
        return op_err(err.c_str());
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
    walk_ctx.first_tick = true;   // v0.4.57：首帧缓冲
    if (frame_task_register(walk_task_tick, &walk_ctx) == 0) return op_err("walk start failed");
    return op_ok();
}
std::string data_op_walk_stop() {
    if (!game_in_world()) return op_err("not in game");
    if (const char* ui = ui_blocked()) {
        std::string err = "ui occupied: ";
        err += ui;
        return op_err(err.c_str());
    }
    void* ch = member_or_null(0);
    if (ch == nullptr) return op_err("role not found");
    stop_all_tasks();
    if (fn_char_remove_path == nullptr) return op_err("symbol not resolved");
    fn_char_remove_path(ch);
    return op_ok();
}
std::string data_op_interact() {
    if (!game_in_world()) return op_err("not in game");
    if (const char* ui = ui_blocked()) {
        std::string err = "ui occupied: ";
        err += ui;
        return op_err(err.c_str());
    }
    if (fn_evtsystem_do_check_all_event == nullptr) return op_err("symbol not resolved");
    fn_evtsystem_do_check_all_event(2);
    return op_ok();
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
                if (!pool_obj_valid(obj)) continue;
                int16_t x = *reinterpret_cast<int16_t*>(obj + C_POS_X);
                int16_t y = *reinterpret_cast<int16_t*>(obj + C_POS_Y);
                int type = static_cast<int>(reinterpret_cast<int8_t*>(obj)[C_TYPE]);
                uint8_t status = obj[C_STATUS];
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
