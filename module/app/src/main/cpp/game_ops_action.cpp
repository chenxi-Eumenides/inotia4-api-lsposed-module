// game_ops_action.cpp —— 由 game_data.cpp 拆分生成（纯搬代码，零逻辑变更）

#include "game_data.h"

#include "game_access.h"
#include "game_ptr_hook.h"
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

#include <sys/mman.h>
#include <unistd.h>

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
    if (fn_get_bit == nullptr || fn_get_cumulate_count == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, "Inotia4Export", "inventory_gained_json: fn_get_bit/fn_get_cumulate_count not resolved");
        return std::string();
    }
    std::string s;
    int n = 0;
    for (int b = 0; b < 6; ++b) {
        uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * 0x80;
        for (int j = 0; j < 16; ++j) {
            void* item = *reinterpret_cast<void**>(bag_slots + j * 8);
            if (item == nullptr) continue;
            // ITEM_GetCumulateCount 自动适配 patch：可堆叠返回数量、装备/不可堆叠返回 1（已归一化）
            int count = fn_get_cumulate_count(item);
            void* old = before[b * 16 + j];
            if (old == item) {
                count -= fn_get_cumulate_count(old);
                if (count <= 0) continue;
            } else if (old != nullptr) {
                continue;  // 同槽不同指针：旧物品被消耗/替换，非新增
            }
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
    n->replan_count = 0;
    return !map_link_check(n->ch);
}
void* pool_slot_obj(int slot) {
    if (g_base == 0 || slot < 0 || slot >= C_CHARSYSTEM_POOL_SLOTS) return nullptr;
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
    if (g_state == nullptr) return op_err("libgame not ready");
    uint16_t st = *reinterpret_cast<uint16_t*>(g_state);
    if (st == 5) return op_err("already in game");
    // 前置检查（v0.5.7）：仅主菜单（STATE==4）可进档。loading/切换态（STATE=0xFFFF）下
    // GAME_Initialize 未完成，GAME_StartResumeGame→ASSYSTEM_Initialize 空指针崩溃
    // （真机 tombstone 实测：ASNODE_Initialize+4 fault addr 0xe）
    if (st != 4) {
        std::string e = "not in main menu (state=" + std::to_string(st) + ")";
        return op_err(e.c_str());
    }
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
// v0.4.64：创建新角色存档（复刻官方 SaveSlot_GoToNewGame + SelectCharacter_ButtonStartExe 链，
// frida 全流程监听实证，见 docs/systems/save.md §10）
std::string data_op_create_slot(int32_t slot, int32_t class_idx) {
    if (g_state == nullptr) return op_err("libgame not ready");
    uint16_t st = *reinterpret_cast<uint16_t*>(g_state);
    if (st == 5) return op_err("already in game");
    if (st != 4) {
        std::string e = "not in main menu (state=" + std::to_string(st) + ")";
        return op_err(e.c_str());
    }
    if (slot < 0 || slot > 2) return op_err("bad slot");
    if (class_idx < 0 || class_idx > 5) return op_err("bad class");
    if (g_base == 0) return op_err("libgame not ready");
    if (fn_save_create_save_slot == nullptr || fn_game_exit_save_slot_select_char == nullptr ||
        fn_select_character_start_game == nullptr || fn_tutorial_start == nullptr ||
        fn_save_get_save_file_name == nullptr || fn_cs_fs_remove == nullptr)
        return op_err("symbol not resolved");
    // 槽区初始化（SAVE_CreateSaveSlot 循环加载 3 槽存档到内存，确保槽位状态可用）
    fn_save_create_save_slot();
    // 删除目标槽旧存档文件（SaveSlot_GoToNewGame 官方链：SAVE_GetSaveFileName + CS_fsRemove）
    char fname[128] = {0};
    fn_save_get_save_file_name(slot, fname);
    if (fname[0] != '\0') fn_cs_fs_remove(fname, 1);
    // 当前槽位（SaveSlot_GoToNewGame：*[0x2f4000+0xd20] = slot）
    uint8_t** cur_slot = reinterpret_cast<uint8_t**>(g_base + G_CURRENT_SLOT_GOT_VMA);
    if (cur_slot == nullptr || *cur_slot == nullptr) return op_err("save slot state not ready");
    **cur_slot = static_cast<uint8_t>(slot);
    // 新建标志（SaveSlot_GoToNewGame：*[0x2f6000+0x8] = 1；STATE_EnterGame 检测后走 GAME_StartNewGame）
    uint8_t** newgame_flag = reinterpret_cast<uint8_t**>(g_base + G_GAME_RESUME_FLAG_GOT_VMA);
    if (newgame_flag == nullptr || *newgame_flag == nullptr) return op_err("newgame flag not ready");
    **newgame_flag = 1;
    // 选中职业（SelectCharacter_StartGame 读取源 [0x308080+0x8]）
    *reinterpret_cast<uint32_t*>(g_base + G_SELECTED_CLASS_VMA) = static_cast<uint32_t>(class_idx);
    // 进入选角环境（GAME_Initialize + MAP_Load(6) + MAINMENU_CreateSelectCharList）
    fn_game_exit_save_slot_select_char();
    // 选角确认开始（*[0x2f5000+0xa00] = class_idx + STATE_Set(5) + UI_SetPopupProcessInfo(4,0)）
    fn_select_character_start_game();
    // 新档教学初始化（SelectCharacter_ButtonStartExe：StartGame 后 TutorialStart）
    fn_tutorial_start();
    // 状态机驱动：STATE_NextStartProcess → STATE_EnterGame → GAME_StartNewGame → 剧情 → 初始营地
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
                for (int i = 0; i < C_CHARSYSTEM_POOL_SLOTS; ++i) {
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
    } else if (data_popup_top_vma() == F_PANEL_NPC_QUEST_ENTER) {
        // npc_quest 面板：仅接受 complete（完成任务）/ close（关闭面板）
        if (action != "complete" && action != "close") return op_err("no such option in npc_quest");
    } else if (g_base != 0 &&
               (*reinterpret_cast<uint8_t*>(g_base + G_UICHOICE_COUNT_VMA) > 0 ||
                *reinterpret_cast<uint8_t*>(g_base + G_NPCTASKLIST_COUNT_VMA) > 0)) {
        if (action != "next" && index < 0) return op_err("no such option in npc");
    } else if (data_top_panel_name() != nullptr) {
        // 面板态（v0.5.6 U1）：save_slot 接受 save/close，其余面板仅 close（panel/close 官方流程3）
        if (action != "close" && !(action == "save" && strcmp(data_top_panel_name(), "save_slot") == 0)) {
            std::string err = "no such option in ";
            err += data_top_panel_name();
            return op_err(err.c_str());
        }
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
    if (data_popup_top_vma() == F_PANEL_NPC_QUEST_ENTER) {
        if (action == "complete") {
            if (fn_uinpc_quest_button_ok_exe == nullptr) return op_err("symbol not resolved");
            fn_uinpc_quest_button_ok_exe();
            return op_ok();
        }
        if (action == "close") return data_op_panel_close();
    }
    // wipeout 死亡面板动作（v0.4.35）：栈顶是 wipeout 面板时接受 revive/special_revive/game_over
    if (action == "revive" || action == "special_revive" || action == "game_over") {
        if (data_popup_top_vma() != F_PANEL_WIPEOUT_ENTER) return op_err("not in wipeout");
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
    // 面板态动作（v0.5.6 U1）：save=存档落盘、close=关闭面板（panel/close 官方流程3）
    if (data_top_panel_name() != nullptr) {
        if (action == "save") return data_op_save();
        if (action == "close") return data_op_panel_close();
    }
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
std::string data_op_enchant(int role, int bag, int slot, int equip_slot) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_enchant_item == nullptr || fn_is_enchant_scroll == nullptr || fn_consume_item == nullptr || fn_get_bit == nullptr)
        return op_err("symbol not resolved");
    if (bag < 0 || bag >= 6 || slot < 0 || slot >= 16) return op_err("bad slot");
    if (equip_slot < 0 || equip_slot >= C_EQUIP_SLOTS) return op_err("bad equip slot");
    void* equip = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ch) + C_EQUIP + equip_slot * 8);
    if (equip == nullptr) return op_err("equip slot empty");
    void* scroll = inventory_item_at(bag, slot);
    if (scroll == nullptr) return op_err("scroll not found");
    uint16_t sflags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(scroll) + I_TYPE);
    int scroll_cat = fn_get_bit(sflags, 15, 6);
    if (!fn_is_enchant_scroll(scroll_cat)) return op_err("not enchant scroll");
    // ITEMSYSTEM_EnchantItem 成功(0)后写回 +0x1A 新附魔等级；卷轴消耗由调用方负责（UIEquip_ApplyStuff 同款）
    int r = fn_enchant_item(equip, scroll_cat);
    if (r != 0) return r == 7 ? op_err("not enchantable") : op_err("enchant failed");
    fn_consume_item(scroll);
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
    if (!r) return op_err("switch failed");
    // v0.5.9：PARTY_SetActivePlayer 只写 PLAYER_pActivePlayer，不同步 SAVE_nMainMercenarySlot，
    // 导致 main_mercenary_slot/leader 端点不更新（2026-08-13 实测修复）
    if (g_main_merc_slot != nullptr)
        *reinterpret_cast<uint8_t*>(g_main_merc_slot) = static_cast<uint8_t>(slot);
    return op_ok();
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
    nav_ctx.wait_frames = 0;
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

// ============================================================
// 合成器批量宝石合成 + 自定义 UI 按钮（v0.5.18）
// ============================================================

namespace {

std::mutex g_craft_mtx;            // 注入/还原互斥（串行化 enable/disable）
bool g_craft_injected = false;     // 是否已注入
void* g_craft_orig = nullptr;      // 原宝石按钮 ControlObject*（还原槽指针用）
PtrHook g_craft_exec_hook;         // 原宝石按钮 ExecuteProc 的函数指针 hook（覆盖为批量合成）
void* g_craft_mmap = nullptr;      // mmap 区域（ControlObject + 按钮数据）
size_t g_craft_mmap_len = 0;       // mmap 长度
std::atomic<bool> g_craft_want{false};           // 是否期望注入（配置开关）
std::atomic<bool> g_craft_thread_started{false};

}  // namespace

// 懒注入线程：合成器界面（UIMix）只在玩家打开合成器时才创建（UIMix_CreateMainControl），
// 启动时（main_menu）宝石按钮槽 [0x305550+0xa0] 为空，立即注入会失败。这里后台轮询，
// 槽非空（界面已打开）时注入；关闭配置时由 data_craft_btn_set_enabled(false) 还原。
void ensure_craft_inject_thread() {
    if (g_craft_thread_started.exchange(true)) return;
    std::thread([]() {
        for (;;) {
            if (g_craft_want.load() && !g_craft_injected && g_base != 0 && g_uimix != nullptr) {
                void** slot = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_uimix) + UIMIX_SLOT_GEM_BTN);
                if (*slot != nullptr) {
                    data_craft_btn_inject();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();
}

void data_craft_btn_set_enabled(bool enabled) {
    g_craft_want.store(enabled);
    if (enabled) {
        ensure_craft_inject_thread();
    } else {
        data_craft_btn_remove();
    }
}

// 批量宝石合成（按钮 ExecuteProc 回调，x0=控件对象）。
// 遍历第一袋 16 槽，按宝石类别 cat28-31（排除混沌 32）分组，每组 3 个一批走
// MIXSYSTEM_MakeItem（词条定向继承由游戏自动处理，无需复刻）→ 产物入库 → 消耗 3 材料。
// 失败（配方/背包满/金币不足）安全停止；完成后一次性扣费并静默存档。
void data_op_mix_gem_batch(void* ctrl) {
    (void)ctrl;
    if (g_base == 0 || g_inven == nullptr) {
        MOVE_LOG("mix_gem_batch: libgame not ready");
        return;
    }
    if (!game_in_world()) {
        MOVE_LOG("mix_gem_batch: not in game");
        return;
    }
    if (fn_make_mix == nullptr || fn_get_cost == nullptr || fn_is_jewel == nullptr ||
        fn_remove_item_direct == nullptr || fn_inven_save_item == nullptr ||
        fn_minus_money == nullptr || fn_save == nullptr || fn_get_bit == nullptr ||
        fn_get_money == nullptr) {
        MOVE_LOG("mix_gem_batch: symbol not resolved");
        return;
    }
    // 扫描第一袋，按类别分组记录槽位（cat28-31，排除混沌 32 及异常类别）
    int slot_by_cat[4][16];
    int cnt[4] = {0, 0, 0, 0};
    for (int slot = 0; slot < 16; ++slot) {
        void* item = inventory_item_at(0, slot);
        if (item == nullptr) continue;
        uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
        int cat = fn_get_bit(flags, 15, 6);
        if (!fn_is_jewel(cat)) continue;
        if (cat < 28 || cat > 31) continue;
        int idx = cat - 28;
        if (cnt[idx] < 16) slot_by_cat[idx][cnt[idx]++] = slot;
    }
    int64_t total_cost = 0;
    int made = 0;
    const char* stop_reason = nullptr;
    for (int idx = 0; idx < 4 && stop_reason == nullptr; ++idx) {
        int mix_type = 12 + idx;  // 12/13/14/15 = 低级/中级/高级/顶级 → 上一级
        int used = 0;
        while (cnt[idx] - used >= 3) {
            int64_t cost = fn_get_cost(mix_type, nullptr);
            if (cost < 0) { stop_reason = "cost failed"; break; }
            if (fn_get_money() < total_cost + cost) { stop_reason = "not enough money"; break; }
            void* out = nullptr;
            if (fn_make_mix(mix_type, &out) != 0 || out == nullptr) {
                stop_reason = "make item failed";
                break;
            }
            if (!fn_inven_save_item(out, nullptr)) {
                // 产物入库失败（背包满）：out 未入库，仅单个对象泄漏（每次点击至多 1 个），停止
                stop_reason = "inventory full";
                break;
            }
            // 成功：消耗 3 材料（RemoveItemDirect 仅清空该槽，不移动其它槽，槽位索引保持有效）
            for (int k = 0; k < 3; ++k) fn_remove_item_direct(0, slot_by_cat[idx][used + k]);
            used += 3;
            total_cost += cost;
            ++made;
        }
    }
    if (total_cost > 0) fn_minus_money(total_cost);
    if (made > 0) fn_save();
    MOVE_LOG("mix_gem_batch: made=%d cost=%lld%s%s", made, static_cast<long long>(total_cost),
             stop_reason != nullptr ? " stop=" : "", stop_reason != nullptr ? stop_reason : "");
}

// 注入批量合成按钮（方案 2：mmap 新建 ControlObject + 按钮数据，复用 [0x3055f0] 宝石按钮槽）。
// 绘制（UIMix_Draw）硬编码枚举固定槽 → 写新指针后即显示新按钮；
// 点击（ControlObject_EventProc）递归控件树遍历，原宝石按钮仍在树中 → 同时覆盖原按钮
// ExecuteProc 为批量合成，保证点击触发本函数。
bool data_craft_btn_inject() {
    std::lock_guard<std::mutex> lock(g_craft_mtx);
    if (g_craft_injected) return true;
    if (g_base == 0 || g_uimix == nullptr) {
        MOVE_LOG("craft_btn_inject: libgame not ready");
        return false;
    }
    void** slot = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_uimix) + UIMIX_SLOT_GEM_BTN);
    uint8_t* orig = reinterpret_cast<uint8_t*>(*slot);
    if (orig == nullptr) {
        MOVE_LOG("craft_btn_inject: gem button slot empty");
        return false;
    }
    uint8_t* orig_data = *reinterpret_cast<uint8_t**>(orig + CO_DATA);
    if (orig_data == nullptr) {
        MOVE_LOG("craft_btn_inject: gem button data empty");
        return false;
    }
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) page = 4096;
    size_t total = CO_SIZE + CB_SIZE;
    size_t len = (total + static_cast<size_t>(page) - 1) / static_cast<size_t>(page) * static_cast<size_t>(page);
    void* region = mmap(nullptr, len, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        MOVE_LOG("craft_btn_inject: mmap failed");
        return false;
    }
    uint8_t* co = reinterpret_cast<uint8_t*>(region);   // 新 ControlObject
    uint8_t* cb = co + CO_SIZE;                          // 新按钮数据
    memset(co, 0, CO_SIZE);
    memset(cb, 0, CB_SIZE);

    // 复刻原宝石按钮 rect/贴图，替换点击回调为批量合成
    *reinterpret_cast<uint32_t*>(co + CO_TYPE) = 3;
    *reinterpret_cast<uint32_t*>(co + CO_ACTIVE) = 0x20;
    *reinterpret_cast<int64_t*>(co + CO_RECT_X) = *reinterpret_cast<int64_t*>(orig + CO_RECT_X);
    *reinterpret_cast<int64_t*>(co + CO_RECT_Y) = *reinterpret_cast<int64_t*>(orig + CO_RECT_Y);
    *reinterpret_cast<int64_t*>(co + CO_RECT_W) = *reinterpret_cast<int64_t*>(orig + CO_RECT_W);
    *reinterpret_cast<int64_t*>(co + CO_RECT_H) = *reinterpret_cast<int64_t*>(orig + CO_RECT_H);
    *reinterpret_cast<uint64_t*>(co + CO_USERTYPE) = 1;
    *reinterpret_cast<void**>(co + CO_DATA) = cb;
    *reinterpret_cast<uint32_t*>(co + CO_COUNT) = 0;
    *reinterpret_cast<uint32_t*>(co + CO_EVENT_CALL_TYPE) = 0x200;
    *reinterpret_cast<uintptr_t*>(co + CO_PROC) = g_base + F_TOUCH_HANDLE_CONTROL_EVENT_PROC_VMA;
    *reinterpret_cast<uintptr_t*>(co + CO_CONTROL_PROC) = g_base + F_CONTROL_BUTTON_CONTROL_EVENT_PROC_VMA;
    *reinterpret_cast<void**>(co + CO_PARENT) = nullptr;

    *reinterpret_cast<uintptr_t*>(cb + CB_EXECUTE_PROC) = reinterpret_cast<uintptr_t>(&data_op_mix_gem_batch);
    *reinterpret_cast<uint32_t*>(cb + CB_DRAW_TYPE) = *reinterpret_cast<uint32_t*>(orig_data + CB_DRAW_TYPE);
    *reinterpret_cast<int64_t*>(cb + CB_DRAW_ID) = *reinterpret_cast<int64_t*>(orig_data + CB_DRAW_ID);
    *reinterpret_cast<int64_t*>(cb + CB_DRAW_SUB_ID) = *reinterpret_cast<int64_t*>(orig_data + CB_DRAW_SUB_ID);
    *reinterpret_cast<uintptr_t*>(cb + CB_DRAW_PROC) = g_base + F_UIMIX_BUTTON_DRAW_MIXING_GEM_VMA;
    *reinterpret_cast<uint8_t*>(cb + CB_STATE) = 0;
    *reinterpret_cast<uint8_t*>(cb + CB_ENABLED) = 1;

    // 槽指针写新按钮（绘制走固定槽）；覆盖原按钮 ExecuteProc（点击走控件树）
    g_craft_exec_hook.install(orig_data + CB_EXECUTE_PROC, reinterpret_cast<void*>(&data_op_mix_gem_batch));
    *slot = co;

    g_craft_orig = orig;
    g_craft_mmap = region;
    g_craft_mmap_len = len;
    g_craft_injected = true;
    MOVE_LOG("craft_btn_inject: ok, orig=%p new=%p", reinterpret_cast<void*>(orig),
             reinterpret_cast<void*>(co));
    return true;
}

// 还原宝石按钮槽指针 + 原 ExecuteProc，并释放 mmap。
void data_craft_btn_remove() {
    std::lock_guard<std::mutex> lock(g_craft_mtx);
    if (!g_craft_injected) return;
    if (g_uimix != nullptr && g_craft_orig != nullptr) {
        void** slot = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_uimix) + UIMIX_SLOT_GEM_BTN);
        g_craft_exec_hook.uninstall();
        *slot = g_craft_orig;
    }
    if (g_craft_mmap != nullptr) munmap(g_craft_mmap, g_craft_mmap_len);
    g_craft_mmap = nullptr;
    g_craft_mmap_len = 0;
    g_craft_orig = nullptr;
    g_craft_injected = false;
    MOVE_LOG("craft_btn_remove: restored");
}
