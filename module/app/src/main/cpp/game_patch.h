#pragma once

#include <cstddef>
#include <cstdint>

// 游戏内存 patch 基础设施（stack-limit-999，v0.5.18）：可逆改写 libgame.so 指令。
// 数量位段从 7bit(bit25-31) 扩到 10bit(bit22-31) 无法通过读写内存/调函数实现，
// 只能改写位段参数立即数。所有地址走 fn_resolve 符号解析，禁止裸地址。

struct PatchEntry {
    const char* func_macro;   // 函数符号宏名（symbol_registry.h 的 SYM 宏名，fn_resolve 用）
    uintptr_t func_vma;       // 函数 VMA（动态解析失败时回退）
    uint32_t func_offset;     // 函数内偏移
    uint32_t orig;            // 原始指令（关闭时还原）
    uint32_t replacement;     // 替换指令
};

// stack-limit-999 的 42 个 patch 点（位段扩展 31 + clamp 9 + 存档子物品检查 2）
extern const PatchEntry g_stack_limit_patches[];
extern const size_t g_stack_limit_patch_count;

bool patch_apply(const PatchEntry* entries, size_t n);
bool patch_revert(const PatchEntry* entries, size_t n);

bool data_op_migrate_stack(bool enabling);

bool set_stack_limit_enabled(bool enabled);
bool stack_limit_enabled();
