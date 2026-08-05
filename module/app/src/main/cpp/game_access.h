#pragma once

#include "game_symbols.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// 符号解析层：从 /proc/self/maps 定位 libgame.so 基址，计算全部符号运行时地址。
// 不用 dlopen/dlsym：Android linker namespace 隔离下 dlopen 会加载独立副本，读不到游戏数据。

extern uintptr_t g_base;
extern void* g_money;
extern void* g_map_id;
extern void* g_party;
extern void* g_active_quest;
extern void* g_inven;
extern void* g_main_merc_slot;
extern void* g_state;
extern void* g_gamestate;
extern void* g_initstate;

extern GetMoneyFn fn_get_money;
extern GetMemberFn fn_get_member;
extern GetPartySizeFn fn_get_party_size;
extern GetAttrFn fn_get_attr;
extern GetEquipFn fn_get_equip;
extern GetExpFn fn_get_exp;
extern GetExpFn fn_get_next_exp;
extern GetRarityFn fn_get_rarity;
extern GetBagSizeFn fn_get_bag_size;
extern GetBitFn fn_get_bit;

extern std::vector<std::pair<const char*, bool>> g_symbol_report;
extern std::string g_dl_error;
extern std::string g_lib_path;

bool bridge_init();
