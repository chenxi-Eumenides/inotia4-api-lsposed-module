// game_ops_value.cpp —— 由 game_data.cpp 拆分生成（纯搬代码，零逻辑变更）

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
#include "game_ops_value.h"
#include "game_state.h"
#include "game_cache.h"

std::string data_op_set_money(int64_t money) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_set_money == nullptr) return op_err("symbol not resolved");
    fn_set_money(money);
    return op_ok();
}

std::string data_op_add_money(int64_t delta) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_add_money == nullptr) return op_err("symbol not resolved");
    int r = fn_add_money(delta);
    return r ? op_ok() : op_err("add money failed");
}

std::string data_op_add_item(int32_t category, int32_t count) {
    if (!game_in_world()) return op_err("not in game");
    if (count <= 0) return op_err("bad count");
    if (fn_create_item == nullptr || fn_inven_save_item == nullptr || fn_inven_find_save_slot == nullptr ||
        fn_get_bit == nullptr)
        return op_err("symbol not resolved");
    // ITEMSYSTEM_CreateItem 创建物品对象（MakeItem 带 search_tbl 校验会失败，CreateItem 无此限制）
    void* item = fn_create_item(category, 0, 0, 0);
    if (item == nullptr) return op_err("create item failed");
    // 数量位域 [item+0x10] bit25-31 语义：0=不可堆叠、100=装备、1~99=可堆叠（上限99）。
    // CreateItem 已设好标记值（装备=100/不可堆叠=0），只在可堆叠物品上覆盖为请求数量。
    uint32_t cf = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT);
    int base_count = fn_get_bit(static_cast<int>(cf), 31, 25);
    if (count > 1 && (base_count < 1 || base_count > 99))
        return op_err("item not stackable");
    if (count > 1 && base_count >= 1 && base_count <= 99) {
        if (count > 99) count = 99;
        cf &= ~(0x7F800000u);
        cf |= (static_cast<uint32_t>(count) << 25);
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT) = cf;
    }
    int save_slot = fn_inven_find_save_slot(item, 0);
    if (save_slot <= 0) return op_err("inventory full");
    if (!fn_inven_save_item(item, nullptr)) return op_err("save item failed");
    return op_ok();
}

std::string data_op_minus_money(int64_t delta) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_minus_money == nullptr) return op_err("symbol not resolved");
    int r = fn_minus_money(delta);
    return r ? op_ok() : op_err("insufficient money");
}

std::string data_op_set_experience(int role, int64_t exp) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_exp == nullptr) return op_err("symbol not resolved");
    fn_set_exp(ch, static_cast<int32_t>(exp));
    return op_ok();
}

std::string data_op_set_level(int role, int32_t level, bool force) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_level == nullptr) return op_err("symbol not resolved");
    // 默认限制游戏实际上限 105（EXP 表 105 级）；force=true 跳过校验（level 超出 s8 存储 127 会溢出为负，调用方自担）
    if (!force && (level < 1 || level > 105)) return op_err("level 1-105 (game max)");
    // CHAR_SetLevel 完整结算：写 [ch+0xe] + 重算 nextExp + InitializeFromLevel + 升级加能力点/技能点 + 回满血蓝
    int ok = fn_set_level(ch, level);
    return ok ? op_ok() : op_err("level down not allowed");
}

std::string data_op_add_experience(int role, int64_t delta) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_add_exp == nullptr) return op_err("symbol not resolved");
    int r = fn_add_exp(ch, static_cast<int32_t>(delta), 1);
    return r ? op_ok() : op_err("add exp failed");
}

std::string data_op_set_status_point(int role, int32_t points) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_status_point == nullptr) return op_err("symbol not resolved");
    fn_set_status_point(ch, points);
    return op_ok();
}

std::string data_op_add_stat(int role, int32_t attr) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_get_status_point == nullptr || fn_get_stat_main == nullptr || fn_set_stat_main == nullptr ||
        fn_set_status_point == nullptr)
        return op_err("symbol not resolved");
    if (attr < 0 || attr > 4) return op_err("bad attr");
    int32_t points = fn_get_status_point(ch);
    if (points <= 0) return op_err("no status point");
    int32_t cur = fn_get_stat_main(ch, attr);
    fn_set_stat_main(ch, attr, cur + 1);
    fn_set_status_point(ch, points - 1);
    return op_ok();
}

std::string data_op_set_auto_attack(int role, int32_t onoff) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_auto_attack == nullptr) return op_err("symbol not resolved");
    fn_set_auto_attack(ch, onoff ? 1 : 0);
    return op_ok();
}

std::string data_op_set_skill_usage(int role, int32_t onoff) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_set_skill_usage == nullptr) return op_err("symbol not resolved");
    fn_set_skill_usage(ch, onoff ? 1 : 0);
    return op_ok();
}

std::string data_op_learn_action(int role, int32_t action_id, int32_t level) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (fn_learn_action == nullptr) return op_err("symbol not resolved");
    fn_learn_action(ch, action_id, level);
    return op_ok();
}

std::string data_op_remove_item(int32_t category) {
    if (!game_in_world()) return op_err("not in game");
    if (fn_remove_item == nullptr || fn_get_bit == nullptr) return op_err("symbol not resolved");
    // 按类别删第一个匹配物品（INVEN_RemoveItem 按 item 指针删，需先按类别定位）
    for (int b = 0; b < 6; ++b) {
        for (int j = 0; j < 16; ++j) {
            void* item = inventory_item_at(b, j);
            if (item == nullptr) continue;
            uint16_t flags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item) + I_TYPE);
            if (fn_get_bit(flags, 15, 6) == category) {
                int r = fn_remove_item(item);
                return r ? op_ok() : op_err("item not found");
            }
        }
    }
    return op_err("item not found");
}

std::string data_op_set_hp(int role, int32_t hp) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (hp < 0) hp = 0;
    int32_t max_hp = fn_get_attr != nullptr ? fn_get_attr(ch, 0x1e) : 32767;
    if (hp > max_hp) hp = max_hp;
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_HP) = hp;
    return op_ok();
}

std::string data_op_set_mp(int role, int32_t mp) {
    if (!game_in_world()) return op_err("not in game");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (mp < 0) mp = 0;
    int32_t max_mp = fn_get_attr != nullptr ? fn_get_attr(ch, 0x1f) : 32767;
    if (mp > max_mp) mp = max_mp;
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ch) + C_MP) = mp;
    return op_ok();
}

std::string data_op_set_attr(int role, int attr_index, int32_t value) {
    if (!game_in_world()) return op_err("not in game");
    if (attr_index < 0 || attr_index > 4) return op_err("bad attr");
    if (fn_set_stat_base == nullptr) return op_err("symbol not resolved");
    void* ch = member_or_null(role);
    if (ch == nullptr) return op_err("role not found");
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    // CHAR_SetStatBase 直接写"基础属性"（[ch+0x250+i] s8），装备/分配/动态加成不受影响
    // 总属性 = 基础 + 分配(main) + 加成(bonus) + 动态(equip/skill/buff)
    fn_set_stat_base(ch, attr_index, value);
    return op_ok();
}

