// game_ops_action.cpp —— 由 game_data.cpp 拆分生成（纯搬代码，零逻辑变更）

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
#include "game_ops_action.h"
#include "game_nav.h"
#include "game_state.h"
#include "game_cache.h"
#include "game_motion.h"
#include "game_json.h"
#include "game_read.h"
#include "game_misc.h"

// ---- 辅助定义（move/walk/战斗/背包共用，前置以满足使用顺序）----
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
bool map_link_check(void* ch) {
    if (fn_go_map_link_by_char == nullptr || ch == nullptr) return false;
    int16_t px = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_X);
    int16_t py = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(ch) + C_POS_Y);
    return fn_go_map_link_by_char(ch, px >> 4, py >> 4) != 0;
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
    int8_t dirs[NAV_MAX_DIRS];
};

bool walk_task_tick(void* ctx) {
    WalkCtx* w = static_cast<WalkCtx*>(ctx);
    if (fn_char_move == nullptr || w == nullptr) return false;
    // v0.4.37：剧情/切图触发（GAMESTATE_nState!=0）立即自终止——避免后台线程继续
    // CHAR_Move 与剧情状态机竞争（真机实测：剧情结束后触摸无法移动、怪无法攻击）
    if (g_gamestate != nullptr && *reinterpret_cast<uint32_t*>(g_gamestate) != 0) return false;
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
    return --w->remaining > 0 && !map_link_check(w->ch);  // 走完 60 帧或切图终止
}
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


std::string data_op_stat_reset(int role) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_initialize_status == nullptr) return op_err("symbol not resolved");
    fn_char_initialize_status(ch);
    return op_ok();
}
std::string data_op_skill_reset(int role) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_char_initialize_skill == nullptr) return op_err("symbol not resolved");
    fn_char_initialize_skill(ch);
    return op_ok();
}
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
std::string data_op_quest_quit(int32_t quest_id) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_questsystem_find == nullptr || fn_questsystem_remove_slot == nullptr)
        return op_err("symbol not resolved");
    int slot = fn_questsystem_find(quest_id);
    if (slot < 0) return op_err("quest not found");
    int r = fn_questsystem_remove_slot(slot);
    return r ? op_ok() : op_err("quest quit failed");
}
std::string data_op_save() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_save == nullptr) return op_err("symbol not resolved");
    int r = fn_save();
    return r ? op_ok() : op_err("save failed");
}
std::string data_op_main_menu() {
    if (fn_gamestate_set_state == nullptr) return op_err("symbol not resolved");
    fn_gamestate_set_state(4);
    return op_ok();
}
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
    uint8_t** flag_ptr = reinterpret_cast<uint8_t**>(g_base + G_GAME_RESUME_FLAG_GOT_VMA);
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
        case F_PANEL_CHARACTER_INFO_ENTER: case F_PANEL_CHOICE_ENTER: case F_PANEL_INVENTORY_ENTER: case F_PANEL_INPUT_COUNT_ENTER:
        case F_PANEL_MERCENARY_ENTER: case F_PANEL_CRAFT_ENTER: case F_PANEL_NPC_ENTER: case F_PANEL_NPC_QUEST_ENTER:
        case F_PANEL_NPC_REST_ENTER: case F_PANEL_NPC_REVIVE_ENTER: case F_PANEL_OPTIONS_ENTER: case F_PANEL_QUESTS_ENTER:
        case F_PANEL_SAVE_SLOT_ENTER: case F_PANEL_CHAR_SELECT_ENTER: case F_PANEL_SHORTCUT_ENTER: case F_PANEL_SKILLS_ENTER:
        case F_PANEL_SHOP_ENTER: case F_PANEL_SETTINGS_ENTER: case F_PANEL_WIPEOUT_ENTER: case F_PANEL_WORLD_MAP_ENTER:
        case F_PANEL_IN_APP_ENTER: case F_PANEL_DAILY_REWARD_ENTER:
        case F_PANEL_UNK1_ENTER: case F_PANEL_UNK2_ENTER: case F_PANEL_UNK3_ENTER:
        case F_PANEL_UNK4_ENTER: case F_PANEL_UNK5_ENTER:
            is_panel = true;
            break;
        default: break;
    }
    if (!is_panel) return op_err("top of stack is not a panel");
    // 官方 ButtonBackExe 链：SOUNDSYSTEM_Play(0) + 流程3 + HUD 开关恢复
    fn_ui_set_popup_process_info(3, 0);
    uint8_t** hud_gate = reinterpret_cast<uint8_t**>(g_base + G_HUD_GATE_GOT_VMA);
    if (hud_gate != nullptr && *hud_gate != nullptr) **hud_gate = 1;
    return op_ok();
}
std::string data_op_panel_open(const std::string& panel) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    if (g_base == 0) return op_err("libgame not ready");
    // 面板名 → enter VMA（与 data_gamestate_json 的 PANELS 映射一致）
    uintptr_t target = 0;
    if (panel == "character_info") target = F_PANEL_CHARACTER_INFO_ENTER;
    else if (panel == "choice") target = F_PANEL_CHOICE_ENTER;
    else if (panel == "inventory") target = F_PANEL_INVENTORY_ENTER;
    else if (panel == "input_count") target = F_PANEL_INPUT_COUNT_ENTER;
    else if (panel == "mercenary") target = F_PANEL_MERCENARY_ENTER;
    else if (panel == "craft") target = F_PANEL_CRAFT_ENTER;
    else if (panel == "npc") target = F_PANEL_NPC_ENTER;
    else if (panel == "npc_quest") target = F_PANEL_NPC_QUEST_ENTER;
    else if (panel == "npc_rest") target = F_PANEL_NPC_REST_ENTER;
    else if (panel == "npc_revive") target = F_PANEL_NPC_REVIVE_ENTER;
    else if (panel == "options") target = F_PANEL_OPTIONS_ENTER;
    else if (panel == "quests") target = F_PANEL_QUESTS_ENTER;
    else if (panel == "save_slot") target = F_PANEL_SAVE_SLOT_ENTER;
    else if (panel == "character_select") target = F_PANEL_CHAR_SELECT_ENTER;
    else if (panel == "shortcut") target = F_PANEL_SHORTCUT_ENTER;
    else if (panel == "skills") target = F_PANEL_SKILLS_ENTER;
    else if (panel == "shop") target = F_PANEL_SHOP_ENTER;
    else if (panel == "settings") target = F_PANEL_SETTINGS_ENTER;
    else if (panel == "wipeout") target = F_PANEL_WIPEOUT_ENTER;
    else if (panel == "world_map") target = F_PANEL_WORLD_MAP_ENTER;
    else if (panel == "in_app") target = F_PANEL_IN_APP_ENTER;
    else if (panel == "daily_reward") target = F_PANEL_DAILY_REWARD_ENTER;
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
std::string data_recover_after_hive_block() {
    if (g_base == 0) return op_err("base not ready");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    if (fn_networkstore_set_state == nullptr) return op_err("symbol not resolved");
    uint32_t** daily_trigger = reinterpret_cast<uint32_t**>(g_base + G_DAILY_TRIGGER_GOT_VMA);
    if (*daily_trigger != nullptr) **daily_trigger = 1;
    uint8_t** hud_gate = reinterpret_cast<uint8_t**>(g_base + G_HUD_GATE_GOT_VMA);
    if (*hud_gate != nullptr) **hud_gate = 1;
    fn_networkstore_set_state(0);
    fn_ui_set_popup_process_info(4, 0);
    return op_ok();
}
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
std::string data_op_npc_dialog_next() {
    if (!game_in_world()) return op_err("not in game");
    if (fn_npctasklist_make_dlg == nullptr) return op_err("symbol not resolved");
    char* text = fn_npctasklist_make_dlg();
    if (text == nullptr) return op_err("no dialog");
    return "{\"ok\":true,\"text\":\"" + json_escape(text) + "\"}";
}
std::string data_op_npc_dialog_select(int index) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_uinpc_exe_current_task == nullptr) return op_err("symbol not resolved");
    uint8_t count = *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_COUNT_VMA);
    if (index < 0 || index >= count) return op_err("bad index");
    *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_INDEX_VMA) = static_cast<uint8_t>(index);
    fn_uinpc_exe_current_task();
    return op_ok();
}
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
std::string data_op_party_swap(int32_t a, int32_t b) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_party_swap == nullptr) return op_err("symbol not resolved");
    if (a < 0 || a > 2 || b < 0 || b > 2) return op_err("bad slot");
    fn_party_swap(a, b);
    return op_ok();
}
std::string data_op_move(int32_t x, int32_t y) {
    if (!game_in_world()) return op_err("not in game");
    // v0.4.56：药水教学暂停自动取消（视为完成）后放行；早期 v0.4.41 在此拒绝
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
    // v0.4.56：药水教学暂停自动取消（视为完成）后放行；早期 v0.4.41 在此拒绝
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
    walk_ctx.first_tick = true;   // v0.4.57：首帧缓冲
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
std::string data_op_withdraw(int mercenary_slot, int32_t equip_slot) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_unequip == nullptr) return op_err("symbol not resolved");
    void* ch = find_char_by_merc_slot(mercenary_slot);
    if (ch == nullptr) return op_err("mercenary not found");
    if (equip_slot < 0 || equip_slot >= C_EQUIP_SLOTS) return op_err("bad slot");
    int r = fn_unequip(ch, equip_slot);
    return r ? op_ok() : op_err("withdraw failed");
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
std::string story_next() {
    if (g_base == 0) return op_err("not in game");
    if (fn_event_button_ok_exe == nullptr) return op_err("symbol not resolved");
    fn_event_button_ok_exe();
    return op_ok();
}
std::string story_skip() {
    if (g_base == 0) return op_err("not in game");
    if (fn_event_button_skip_exe == nullptr) return op_err("symbol not resolved");
    fn_event_button_skip_exe();
    return op_ok();
}
