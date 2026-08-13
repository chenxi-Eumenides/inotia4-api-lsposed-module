#pragma once

#include <cstdint>
#include <string>

// 杂项直接构造端点 + 事件流：非缓存槽的数据端点。

// 快照结构：events 轮询 diff + build_snapshot_json 共用
struct Snapshot {
    int64_t money;
    int16_t x, y;
    int32_t hp[3], mp[3], level[3];
    int64_t exp[3];
    int inv_count;
};

std::string data_debug_ui_json();
int64_t data_frame_count();
bool data_story_active();
uintptr_t data_popup_top_vma();
std::string data_story_json();
std::string data_path_json(int tx, int ty);
std::string data_distance_json(int32_t tx, int32_t ty);
int data_active_quest();
std::string data_quest_list_json();
std::string data_quest_completed_json();
std::string data_init_report();
std::string data_events_json();
