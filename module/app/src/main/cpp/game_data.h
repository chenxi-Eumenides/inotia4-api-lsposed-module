#pragma once

#include <string>

// 数据读取层：构造各 API 端点的 JSON 响应。
// 所有读取基于 game_access 解析的符号指针（base+VMA），
// 偏移定义见 game_symbols.h。

// 惰性/预取混合缓存（v0.4.59）：interval>0 槽由预取线程每 n 帧主动构造（请求命中缓存 µs 级），
// interval=0 槽惰性（请求驱动，过期等帧边界构造）。表驱动改一行即切换。
void frame_cache_start();       // bridge_init 成功后启动预取线程（存在 interval>0 槽时）
void frame_cache_force_refresh();  // 写操作成功后同步刷新（op_ok 内部调用）
bool frame_cache_ready();       // 是否已成功构造过至少一个槽

std::string data_player_json();
std::string data_party_json();
std::string data_inventory_json();
std::string data_map_json();
std::string data_units_json();
std::string data_enemies_json();
std::string data_interactives_json();
std::string build_tiles_json();  // v0.4.62 P0：完整瓦片矩阵（64×64 base64）
std::string data_gamestate_json();
int64_t data_frame_count();
bool data_story_active();
const char* data_ui_screen();
uintptr_t data_popup_top_vma();
const char* data_top_panel_name();
std::string data_story_json();
std::string data_debug_ui_json();
std::string data_snapshot_json();
std::string data_skills_json();
std::string data_mercenaries_json();
std::string data_drops_json();
std::string data_path_json(int tx, int ty);
std::string data_distance_json(int32_t tx, int32_t ty);  // v0.4.29 自研 BFS 距离（玩家→目标）
std::string data_debug_path_json(int32_t tx, int32_t ty);  // debug 端点：BFS 完整路线 + 阻挡信息
int data_active_quest();
std::string data_quest_list_json();
std::string data_quest_completed_json();
std::string data_quest_active_json();
std::string data_current_save_slot_json();
std::string data_init_report();

// ---- 写操作（v0.3.0，2026-08-05 逆向实现，签名见 control-capability.md §5）----
std::string data_op_set_money(int64_t money);
std::string data_op_add_money(int64_t delta);
std::string data_op_minus_money(int64_t delta);
std::string data_op_set_experience(int role, int64_t exp);
std::string data_op_set_level(int role, int32_t level, bool force);
std::string data_op_add_experience(int role, int64_t delta);
std::string data_op_set_status_point(int role, int32_t points);
std::string data_op_set_hp(int role, int32_t hp);
std::string data_op_set_mp(int role, int32_t mp);
std::string data_op_set_attr(int role, int attr_index, int32_t value);
std::string data_op_add_item(int32_t category, int32_t count);
std::string data_op_add_stat(int role, int32_t attr);
std::string data_op_jewel(int role, int bag, int slot, int equip_slot);
std::string data_op_enchant(int role, int bag, int slot, int equip_slot);
std::string data_op_set_auto_attack(int role, int32_t onoff);
std::string data_op_set_skill_usage(int role, int32_t onoff);
std::string data_op_equip(int role, int bag, int slot);
std::string data_op_unequip(int role, int32_t equip_slot);
std::string data_op_switch_player(int32_t slot);
std::string data_op_party_swap(int32_t a, int32_t b);
std::string data_op_teleport(int32_t map_id, int32_t x, int32_t y);
std::string data_op_remove_item(int32_t category);
std::string data_shop_items_json();
std::string data_op_shop_buy(int32_t slot);
std::string data_op_learn_action(int role, int32_t action_id, int32_t level);

// ---- 合法操作（v0.3.1，玩家游戏内可做的事）----
std::string data_op_move(int32_t x, int32_t y);
std::string data_op_walk(int32_t direction);
std::string data_op_walk_stop();
std::string data_op_interact();
std::string data_op_use_item(int bag, int slot);
std::string data_op_dice_accept();
std::string data_op_dice_reject();
std::string data_op_discard_item(int bag, int slot);
std::string data_op_include_party(int mercenary_slot);
std::string data_op_exclude_party(int mercenary_slot);
std::string data_op_discharge(int mercenary_slot);
std::string data_op_sell_item(int bag, int slot);
std::string data_op_move_item(int bag, int slot, int count, int to_bag, int to_slot);
std::string data_op_stat_reset(int role);
std::string data_op_skill_reset(int role);
std::string data_op_cast(int role, int32_t action_id);
std::string data_op_quest_quit(int32_t quest_id);
std::string data_op_save();
std::string data_op_main_menu();
std::string data_op_enter_slot(int32_t slot);
std::string data_op_create_slot(int32_t slot, int32_t class_idx);
std::string data_op_panel_close();
std::string data_op_panel_open(const std::string& panel);
std::string data_recover_after_hive_block();
std::string data_save_slots_json();
std::string data_op_npc_interact();
std::string data_npc_dialog_options_json();
std::string data_op_npc_dialog_next();
std::string data_op_npc_dialog_select(int index);
std::string data_dialog_content_json();
std::string data_op_dialog_select(const std::string& action, int index);
std::string data_op_withdraw(int mercenary_slot, int32_t equip_slot);std::string data_op_dialog_ok();
std::string data_op_dialog_cancel();
std::string data_op_attack(int role, int target_slot);
std::string data_op_stop_combat(int role);

// ---- 合成器批量宝石合成 + 自定义 UI 按钮（v0.5.18）----
bool data_craft_btn_inject();   // 注入批量合成按钮（mmap 新建 ControlObject + 写宝石按钮槽）
void data_craft_btn_remove();   // 还原宝石按钮槽 + 释放 mmap
void data_craft_btn_set_enabled(bool enabled);  // 配置开关入口：true 懒注入（轮询槽非空），false 还原

// ---- 事件流（/api/events，轮询差异检测，零 hook）----
std::string data_events_json();
