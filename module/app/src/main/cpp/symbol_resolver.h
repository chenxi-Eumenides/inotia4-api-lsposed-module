#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// ELF 内存符号解析器（v0.5.13，backlog P1 VMA 治理）：
// 从已加载 libgame.so 的 .dynsym 符号表按名字动态定位偏移，替代硬编码 VMA（换版本漂移免疫）。
// 不用 dlopen/dlsym：Android linker namespace 隔离下 dlopen 会加载独立副本；dlsym 有同名歧义。
// 已登记 VMA 作保底回退（GOT 槽/无名表项无符号名，恒走回退）。

// 解析来源统计（g_symbol_report 用）
enum class SymbolSource : uint8_t {
    ELF = 0,   // 动态命中（.dynsym 找到且范围校验通过）
    VMA = 1,   // 回退：无符号名（GOT 槽/表项）或名字未命中
    MISS = 2,  // 有符号名但解析失败（未命中/越界），已回退 VMA
};

struct ResolvedSymbol {
    uintptr_t offset = 0;   // 相对基址偏移（运行时地址 = g_base + offset）
    SymbolSource source = SymbolSource::VMA;
    bool has_name = false;  // 注册表声明了符号名（可动态化）？
};

class SymbolResolver {
public:
    // 解析 ELF 头/程序头/dynamic 段，构建 hash 表定位。幂等：重复 attach 直接复用。
    void attach(uintptr_t base);

    // 按符号名查偏移（SysV hash 查找，SHN_UNDEF/STT_TLS 过滤 + PT_LOAD 范围校验）。
    // 未 attach 或未命中返回 0。
    uintptr_t resolveByName(const char* name) const;

    // 统一入口：声明了符号名 → 动态解析，未命中回退 vma 并标 MISS；
    // 无符号名（GOT 槽/表项）→ 直接 vma，标 VMA。
    ResolvedSymbol resolve(const char* name, uintptr_t vma) const;

    bool attached() const { return attached_; }
    int elfOk() const { return elf_ok_; }
    int fallbackVma() const { return fallback_vma_; }
    int missed() const { return missed_; }

private:
    uintptr_t base_ = 0;
    const uint8_t* symtab_ = nullptr;   // DT_SYMTAB（load_bias + d_ptr）
    const char* strtab_ = nullptr;      // DT_STRTAB
    const uint32_t* hash_bucket_ = nullptr;  // SysV hash bucket
    const uint32_t* hash_chain_ = nullptr;
    uint32_t hash_nbucket_ = 0;
    uint32_t hash_nchain_ = 0;
    uint32_t sym_entsize_ = 24;         // DT_SYMENT（Elf64=24）
    uintptr_t load_bias_ = 0;
    uintptr_t lo_ = 0, hi_ = 0;         // PT_LOAD 并集可读范围
    bool attached_ = false;

    mutable int elf_ok_ = 0, fallback_vma_ = 0, missed_ = 0;
};

// 全局实例（game_access.cpp 使用）
extern SymbolResolver g_resolver;
