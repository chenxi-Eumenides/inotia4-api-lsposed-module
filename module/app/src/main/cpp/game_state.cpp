// game_state.cpp —— 由 game_data.cpp 拆分生成（纯搬代码，零逻辑变更）

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
#include "game_state.h"
#include "game_cache.h"

bool game_in_world() {
    return g_state != nullptr && *reinterpret_cast<uint16_t*>(g_state) == 5;
}

int tutorial_state() {
    if (g_base == 0) return 0;
    uint8_t* obj = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_OBJ_GOT_VMA);
    if (obj == nullptr) return 0;
    return static_cast<int>(*reinterpret_cast<uint64_t*>(obj));
}

void tutorial_cancel() {
    if (g_base == 0) return;
    // 复现 F_TUTORIAL_GETSTATE_VMA(0xec340)：Tutorialgetstate 轮转 + 写回 obj170 + 关闭 3 个教学标志
    uint8_t* obj = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_OBJ_GOT_VMA);
    if (obj == nullptr) return;
    // v0.4.50：直接写 obj170=0（无教学态）而非依赖 Tutorialgetstate 返回值——轮转结果依赖
    // 教学槽位历史，进档后旧槽残留导致轮转返回 6，教学无法退出（真机实测 v0.4.49 复现）
    *reinterpret_cast<uint64_t*>(obj) = 0;
    uint8_t* bb8 = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_FLAG1_GOT_VMA);
    if (bb8 != nullptr) *bb8 = 0;
    uint8_t* f170 = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_FLAG2_GOT_VMA);
    if (f170 != nullptr) *f170 = 1;
    uint8_t* ee0 = *reinterpret_cast<uint8_t**>(g_base + G_TUTORIAL_FLAG3_GOT_VMA);
    if (ee0 != nullptr) *ee0 = 0;
}

const char* tutorial_block_error() {
    if (tutorial_state() == 6) tutorial_cancel();
    return nullptr;
}

std::string op_ok() {
    frame_cache_force_refresh();   // 写操作成功后同步刷新缓存（attach 立即读最新，v0.4.57）
    return "{\"ok\":true}";
}

std::string op_err(const char* msg) {
    return std::string("{\"ok\":false,\"error\":\"") + msg + "\"}";
}

void* member_or_null(int role) {
    return (fn_get_member != nullptr && role >= 0 && role < 3) ? fn_get_member(role) : nullptr;
}

void* find_inventory_item(int category) {
    if (g_inven == nullptr || fn_get_bit == nullptr) return nullptr;
    for (int b = 0; b < 6; ++b) {
        uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * 0x80;
        for (int j = 0; j < 16; ++j) {
            void* item = *reinterpret_cast<void**>(bag_slots + j * 8);
            if (item == nullptr) continue;
            uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
            if (fn_get_bit(flags, 15, 6) == category) return item;
        }
    }
    return nullptr;
}

int inventory_count() {
    if (g_inven == nullptr) return -1;
    int n = 0;
    for (int b = 0; b < 6; ++b) {
        uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + b * 0x80;
        for (int j = 0; j < 16; ++j) {
            if (*reinterpret_cast<void**>(bag_slots + j * 8) != nullptr) ++n;
        }
    }
    return n;
}

void* inventory_item_at(int bag, int slot) {
    if (g_inven == nullptr) return nullptr;
    if (bag < 0 || bag >= 6 || slot < 0 || slot >= 16) return nullptr;
    uint8_t* bag_slots = reinterpret_cast<uint8_t*>(g_inven) + bag * 0x80;
    return *reinterpret_cast<void**>(bag_slots + slot * 8);
}

