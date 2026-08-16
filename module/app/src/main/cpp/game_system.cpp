// game_system.cpp —— 系统聚合域：帧计数 + 初始化报告 + 事件流（唯一允许 include 其他域头的聚合域）（parse 域）
// 由 game_misc.cpp 拆分生成（纯搬代码，零逻辑变更）。

#include "game_system.h"

#include "game_access.h"
#include "game_state.h"

#include <mutex>

// 事件流基线（审计 H4 修复：diff 全程锁保护）
std::mutex g_events_mtx;
bool g_events_has_last = false;
Snapshot g_events_last;

int64_t data_frame_count() {
    if (g_base == 0) return -1;
    // 帧计数 GOT 槽（G_FRAME_COUNT_VMA）：先解引用取 u64 指针，再读计数
    uintptr_t* slot = reinterpret_cast<uintptr_t*>(g_base + G_FRAME_COUNT_VMA);
    uint64_t* cnt = reinterpret_cast<uint64_t*>(*slot);
    return cnt != nullptr ? static_cast<int64_t>(*cnt) : -1;
}

std::string data_init_report() {
    std::string s = "{";
    bool first = true;
    for (const auto& item : g_symbol_report) {
        if (!first) s += ",";
        s += "\"" + std::string(item.first) + "\":" + (item.second ? "true" : "false");
        first = false;
    }
    if (!g_dl_error.empty()) {
        s += ",\"error\":\"" + g_dl_error + "\"";
    }
    if (!g_lib_path.empty()) {
        s += ",\"path\":\"" + g_lib_path + "\"";
    }
    s += "}";
    return s;
}

namespace {

Snapshot take_snapshot() {
    Snapshot s{};
    s.money = (fn_get_money != nullptr) ? fn_get_money() : -1;
    s.x = s.y = -1;
    s.inv_count = inventory_count();
    for (int i = 0; i < 3; ++i) {
        void* ch = (fn_get_member != nullptr) ? fn_get_member(i) : nullptr;
        s.hp[i] = s.mp[i] = s.level[i] = -1;
        s.exp[i] = -1;
        if (ch != nullptr) {
            uint8_t* b = reinterpret_cast<uint8_t*>(ch);
            s.hp[i] = *reinterpret_cast<int32_t*>(b + C_HP);
            s.mp[i] = *reinterpret_cast<int32_t*>(b + C_MP);
            s.level[i] = reinterpret_cast<int8_t*>(ch)[C_LEVEL];
            s.exp[i] = (fn_get_exp != nullptr) ? fn_get_exp(ch) : -1;
        }
    }
    void* lead = (fn_get_member != nullptr) ? fn_get_member(0) : nullptr;
    if (lead != nullptr) {
        uint8_t* b = reinterpret_cast<uint8_t*>(lead);
        s.x = *reinterpret_cast<int16_t*>(b + C_POS_X);
        s.y = *reinterpret_cast<int16_t*>(b + C_POS_Y);
    }
    return s;
}

void emit(std::string& out, bool& first, const char* type, int role, int64_t a, int64_t b) {
    if (!first) out += ",";
    first = false;
    out += "{\"type\":\"" + std::string(type) + "\"";
    if (role >= 0) out += ",\"role\":" + std::to_string(role);
    out += ",\"old\":" + std::to_string(a);
    out += ",\"new\":" + std::to_string(b);
    out += "}";
}

}  // namespace

std::string data_events_json() {
    Snapshot cur = take_snapshot();
    std::lock_guard<std::mutex> lock(g_events_mtx);
    static Snapshot last;
    static bool has_last = false;
    std::string s = "{\"events\":[";
    bool first = true;
    if (!has_last) {
        has_last = true;
        last = cur;
        s += "]}";
        return s;
    }
    if (cur.money >= 0 && last.money >= 0 && cur.money != last.money)
        emit(s, first, "money", -1, last.money, cur.money);
    if (cur.inv_count >= 0 && last.inv_count >= 0 && cur.inv_count != last.inv_count)
        emit(s, first, "inventory", -1, last.inv_count, cur.inv_count);
    if (cur.x >= 0 && last.x >= 0 && (cur.x != last.x || cur.y != last.y))
        emit(s, first, "move", -1, 0, 0);
    for (int i = 0; i < 3; ++i) {
        if (cur.hp[i] >= 0 && last.hp[i] >= 0 && cur.hp[i] != last.hp[i])
            emit(s, first, "hp", i, last.hp[i], cur.hp[i]);
        if (cur.mp[i] >= 0 && last.mp[i] >= 0 && cur.mp[i] != last.mp[i])
            emit(s, first, "mp", i, last.mp[i], cur.mp[i]);
        if (cur.level[i] >= 0 && last.level[i] >= 0 && cur.level[i] != last.level[i])
            emit(s, first, "level_up", i, last.level[i], cur.level[i]);
        if (cur.exp[i] >= 0 && last.exp[i] >= 0 && cur.exp[i] != last.exp[i])
            emit(s, first, "exp", i, last.exp[i], cur.exp[i]);
    }
    last = cur;
    s += "]}";
    return s;
}
