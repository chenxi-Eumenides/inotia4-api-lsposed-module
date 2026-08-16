#pragma once

#include <cstdint>
#include <string>

// 世界域（parse 域）：剧情状态 + 自研 BFS 路径/距离/调试路径。

bool data_story_active();
std::string data_story_json();
std::string data_path_json(int tx, int ty);
std::string data_distance_json(int32_t tx, int32_t ty);
std::string data_debug_path_json(int32_t tx, int32_t ty);

// 世界操作：移动/传送/交互。
std::string data_op_teleport(int32_t map_id, int32_t x, int32_t y);
std::string data_op_move(int32_t x, int32_t y);
std::string data_op_walk(int32_t direction);
std::string data_op_walk_stop();
std::string data_op_interact();
