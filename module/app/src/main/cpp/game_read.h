#pragma once

#include <cstdint>
#include <string>

// 数据读取层：build_*_json 构造器 + 直接构造的 data_*_json（非缓存端点）。
// 依赖 game_nav（BFS）/game_state（world 检测）/game_json（转义）。

std::string member_json(void* ch);
void append_item_attrs(std::string& s, void* item);
void append_position(std::string& s, void* member);
void* lead_member();
bool item_is_equip(void* item);

std::string build_player_json();
std::string build_party_json();
std::string build_inventory_json();
std::string build_map_json();
std::string build_tiles_json();
std::string build_units_json();
std::string build_enemies_json();
std::string build_interactives_json();
std::string build_drops_json();
std::string build_gamestate_json();
std::string build_skills_json();
std::string build_mercenaries_json();
std::string build_snapshot_json();
void* find_char_by_merc_slot(int slot);
