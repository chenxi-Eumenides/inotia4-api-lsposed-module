#include "game_ui_custom.h"

#include "game_access.h"
#include "game_ops_common.h"
#include "game_patch.h"
#include "game_ptr_hook.h"
#include "game_state.h"
#include "game_symbols.h"
#include "game_ui.h"

#include <android/log.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#define CUSTOM_TAG "Inotia4UICustom"
#define CUSTOM_LOG(...) __android_log_print(ANDROID_LOG_INFO, CUSTOM_TAG, __VA_ARGS__)

#define POPUP_STATE_SIZE 0x40
#define CB_TEXT_SIZE 0x20

// 面板布局（逻辑坐标 0-960×0-640 空间，相对全屏居中 root；触摸坐标同空间）
#define ROOT_W 0x3c0
#define ROOT_H 0x280
#define BTN_W 0x120
#define BTN_H 0x50
#define DESC_W 0x120
#define DESC_H 0x30
#define CELL_COL0_X 0x50
#define CELL_COL1_X 0x1b0
#define CELL_ROW0_Y 0x60
#define CELL_ROW1_Y 0x120
#define CELL_ROW2_Y 0x1e0
#define BTN_IMG_OFF_X (-0xc)
#define BTN_IMG_OFF_Y (-0x9)

namespace {

std::mutex g_custom_mtx;
PtrHook g_store_back_hook;
bool g_btn_injected = false;
std::atomic<bool> g_btn_want{false};
std::atomic<bool> g_thread_started{false};

int font_id();

bool g_panel_active = false;
uint8_t* g_state_entry = nullptr;
uint8_t g_state_backup[POPUP_STATE_SIZE] = {0};
int g_state_id = -1;
void* g_root = nullptr;

struct CustomCell {
    void* btn;
    void* desc;
};
CustomCell g_cells[3];
static void (*g_cell_procs[3])(void*) = {nullptr, nullptr, nullptr};

// ControlObject_SetRect 有 AArch64 x8 输出参数（GetRelativeRect 写 x8）→ C++ 无法传 x8，
// 直接写 [ctrl+0x18..0x30] 内存（docs/system/ui.md §6 关键坑 3）。
void set_ctrl_rect(void* ctrl, int64_t x, int64_t y, int64_t w, int64_t h) {
    uint8_t* c = reinterpret_cast<uint8_t*>(ctrl);
    *reinterpret_cast<int64_t*>(c + CO_RECT_X) = x;
    *reinterpret_cast<int64_t*>(c + CO_RECT_Y) = y;
    *reinterpret_cast<int64_t*>(c + CO_RECT_W) = w;
    *reinterpret_cast<int64_t*>(c + CO_RECT_H) = h;
}

// ControlObject_GetAbsoluteRect 同样用 x8 输出（GetRelativeRect stp [x8]）→ C++ 无法传 x8
// （真机 SIGSEGV 实测）。自实现：累加父链 rect（控件树固定两层 root+子，root 无父）。
// 输出 [abs_x, abs_y]（w/h 用贴图固定尺寸，GetAbsoluteRect 也只输出 x/y 两个 i64）。
void ctrl_abs_point(void* ctrl, int64_t out[2]) {
    uint8_t* c = reinterpret_cast<uint8_t*>(ctrl);
    int64_t x = *reinterpret_cast<int64_t*>(c + CO_RECT_X);
    int64_t y = *reinterpret_cast<int64_t*>(c + CO_RECT_Y);
    void* p = *reinterpret_cast<void**>(c + CO_PARENT);
    while (p != nullptr) {
        uint8_t* pc = reinterpret_cast<uint8_t*>(p);
        x += *reinterpret_cast<int64_t*>(pc + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(pc + CO_RECT_Y);
        p = *reinterpret_cast<void**>(pc + CO_PARENT);
    }
    out[0] = x;
    out[1] = y;
}

// 官方 UI_DrawStringHAlign 的 font 参数来自 [0x2f3000+0x628] 指向配置的 +4（UIWorldMap_Draw 0xd3684）
int font_id() {
    if (g_base == 0) return 1;
    void** got = reinterpret_cast<void**>(g_base + G_FONT_CONFIG_GOT_VMA);
    if (got == nullptr || *got == nullptr) return 1;
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(*got) + 4);
}

// ---- 格子按钮回调 ----

void cell0_log_clicked(void* ctrl) {
    CUSTOM_LOG("CUSTOM-PANEL: cell0 log button clicked (ctrl=%p)", ctrl);
}

void cell1_stack_clicked(void* ctrl) {
    bool target = !stack_limit_enabled();
    bool ok = set_stack_limit_enabled(target);
    CUSTOM_LOG("CUSTOM-PANEL: cell1 stack limit %s -> %s", target ? "ON" : "OFF", ok ? "ok" : "failed");
    if (ok && g_cells[1].btn != nullptr && fn_ctrl_btn_set_text != nullptr) {
        static char t[CB_TEXT_SIZE];
        snprintf(t, sizeof(t), "堆叠上限:%s", target ? "开" : "关");
        fn_ctrl_btn_set_text(g_cells[1].btn, t);
    }
}

void cell2_close_clicked(void* ctrl) {
    CUSTOM_LOG("CUSTOM-PANEL: cell2 close clicked");
    if (fn_ui_set_popup_process_info == nullptr) return;
    fn_ui_set_popup_process_info(3, 0);  // 弹出自定义面板
    fn_ui_set_popup_process_info(3, 0);  // 弹出商店
}

// ---- 绘制回调（DrawProc，x0=ctrl；ControlButton_Draw 0xaac2c 分发调用）----

void custom_btn_draw(void* ctrl) {
    if (g_base == 0) return;
    uint8_t* data = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(ctrl) + CO_DATA);
    if (data == nullptr) return;
    int64_t rect[2];
    ctrl_abs_point(ctrl, rect);
    static int dbg = 0;
    if (++dbg % 60 == 1) {
        // font 来源实证：GOT 槽 0x2f3000+0x628（font_id 路径）、字体对象槽 0x2f3000+0xf88
        void** slot_f88 = reinterpret_cast<void**>(g_base + 0x2f3f88);
        void* font_obj = (slot_f88 != nullptr) ? *slot_f88 : nullptr;
        int f88_first = 0;
        if (font_obj != nullptr) f88_first = *reinterpret_cast<int*>(font_obj);
        CUSTOM_LOG("btn_draw: data=%s rect=(%lld,%lld) font_id=%d f88_obj=%p f88[0]=%d",
                   reinterpret_cast<char*>(data),
                   static_cast<long long>(rect[0]), static_cast<long long>(rect[1]),
                   font_id(), font_obj, f88_first);
    }
    // GRPX_DrawPart（贴图）在 POPUPSTATE_Process（MainProcess 阶段，非 GL 绘制帧）调用会
    // SIGSEGV（真机实测，纹理 GL 状态未就绪）→ 降级为色块背景 + 文本，布局/功能先行验证。
    // GRPX_FillRectAlpha 的 alpha>0x64(100) 直接 return 不绘制（反汇编 8fcd0）→ 用 FillRect（alpha 嵌 ABGR 色值）
    if (fn_grpx_fill_rect != nullptr) {
        fn_grpx_fill_rect(static_cast<int>(rect[0]), static_cast<int>(rect[1]),
                          BTN_W, BTN_H, 0xFF606060);  // ABGR 灰按钮底
    }
    if (fn_grpx_set_font_color != nullptr) fn_grpx_set_font_color(0xFFFFFFFF);
    if (fn_ui_draw_string_halign != nullptr && data[0] != 0) {
        fn_ui_draw_string_halign(reinterpret_cast<char*>(data),
                                 static_cast<int>(rect[0]) + 8, static_cast<int>(rect[1]) + 14,
                                 font_id(), 1);
    }
}

void custom_desc_draw(void* ctrl) {
    if (g_base == 0) return;
    uint8_t* data = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(ctrl) + CO_DATA);
    if (data == nullptr || data[0] == 0 || fn_ui_draw_string_halign == nullptr) return;
    int64_t rect[2];
    ctrl_abs_point(ctrl, rect);
    if (fn_grpx_set_font_color != nullptr) fn_grpx_set_font_color(0xFFC8C8C8);
    fn_ui_draw_string_halign(reinterpret_cast<char*>(data),
                             static_cast<int>(rect[0]) + 2, static_cast<int>(rect[1]) + 2,
                             font_id(), 1);
}

// ---- state list 条目回调（注入 INAP 条目，IAP 已被模块屏蔽=死条目）----

void custom_panel_enter() {
    CUSTOM_LOG("custom panel enter");
    if (g_root != nullptr) {
        g_panel_active = true;
        return;
    }
    if (g_base == 0 || fn_ctrl_create == nullptr || fn_ctrl_btn_create == nullptr ||
        fn_ctrl_btn_set_text == nullptr || fn_ctrl_btn_set_draw_proc == nullptr ||
        fn_ctrl_set_active == nullptr ||
        fn_calc_res_width == nullptr || fn_calc_res_height == nullptr) {
        CUSTOM_LOG("custom panel enter: symbols not resolved");
        return;
    }
    // UI_CreateGroupBaseControl 要求 parent 非空（反汇编 cbz x0 → ret 0）→ 用 ControlObject_Create
    // 建独立根（type=0 组容器），手动补 Proc（TouchHandle 分发）+ Active（CreateControlInfo 已置 0x20）。
    void* root = fn_ctrl_create(0, nullptr, nullptr, nullptr);
    if (root == nullptr) {
        CUSTOM_LOG("custom panel enter: root create failed");
        return;
    }
    set_ctrl_rect(root, fn_calc_res_width(), fn_calc_res_height(), ROOT_W, ROOT_H);
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(root) + CO_TYPE) = 0;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(root) + CO_ACTIVE) = 0x20;
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(root) + CO_PROC) =
        g_base + F_TOUCH_HANDLE_CONTROL_EVENT_PROC_VMA;
    // root 无数据块：CO_CONTROL_PROC 若指向 ControlButton_ControlEventProc，
    // 触摸选中时 GetData=null → [data+0x70] SIGSEGV（真机实测）→ 清零，不参与游戏触摸链
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(root) + CO_CONTROL_PROC) = 0;
    static const char* kBtnText[3] = {"写日志", "堆叠上限:关", "关闭面板"};
    static const char* kDescText[3] = {"点击写入一条日志", "切换999堆叠补丁", "返回并关闭商店"};
    static void (*const kBtnProc[3])(void*) = {cell0_log_clicked, cell1_stack_clicked, cell2_close_clicked};
    const int col_x[2] = {CELL_COL0_X, CELL_COL1_X};
    const int row_y[3] = {CELL_ROW0_Y, CELL_ROW1_Y, CELL_ROW2_Y};
    for (int i = 0; i < 3; ++i) {
        g_cell_procs[i] = kBtnProc[i];
        int bx = col_x[i % 2];
        int by = row_y[i / 2];
        void* btn = fn_ctrl_btn_create(root, reinterpret_cast<void*>(kBtnProc[i]));
        if (btn == nullptr) continue;
        set_ctrl_rect(btn, bx, by, BTN_W, BTN_H);
        fn_ctrl_btn_set_text(btn, const_cast<char*>(kBtnText[i]));
        fn_ctrl_btn_set_draw_proc(btn, reinterpret_cast<void*>(&custom_btn_draw));
        void* desc = fn_ctrl_btn_create(root, nullptr);
        if (desc == nullptr) continue;
        set_ctrl_rect(desc, bx, by + BTN_H + 8, DESC_W, DESC_H);
        fn_ctrl_btn_set_text(desc, const_cast<char*>(kDescText[i]));
        fn_ctrl_btn_set_draw_proc(desc, reinterpret_cast<void*>(&custom_desc_draw));
        fn_ctrl_set_active(desc, 0);  // 说明文本不参与触摸
        g_cells[i].btn = btn;
        g_cells[i].desc = desc;
    }
    g_root = root;
    g_panel_active = true;
    CUSTOM_LOG("custom panel enter: root=%p 3 cells created", root);
}

void custom_panel_process() {
    if (!g_panel_active || g_root == nullptr || g_base == 0) return;
    if (fn_grpx_start == nullptr || fn_grpx_end == nullptr) return;
    static int dbg_frame = 0;
    if (++dbg_frame % 60 == 1) {
        int64_t pt[2];
        ctrl_abs_point(g_root, pt);
        CUSTOM_LOG("process: frame=%d root_abs=(%lld,%lld) cells ok=%d/%d", dbg_frame,
                   static_cast<long long>(pt[0]), static_cast<long long>(pt[1]),
                   g_cells[0].btn != nullptr, g_cells[2].btn != nullptr);
    }
    // 模拟 Scene_Draw_POPUP_SC_STORE 的 LCD 结构（绘制才进入渲染）：
    // 完整路径(RefreshLCDFlag=1): SaveLCD + 清标志；快路径(=0): RestoreLCD 恢复背景缓冲
    if (fn_ui_get_refresh_lcd_flag != nullptr && fn_ui_set_refresh_lcd_flag != nullptr) {
        if (fn_ui_get_refresh_lcd_flag()) {
            if (fn_grp_save_lcd != nullptr) fn_grp_save_lcd();
            fn_ui_set_refresh_lcd_flag(0);
        } else {
            if (fn_grp_restore_lcd != nullptr) fn_grp_restore_lcd();
        }
    }
    fn_grpx_start();
    // GRPX_FillRect 颜色=ABGR（0xFFFF0000 显示为蓝，真机实测）→ 面板色用 ABGR
    // 遮罩用 FillRectAlpha（alpha 0x3c<=0x64 有效，反汇编 8fcd0 验证）
    if (fn_grpx_fill_rect_alpha != nullptr) {
        fn_grpx_fill_rect_alpha(231, 0, ROOT_W, ROOT_H, 0xFF000000, 0x3c);  // 半透明黑遮罩
    }
    for (int i = 0; i < 3; ++i) {
        if (g_cells[i].btn != nullptr && fn_ctrl_btn_draw != nullptr) {
            fn_ctrl_btn_draw(g_cells[i].btn);
        }
        if (g_cells[i].desc != nullptr && fn_ctrl_btn_draw != nullptr) {
            fn_ctrl_btn_draw(g_cells[i].desc);
        }
    }
    fn_grpx_end();
}

void custom_panel_f4() {}  // noop：绘制在 process（官方 Scene_Draw 结构），f4 是 GRP 背景层会被覆盖

void custom_panel_f3() {
    CUSTOM_LOG("custom panel f3 (terminate)");
    g_panel_active = false;
    // 不调 TouchHandle_DeleteControl：root 为 ControlObject_Create 独立创建（无完整父链/
    // 触摸注册），游戏递归删除遍历链表 SIGBUS（真机实测 tombstone_17）→ 控件树保留复用，
    // 下次 enter 直接沿用（g_root 非空分支），进程重启时由系统释放。
}

uint32_t custom_panel_event(uint64_t event, uint64_t param, uint64_t param2) {
    if (g_root == nullptr || param == 0) return 1;
    // 不转发 fn_touch_handle_event：游戏触摸链选中 root/按钮会调 ControlButton_ControlEventProc，
    // 而 root 无数据块（GetData=null，+0x70 读 SIGSEGV 实测）→ 自实现命中测试。
    // param = 触摸结构 {i64 x, i64 y, i64 id}（TouchHandle_Event 0x17 分支 a3a00 拷贝 24B）
    if (event == 0x17) {
        int64_t tx = *reinterpret_cast<const int64_t*>(param);
        int64_t ty = *reinterpret_cast<const int64_t*>(param + 8);
        for (int i = 0; i < 3; ++i) {
            void* btn = g_cells[i].btn;
            if (btn == nullptr || g_cell_procs[i] == nullptr) continue;
            int64_t pt[2];
            ctrl_abs_point(btn, pt);
            if (tx >= pt[0] && tx < pt[0] + BTN_W && ty >= pt[1] && ty < pt[1] + BTN_H) {
                CUSTOM_LOG("event: cell%d hit tx=%lld ty=%lld rect=(%lld,%lld)", i,
                           static_cast<long long>(tx), static_cast<long long>(ty),
                           static_cast<long long>(pt[0]), static_cast<long long>(pt[1]));
                g_cell_procs[i](btn);
                break;
            }
        }
    }
    return 1;  // 已处理（吞掉触摸，不穿透到底层）
}

// ---- state list 条目定位与注入 ----

uint8_t* find_state_entry(uintptr_t enter_vma) {
    if (g_base == 0) return nullptr;
    void** got = reinterpret_cast<void**>(g_base + G_POPUP_STATE_LIST_GOT_VMA);
    if (got == nullptr || *got == nullptr) return nullptr;
    uint8_t* list = reinterpret_cast<uint8_t*>(*got);
    for (int i = 0; i < 27; ++i) {
        uintptr_t enter = *reinterpret_cast<uintptr_t*>(list + i * POPUP_STATE_SIZE + 0x10);
        if (enter == g_base + enter_vma) return list + i * POPUP_STATE_SIZE;
    }
    return nullptr;
}

bool inject_state_entry() {
    std::lock_guard<std::mutex> lock(g_custom_mtx);
    if (g_state_entry != nullptr) return true;
    if (g_base == 0) return false;
    uint8_t* entry = find_state_entry(F_PANEL_UNK1_ENTER);  // INAP_GEMSHOP（IAP 屏蔽=死条目）
    if (entry == nullptr) {
        CUSTOM_LOG("state entry (INAP_GEMSHOP) not found");
        return false;
    }
    memcpy(g_state_backup, entry, POPUP_STATE_SIZE);
    *reinterpret_cast<uintptr_t*>(entry + 0x10) = reinterpret_cast<uintptr_t>(&custom_panel_enter);
    *reinterpret_cast<uintptr_t*>(entry + 0x18) = reinterpret_cast<uintptr_t>(&custom_panel_process);
    *reinterpret_cast<uintptr_t*>(entry + 0x28) = reinterpret_cast<uintptr_t>(&custom_panel_f3);
    *reinterpret_cast<uintptr_t*>(entry + 0x30) = reinterpret_cast<uintptr_t>(&custom_panel_f4);
    *reinterpret_cast<uintptr_t*>(entry + 0x38) = reinterpret_cast<uintptr_t>(&custom_panel_event);
    g_state_entry = entry;
    g_state_id = *reinterpret_cast<int32_t*>(entry);
    CUSTOM_LOG("state entry %d injected (enter/process/f3/f4/event)", g_state_id);
    return true;
}

// ---- 商店返回按钮替换 ----

void* store_back_btn() {
    if (g_base == 0) return nullptr;
    return *reinterpret_cast<void**>(g_base + G_UISTORE_VMA + UISTORE_SLOT_BACK_BTN);
}

void custom_store_btn_clicked(void* ctrl) {
    CUSTOM_LOG("store back btn clicked -> open custom panel");
    if (fn_ui_set_popup_process_info == nullptr || g_state_id < 0) return;
    fn_ui_set_popup_process_info(1, g_state_id);
}

bool inject_store_back_btn() {
    std::lock_guard<std::mutex> lock(g_custom_mtx);
    if (g_btn_injected) return true;
    if (g_base == 0) return false;
    void* btn = store_back_btn();
    if (btn == nullptr) return false;
    uint8_t* data = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(btn) + CO_DATA);
    if (data == nullptr) return false;
    void** slot = reinterpret_cast<void**>(data + CB_EXECUTE_PROC);
    if (*slot == reinterpret_cast<void*>(&custom_store_btn_clicked)) {
        g_btn_injected = true;
        return true;
    }
    if (*slot == nullptr) return false;
    if (!g_store_back_hook.install_typed(slot, &custom_store_btn_clicked)) {
        return false;
    }
    g_btn_injected = true;
    CUSTOM_LOG("store back btn ExecuteProc hooked, ctrl=%p orig=%p", btn, g_store_back_hook.orig);
    return true;
}

void ensure_inject_thread() {
    if (g_thread_started.exchange(true)) return;
    std::thread([]() {
        for (;;) {
            if (g_btn_want.load()) {
                bool store_open = data_popup_top_vma() == F_PANEL_SHOP_ENTER;  // 相对偏移
                if (store_open) {
                    inject_state_entry();
                    inject_store_back_btn();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();
}

}  // namespace

std::string data_custom_btn_inject() {
    g_btn_want.store(true);
    ensure_inject_thread();
    if (g_state_id >= 0) {
        if (inject_store_back_btn()) return op_ok();
        return op_err("store panel not open");
    }
    return op_err("state entry not injected (wait for store panel)");
}

std::string data_custom_panel_open() {
    if (g_state_id < 0 && !inject_state_entry()) return op_err("state entry inject failed");
    if (fn_ui_set_popup_process_info == nullptr) return op_err("symbol not resolved");
    fn_ui_set_popup_process_info(1, g_state_id);
    return op_ok();
}

std::string data_custom_restore() {
    std::lock_guard<std::mutex> lock(g_custom_mtx);
    g_btn_want.store(false);
    if (g_btn_injected) {
        // 商店关闭后旧按钮已释放：uninstall 会写已释放内存 → 仅当商店仍打开时还原
        if (data_popup_top_vma() == F_PANEL_SHOP_ENTER) {  // 相对偏移
            g_store_back_hook.uninstall();
        }
        g_btn_injected = false;
    }
    if (g_state_entry != nullptr) {
        memcpy(g_state_entry, g_state_backup, POPUP_STATE_SIZE);
        g_state_entry = nullptr;
        g_state_id = -1;
    }
    g_panel_active = false;  // 控件树保留复用（DeleteControl SIGBUS，见 custom_panel_f3）
    return op_ok();
}

std::string data_custom_status_json() {
    std::lock_guard<std::mutex> lock(g_custom_mtx);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{"
        "\"btn_injected\":%s,\"store_btn\":\"%p\","
        "\"state_id\":%d,\"state_injected\":%s,"
        "\"panel_active\":%s,\"root\":\"%p\","
        "\"stack_limit_enabled\":%s,"
        "\"screen\":\"%s\"}",
        g_btn_injected ? "true" : "false", store_back_btn(),
        g_state_id, g_state_entry != nullptr ? "true" : "false",
        g_panel_active ? "true" : "false", g_root,
        stack_limit_enabled() ? "true" : "false",
        data_ui_screen());
    return std::string(buf);
}
