#pragma once

// 游戏状态层：全局状态检测 + 写操作共享工具（无构建/导航依赖）。

bool game_in_world();
int tutorial_state();
void tutorial_cancel();
const char* tutorial_block_error();

// 写操作共享工具（原 game_data.cpp 匿名 namespace op 工具区）
std::string op_ok();
std::string op_err(const char* msg);
void* member_or_null(int role);
int inventory_count();
void* find_inventory_item(int category);
void* inventory_item_at(int bag, int slot);
