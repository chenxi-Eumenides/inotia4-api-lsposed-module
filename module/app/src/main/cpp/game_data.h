#pragma once

#include <string>

// 数据读取层：构造各 API 端点的 JSON 响应。
// 所有读取基于 game_access 解析的符号指针（base+VMA），
// 偏移定义见 game_symbols.h。

std::string data_player_json();
std::string data_party_json();
std::string data_inventory_json();
std::string data_map_json();
std::string data_units_json();
std::string data_gamestate_json();
std::string data_snapshot_json();
std::string data_skills_json();
std::string data_mercenaries_json();
std::string data_path_json(int tx, int ty);
int data_active_quest();
std::string data_init_report();

// ---- 写操作（v0.3.0，2026-08-05 逆向实现，签名见 control-capability.md §5）----
std::string data_op_set_money(int64_t money);
std::string data_op_add_money(int64_t delta);
std::string data_op_minus_money(int64_t delta);
std::string data_op_set_experience(int role, int64_t exp);
std::string data_op_add_experience(int role, int64_t delta);
std::string data_op_set_status_point(int role, int32_t points);
std::string data_op_set_auto_attack(int role, int32_t onoff);
std::string data_op_equip(int role, int bag, int slot);
std::string data_op_unequip(int role, int32_t equip_slot);
std::string data_op_switch_player(int32_t slot);
std::string data_op_teleport(int32_t map_id, int32_t x, int32_t y);
std::string data_op_remove_item(int32_t category);
std::string data_op_learn_action(int role, int32_t action_id, int32_t level);

// ---- 合法操作（v0.3.1，玩家游戏内可做的事）----
std::string data_op_move(int32_t x, int32_t y);
std::string data_op_use_item(int bag, int slot);
std::string data_op_discard_item(int bag, int slot);
std::string data_op_include_party(int mercenary_slot);
std::string data_op_exclude_party(int mercenary_slot);
std::string data_op_sell_item(int bag, int slot, int64_t price);

// ---- 事件流（/api/events，轮询差异检测，零 hook）----
std::string data_events_json();
