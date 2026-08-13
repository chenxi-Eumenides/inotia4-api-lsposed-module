#pragma once

#include <string>

// 写数值操作层：直接改内存/调用 setter 的 op（set/add money/exp/hp/mp 等）。

std::string data_op_set_money(int64_t money);
std::string data_op_add_money(int64_t delta);
std::string data_op_minus_money(int64_t delta);
std::string data_op_set_experience(int role, int64_t exp);
std::string data_op_set_level(int role, int32_t level, bool force);
std::string data_op_add_experience(int role, int64_t delta);
std::string data_op_set_status_point(int role, int32_t points);
std::string data_op_set_hp(int role, int32_t hp);
std::string data_op_set_mp(int role, int32_t mp);
std::string data_op_set_attr(int role, int attr_index, int32_t value);
std::string data_op_add_item(int32_t category, int32_t count);
std::string data_op_add_stat(int role, int32_t attr);
std::string data_op_remove_item(int32_t category);
std::string data_op_learn_action(int role, int32_t action_id, int32_t level);
std::string data_op_set_auto_attack(int role, int32_t onoff);
std::string data_op_set_skill_usage(int role, int32_t onoff);
