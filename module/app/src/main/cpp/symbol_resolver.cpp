#include "symbol_resolver.h"

#include <elf.h>
#include <cstring>

SymbolResolver g_resolver;

namespace {

// SVR4 SysV ELF hash（bionic 同款）
uint32_t elf_hash(const char* name) {
    uint32_t h = 0, g;
    while (*name != '\0') {
        h = (h << 4) + static_cast<uint8_t>(*name++);
        g = h & 0xf0000000u;
        if (g != 0) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

bool in_range(uintptr_t v, uintptr_t lo, uintptr_t hi) {
    return v >= lo && v < hi;
}

}  // namespace

void SymbolResolver::attach(uintptr_t base) {
    if (attached_) return;
    base_ = base;
    if (base == 0) return;

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(base);
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return;  // 非 64 位 ELF（本库现状）；32 位兼容留待需要时加 Elf32 分支
    }

    // load_bias：通用形式（本库首段 p_vaddr=0 故等于 base，写成通用形式防未来换库）
    uintptr_t bias = base;
    uintptr_t lo = ~0ull, hi = 0;
    const auto* ph = reinterpret_cast<const Elf64_Phdr*>(base + ehdr->e_phoff);
    bool have_load = false;
    bool have_dynamic = false;
    uintptr_t dyn_file_off = 0;  // PT_DYNAMIC 文件偏移（读 ELF 结构用 p_offset，兼容文件 buffer 与已加载内存）
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (ph[i].p_type == PT_LOAD) {
            if (!have_load) {
                bias = base - ph[i].p_vaddr;
                have_load = true;
            }
            uintptr_t seg_lo = bias + ph[i].p_vaddr;
            uintptr_t seg_hi = seg_lo + ph[i].p_memsz;
            if (seg_lo < lo) lo = seg_lo;
            if (seg_hi > hi) hi = seg_hi;
        } else if (ph[i].p_type == PT_DYNAMIC) {
            dyn_file_off = ph[i].p_offset;
            have_dynamic = true;
        }
    }
    if (!have_load || !have_dynamic || lo > hi) return;
    load_bias_ = bias;
    lo_ = lo;
    hi_ = hi;

    // dynamic 段：用文件偏移 p_offset 定位（ELF 结构地址 = base + p_offset；d_ptr 是 vaddr，
    // 定位具体段时用 load_bias + d_ptr——二者指向同一内存，loader 不重定位改写 d_ptr）
    const auto* dyn = reinterpret_cast<const Elf64_Dyn*>(base + dyn_file_off);
    bool have_sym = false, have_str = false, have_hash = false;
    for (; dyn->d_tag != DT_NULL; ++dyn) {
        switch (dyn->d_tag) {
            case DT_SYMTAB:
                symtab_ = reinterpret_cast<const uint8_t*>(bias + dyn->d_un.d_ptr);
                have_sym = true;
                break;
            case DT_STRTAB:
                strtab_ = reinterpret_cast<const char*>(bias + dyn->d_un.d_ptr);
                have_str = true;
                break;
            case DT_HASH:
                hash_bucket_ = reinterpret_cast<const uint32_t*>(bias + dyn->d_un.d_ptr);
                have_hash = true;
                break;
            case DT_SYMENT:
                sym_entsize_ = dyn->d_un.d_val;
                break;
            default:
                break;
        }
    }
    if (!have_sym || !have_str || !have_hash) {
        symtab_ = nullptr;
        strtab_ = nullptr;
        hash_bucket_ = nullptr;
        return;
    }
    // SysV hash 头：nbucket, nchain, bucket[], chain[]
    hash_nbucket_ = hash_bucket_[0];
    hash_nchain_ = hash_bucket_[1];
    hash_chain_ = hash_bucket_ + 2 + hash_nbucket_;
    if (hash_nbucket_ == 0 || hash_nchain_ == 0) {
        symtab_ = nullptr;
        strtab_ = nullptr;
        hash_bucket_ = nullptr;
        return;
    }
    attached_ = true;
}

uintptr_t SymbolResolver::resolveByName(const char* name) const {
    if (!attached_ || name == nullptr || name[0] == '\0') return 0;
    uint32_t h = elf_hash(name) % hash_nbucket_;
    for (uint32_t idx = hash_bucket_[2 + h]; idx != 0 && idx < hash_nchain_; idx = hash_chain_[idx]) {
        const auto* sym = reinterpret_cast<const Elf64_Sym*>(symtab_ + static_cast<uintptr_t>(idx) * sym_entsize_);
        if (ELF64_ST_TYPE(sym->st_info) == STT_TLS) continue;   // TLS 偏移不是 vaddr
        if (sym->st_shndx == SHN_UNDEF) continue;               // 外部导入，st_value 无意义
        const char* sn = strtab_ + sym->st_name;
        if (strcmp(sn, name) != 0) continue;
        uintptr_t v = load_bias_ + sym->st_value;
        if (in_range(v, lo_, hi_)) return v - base_;  // 返回相对基址偏移（防同名撞库范围校验）
    }
    return 0;
}

ResolvedSymbol SymbolResolver::resolve(const char* name, uintptr_t vma) const {
    ResolvedSymbol r;
    r.offset = vma;
    if (name == nullptr || name[0] == '\0') {
        r.source = SymbolSource::VMA;  // 无符号名（GOT 槽/表项），恒走回退
        r.has_name = false;
        ++fallback_vma_;
        return r;
    }
    r.has_name = true;
    uintptr_t dyn = resolveByName(name);
    if (dyn != 0) {
        r.offset = dyn;
        r.source = SymbolSource::ELF;
        ++elf_ok_;
    } else {
        r.source = SymbolSource::MISS;  // 有符号名但未命中/越界 → 回退 VMA
        ++missed_;
    }
    return r;
}
