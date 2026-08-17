#include "game_patch.h"

#include "game_access.h"
#include "game_inventory.h"
#include "game_state.h"
#include "game_symbols.h"
#include "game_ops_common.h"
#include "game_ptr_hook.h"

#include <android/log.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

#define PATCH_TAG "Inotia4Export"
#define MOVE_TAG "Inotia4Move"
#define MOVE_LOG(...) __android_log_print(ANDROID_LOG_INFO, MOVE_TAG, __VA_ARGS__)

namespace {

std::recursive_mutex g_patch_mtx;
std::atomic<bool> g_stack_enabled{false};
std::atomic<bool> g_migrate_done{false};
std::atomic<bool> g_migrate_thread_started{false};


uintptr_t patch_addr(const PatchEntry& e) {
    if (g_base == 0) return 0;
    return g_base + fn_resolve(e.func_macro, e.func_vma) + e.func_offset;
}

bool write_insn(uintptr_t addr, uint32_t value) {
    const uintptr_t page = addr & ~uintptr_t{0xFFF};
    const size_t plen = (addr + sizeof(uint32_t) - page + 0xFFF) & ~size_t{0xFFF};
    if (mprotect(reinterpret_cast<void*>(page), plen, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, PATCH_TAG, "patch: mprotect(0x%lx) failed errno=%d", static_cast<unsigned long>(page), errno);
        return false;
    }
    *reinterpret_cast<uint32_t*>(addr) = value;
    __builtin___clear_cache(reinterpret_cast<char*>(addr),
                            reinterpret_cast<char*>(addr + sizeof(uint32_t)));
    return true;
}

void write_back(const PatchEntry* entries, size_t upto) {
    for (size_t i = 0; i < upto; ++i) {
        uintptr_t addr = patch_addr(entries[i]);
        if (addr == 0) continue;
        write_insn(addr, entries[i].orig);
    }
}

void ensure_migrate_thread() {
    if (g_migrate_thread_started.exchange(true)) return;
    std::thread([]() {
        for (;;) {
            if (g_stack_enabled.load() && !g_migrate_done.load() && game_in_world()) {
                std::lock_guard<std::recursive_mutex> lock(g_patch_mtx);
                if (g_stack_enabled.load() && !g_migrate_done.load() && game_in_world()) {
                    // 只改内存数据，不调 fn_save()：刚进 world 时角色状态数据未就绪，
                    // 后台线程调 SAVE_Save 会 CHAR_GetStatusPoint 空指针崩溃（真机实测）。
                    // 迁移结果由玩家后续存档（游戏自动存档 / API save）自然固化；未固化时
                    // 下次进 world 会再次迁移（幂等），无副作用。
                    data_op_migrate_stack(true);
                    g_migrate_done.store(true);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }).detach();
}

}  // namespace

// 42 个 patch 点（研究文档 docs/features/stack-limit-999.md §4，反汇编逐字节确认）
#define P(macro, off, o, r) { #macro, macro, off, o, r }
const PatchEntry g_stack_limit_patches[] = {
    // ① 位段扩展 mov w2,#0x19(bitStart=25) → mov w2,#0x16(bitStart=22)
    P(F_GET_CUMULATE_COUNT_VMA, 0x78, 0x52800322, 0x528002C2),
    P(F_INVEN_MOVE_ITEM_VMA, 0x178, 0x52800322, 0x528002C2),
    P(F_INVEN_MOVE_ITEM_VMA, 0x194, 0x52800322, 0x528002C2),
    P(F_INVEN_MOVE_ITEM_VMA, 0x224, 0x52800322, 0x528002C2),
    P(F_INVEN_MOVE_ITEM_VMA, 0x23c, 0x52800322, 0x528002C2),
    P(F_CONSUME_ITEM_VMA, 0x58, 0x52800322, 0x528002C2),
    P(F_CONSUME_ITEM_VMA, 0x80, 0x52800322, 0x528002C2),
    P(F_CONSUME_ITEM_VMA, 0x98, 0x52800322, 0x528002C2),
    P(F_CREATE_ITEM_VMA, 0x26c, 0x52800322, 0x528002C2),
    P(F_ITEMSYSTEM_DIVIDE_VMA, 0x68, 0x52800322, 0x528002C2),
    P(F_ITEMSYSTEM_DIVIDE_VMA, 0x90, 0x52800322, 0x528002C2),
    P(F_ITEMSYSTEM_DIVIDE_VMA, 0xa4, 0x52800322, 0x528002C2),
    P(F_ITEMSYSTEM_DIVIDE_VMA, 0xc0, 0x52800322, 0x528002C2),
    P(F_INVEN_SAVE_ITEM_DIRECT_VMA, 0xe8, 0x52800322, 0x528002C2),
    P(F_INVEN_SAVE_ITEM_DIRECT_VMA, 0x110, 0x52800322, 0x528002C2),
    P(F_INVEN_SAVE_ITEM_DIRECT_VMA, 0x168, 0x52800322, 0x528002C2),
    P(F_INVEN_SAVE_ITEM_DATA_VMA, 0x88, 0x52800322, 0x528002C2),
    P(F_INVEN_CHECK_SAVE_IN_NOT_EMPTY_SLOT_VMA, 0x14c, 0x52800322, 0x528002C2),
    P(F_INVEN_REMOVE_ITEM_DATA_VMA, 0x18c, 0x52800322, 0x528002C2),
    P(F_INVEN_GET_CUMULATE_SAVE_SLOT_EX_VMA, 0x15c, 0x52800322, 0x528002C2),
    P(F_INVEN_GET_CUMULATE_SAVE_SLOT_EX_VMA, 0x174, 0x52800322, 0x528002C2),
    P(F_UISTORE_BUTTON_SELL_EXE_VMA, 0xc0, 0x52800322, 0x528002C2),
    P(F_UISTORE_BUTTON_SELL_EXE_VMA, 0xd8, 0x52800322, 0x528002C2),
    P(F_UISTORE_SELL_ITEM_VMA, 0xd0, 0x52800322, 0x528002C2),
    P(F_GAME_START_NEW_GAME_VMA, 0xac, 0x52800322, 0x528002C2),
    P(F_MAKE_ITEM_VMA, 0x370, 0x52800322, 0x528002C2),
    P(F_ITEMSYSTEM_PROCESS_UNPACK_VMA, 0x354, 0x52800322, 0x528002C2),
    P(F_MAPITEMSYSTEM_CREATE_ITEM_VMA, 0xb4, 0x52800322, 0x528002C2),
    P(F_NETWORKSTORE_ADD_ITEM_VMA, 0x110, 0x52800322, 0x528002C2),
    P(F_SAVE_REVISE_CHARACTER_LOCATION_VMA, 0x248, 0x52800322, 0x528002C2),
    P(F_UIMIX_START_MIX_VMA, 0x180, 0x52800322, 0x528002C2),
    // ② 99→999 clamp（cmp/mov #0x63→#0x3E7，#0x62→#0x3E6）
    P(F_INVEN_MOVE_ITEM_VMA, 0x150, 0x71018c1f, 0x710f9c1f),
    P(F_INVEN_MOVE_ITEM_VMA, 0x158, 0x52800c77, 0x52807cf7),
    P(F_INVEN_SAVE_ITEM_DIRECT_VMA, 0xdc, 0x71018c7f, 0x710f9c7f),
    P(F_INVEN_SAVE_ITEM_DIRECT_VMA, 0xf0, 0x71018e9f, 0x710f9e9f),
    P(F_INVEN_SAVE_ITEM_DATA_VMA, 0x7c, 0x71018a9f, 0x710f9a9f),
    P(F_INVEN_SAVE_ITEM_DATA_VMA, 0x8c, 0x52800c63, 0x52807ce3),
    P(F_INVEN_FIND_SAVE_SLOT_VMA, 0x238, 0x71018c1f, 0x710f9c1f),
    P(F_INVEN_GET_CUMULATE_SAVE_SLOT_EX_VMA, 0x164, 0x7101881f, 0x710f981f),
    P(F_INVEN_GET_CUMULATE_SAVE_SLOT_EX_VMA, 0x17c, 0x52800c62, 0x52807ce2),
    // ③ 存档子物品检查位段缩窄 mov w1,#0x18(bitEnd=24) → mov w1,#0x15(bitEnd=21)
    P(F_SAVE_SAVE_INVENTORY_VMA, 0x78, 0x52800301, 0x528002A1),
    P(F_SAVE_LOAD_INVENTORY_VMA, 0xa8, 0x52800301, 0x528002A1),
};
#undef P
const size_t g_stack_limit_patch_count = sizeof(g_stack_limit_patches) / sizeof(g_stack_limit_patches[0]);

bool patch_apply(const PatchEntry* entries, size_t n) {
    if (entries == nullptr) return false;
    std::lock_guard<std::recursive_mutex> lock(g_patch_mtx);
    if (g_base == 0) return false;
    size_t done = 0;
    for (size_t i = 0; i < n; ++i) {
        const PatchEntry& e = entries[i];
        uintptr_t addr = patch_addr(e);
        if (addr == 0) { write_back(entries, done); return false; }
        uint32_t cur = *reinterpret_cast<uint32_t*>(addr);
        if (cur == e.replacement) { ++done; continue; }
        if (cur != e.orig) {
            __android_log_print(ANDROID_LOG_ERROR, PATCH_TAG, "patch_apply mismatch %s+0x%x got 0x%08x want 0x%08x",
                                e.func_macro, e.func_offset, cur, e.orig);
            write_back(entries, done);
            return false;
        }
        if (!write_insn(addr, e.replacement)) { write_back(entries, done); return false; }
        __android_log_print(ANDROID_LOG_INFO, PATCH_TAG, "patch_apply %s+0x%x 0x%08x -> 0x%08x",
                            e.func_macro, e.func_offset, e.orig, e.replacement);
        ++done;
    }
    return true;
}

bool patch_revert(const PatchEntry* entries, size_t n) {
    if (entries == nullptr) return false;
    std::lock_guard<std::recursive_mutex> lock(g_patch_mtx);
    if (g_base == 0) return false;
    size_t done = 0;
    for (size_t i = 0; i < n; ++i) {
        const PatchEntry& e = entries[i];
        uintptr_t addr = patch_addr(e);
        if (addr == 0) return false;
        uint32_t cur = *reinterpret_cast<uint32_t*>(addr);
        if (cur == e.orig) { ++done; continue; }
        if (cur != e.replacement) {
            __android_log_print(ANDROID_LOG_ERROR, PATCH_TAG, "patch_revert mismatch %s+0x%x got 0x%08x want 0x%08x",
                                e.func_macro, e.func_offset, cur, e.replacement);
            return false;
        }
        if (!write_insn(addr, e.orig)) return false;
        __android_log_print(ANDROID_LOG_INFO, PATCH_TAG, "patch_revert %s+0x%x 0x%08x -> 0x%08x",
                            e.func_macro, e.func_offset, e.replacement, e.orig);
        ++done;
    }
    return true;
}

bool data_op_migrate_stack(bool enabling) {
    std::lock_guard<std::recursive_mutex> lock(g_patch_mtx);
    if (g_base == 0 || g_inven == nullptr) return false;
    struct Ctx { bool enabling; int migrated; int truncated; } ctx{enabling, 0, 0};
    for_each_bag_slot([](void* item, int b, int j, void* c) -> bool {
        Ctx* p = static_cast<Ctx*>(c);
        if (item_is_equip(item)) return false;
        uint32_t* cf = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + I_COUNT);
        uint32_t v = *cf;
        if (p->enabling) {
            uint32_t n_old = (v >> 25) & 0x7F;
            if (n_old == 0) return false;
            if (((v >> 22) & 0x7) != 0) return false;
            *cf = (v & ~0x3FF00000u) | (n_old << 22);
            ++p->migrated;
        } else {
            uint32_t x = (v >> 22) & 0x3FF;
            if (x == 0) return false;
            if (x <= 127) {
                *cf = (v & ~0x3FF00000u) | (x << 25);
            } else {
                *cf = (v & ~0x3FF00000u) | (127u << 25);
                __android_log_print(ANDROID_LOG_WARN, PATCH_TAG, "migrate truncate %u -> 127 (bag %d slot %d)", x, b, j);
                ++p->truncated;
            }
            ++p->migrated;
        }
        return false;
    }, &ctx);
    __android_log_print(ANDROID_LOG_INFO, PATCH_TAG, "migrate %s migrated=%d truncated=%d",
                        enabling ? "enable" : "disable", ctx.migrated, ctx.truncated);
    return true;
}

bool set_stack_limit_enabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(g_patch_mtx);
    if (g_base == 0) return false;
    if (enabled == g_stack_enabled.load()) return true;
    if (enabled) {
        if (!patch_apply(g_stack_limit_patches, g_stack_limit_patch_count)) return false;
        g_stack_enabled.store(true);
        ensure_migrate_thread();
        return true;
    }
    if (g_migrate_done.load()) {
        if (game_in_world()) {
            data_op_migrate_stack(false);
            g_migrate_done.store(false);
        } else {
            __android_log_print(ANDROID_LOG_WARN, PATCH_TAG, "disable while not in world: new-format data not reverted");
        }
    }
    bool ok = patch_revert(g_stack_limit_patches, g_stack_limit_patch_count);
    g_stack_enabled.store(false);
    return ok;
}

bool stack_limit_enabled() {
    return g_stack_enabled.load();
}

std::string data_recover_after_hive_block() {
    if (g_base == 0) return op_err("base not ready");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    if (fn_networkstore_set_state == nullptr) return op_err("symbol not resolved");
    uint32_t** daily_trigger = reinterpret_cast<uint32_t**>(g_base + G_DAILY_TRIGGER_GOT_VMA);
    if (*daily_trigger != nullptr) **daily_trigger = 1;
    uint8_t** hud_gate = reinterpret_cast<uint8_t**>(g_base + G_HUD_GATE_GOT_VMA);
    if (*hud_gate != nullptr) **hud_gate = 1;
    fn_networkstore_set_state(0);
    fn_ui_set_popup_process_info(4, 0);
    return op_ok();
}


// ---- move-merge（v0.6.8）：游戏内拖拽移动物品触发同类合并 ----
// 原逻辑（UIEquip_InvenItemControlEventProc event==4）：清空目标槽 → INVEN_MoveItem(源→目标槽)
//（目标槽已空，走空槽放置）→ 原目标物品写回源槽 = 纯交换，同类物品也被拆开。
// 本 wrapper 拦截：源/目标同类且可堆叠时，不清空目标槽直接调 INVEN_MoveItem 走合并分支
//（数量并入目标槽、超出上限部分留在源槽），其余场景回退原处理器。
PtrHook g_move_merge_hook;

uint64_t ui_equip_inven_item_proc_wrapper(void* control, uint64_t event, void* x2, void* param) {
    if (event == 4 && param != nullptr && fn_control_object_get_data != nullptr &&
        fn_ui_equip_is_apply_stuff != nullptr && fn_ui_equip_get_item_slot_index != nullptr &&
        fn_get_cumulate_count != nullptr && fn_get_bit != nullptr && fn_inven_move_item != nullptr) {
        void* ctrl_src = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(param) + 8);
        if (ctrl_src != nullptr) {
            void* src_data = fn_control_object_get_data(ctrl_src);
            void* dst_data = fn_control_object_get_data(control);
            if (src_data != nullptr && dst_data != nullptr) {
                void* item_a = *reinterpret_cast<void**>(src_data);
                void* item_b = *reinterpret_cast<void**>(dst_data);
                if (item_a != nullptr && item_b != nullptr &&
                    !fn_ui_equip_is_apply_stuff(item_b, item_a) && !item_is_equip(item_a)) {
                    uint16_t fa = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item_a) + I_TYPE);
                    uint16_t fb = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(item_b) + I_TYPE);
                    if (fn_get_bit(fa, 15, 6) == fn_get_bit(fb, 15, 6)) {
                        int count = fn_get_cumulate_count(item_a);
                        int bag = *reinterpret_cast<uint8_t*>(g_base + G_UIEQUIP_CUR_BAG_VMA);
                        int slot = fn_ui_equip_get_item_slot_index(control);
                        int r = fn_inven_move_item(item_a, count, bag, slot);
                        if (!r) return 0;
                        if (fn_ui_equip_refresh_item_area != nullptr) fn_ui_equip_refresh_item_area();
                        void* panel_ctrl = *reinterpret_cast<void**>(g_base + G_UIEQUIP_PANEL_CTRL_VMA);
                        if (fn_touch_handle_set_cursor != nullptr && panel_ctrl != nullptr)
                            fn_touch_handle_set_cursor(panel_ctrl, nullptr);
                        MOVE_LOG("move_merge: merged bag=%d slot=%d count=%d", bag, slot, count);
                        // 合并完成后仍需执行原处理器（event==4 分支负责绘制/布局恢复），
                        // 直接 return 会跳过绘制导致格子错位重叠
                        if (fn_ui_equip_inven_item_control_event_proc == nullptr) return 0;
                        return fn_ui_equip_inven_item_control_event_proc(control, event, x2, param);
                    }
                }
            }
        }
    }
    if (fn_ui_equip_inven_item_control_event_proc == nullptr) return 0;
    return fn_ui_equip_inven_item_control_event_proc(control, event, x2, param);
}

bool set_move_merge_enabled(bool enabled) {
    if (g_base == 0) return false;
    void** slot = reinterpret_cast<void**>(g_base + G_UIEQUIP_INVEN_ITEM_PROC_GOT_VMA);
    if (slot == nullptr) return false;
    // GOT 段重定位后 linker 可能将页设为只读（SEGV_ACCERR 实测），写入前临时放开写权限
    const uintptr_t page = reinterpret_cast<uintptr_t>(slot) & ~uintptr_t{0xFFF};
    if (mprotect(reinterpret_cast<void*>(page), 0x1000, PROT_READ | PROT_WRITE) != 0) return false;
    if (enabled) {
        if (g_move_merge_hook.orig != nullptr) return true;
        bool ok = g_move_merge_hook.install(slot, reinterpret_cast<void*>(&ui_equip_inven_item_proc_wrapper));
        if (ok) MOVE_LOG("move_merge: enabled, proc_got -> %p", reinterpret_cast<void*>(&ui_equip_inven_item_proc_wrapper));
        return ok;
    }
    if (g_move_merge_hook.orig == nullptr) return true;
    g_move_merge_hook.uninstall();
    MOVE_LOG("move_merge: disabled");
    return true;
}

bool move_merge_enabled() {
    return g_move_merge_hook.orig != nullptr;
}
