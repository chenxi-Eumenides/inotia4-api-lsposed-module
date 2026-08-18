#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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

class PatchSet {
public:
    PatchSet(const PatchEntry* entries, size_t count) : entries_(entries), count_(count) {}

    bool apply() {
        if (entries_ == nullptr || !patch_apply(entries_, count_)) return false;
        applied_ = true;
        return true;
    }

    bool revert() {
        if (entries_ == nullptr || !patch_revert(entries_, count_)) return false;
        applied_ = false;
        return true;
    }

    bool applied() const { return applied_; }

private:
    const PatchEntry* entries_ = nullptr;
    size_t count_ = 0;
    bool applied_ = false;
};

bool data_op_migrate_stack(bool enabling);

bool set_stack_limit_enabled(bool enabled);
bool stack_limit_enabled();

// ---- IAP 恢复（v0.5.18 hive 屏蔽恢复）+ 合成器批量宝石合成 + 自定义 UI 按钮 ----
std::string data_recover_after_hive_block();
void data_op_mix_gem_batch(void* ctrl);  // 批量合成（按钮 ExecuteProc 回调，x0=控件对象）
bool data_craft_btn_inject();            // 注入批量合成按钮（mmap 新建 ControlObject + 写宝石按钮槽）
void data_craft_btn_remove();            // 还原宝石按钮槽 + 释放 mmap
void data_craft_btn_set_enabled(bool enabled);  // 配置开关入口：true 懒注入（轮询槽非空），false 还原
