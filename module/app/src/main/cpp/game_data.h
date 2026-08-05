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
std::string data_ui_json();
int data_active_quest();
std::string data_init_report();
