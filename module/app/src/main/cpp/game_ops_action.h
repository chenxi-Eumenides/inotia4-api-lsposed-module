#pragma once

#include <string>

std::string story_next();
std::string story_skip();

// 执行操作层：调用游戏函数执行动作的 op（move/walk/equip/use_item/对话/商店等），
// 含 walk/nav 逐帧任务回调（移动操作的具体执行逻辑）。

std::string data_op_move(int32_t x, int32_t y);
std::string data_op_walk(int32_t direction);
std::string data_op_walk_stop();
std::string data_op_interact();
std::string data_op_dialog_ok();
std::string data_op_dialog_cancel();
std::string data_op_use_item(int bag, int slot);
std::string data_op_dice_accept();
std::string data_op_dice_reject();
std::string data_op_discard_item(int bag, int slot);
std::string data_op_sell_item(int bag, int slot);
std::string data_op_move_item(int bag, int slot, int count, int to_bag, int to_slot);
std::string data_op_include_party(int mercenary_slot);
std::string data_op_exclude_party(int mercenary_slot);
std::string data_op_discharge(int mercenary_slot);
std::string data_op_withdraw(int mercenary_slot, int32_t equip_slot);
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
std::string data_op_shop_buy(int32_t slot);
std::string data_shop_items_json();
std::string data_op_jewel(int role, int bag, int slot, int equip_slot);
std::string data_op_equip(int role, int bag, int slot);
std::string data_op_unequip(int role, int32_t equip_slot);
std::string data_op_switch_player(int32_t slot);
std::string data_op_teleport(int32_t map_id, int32_t x, int32_t y);
std::string data_op_party_swap(int32_t a, int32_t b);
std::string data_op_attack(int role, int target_slot);
std::string data_op_stop_combat(int role);
