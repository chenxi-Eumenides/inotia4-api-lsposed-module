#include "game_ui_kit.h"

#include "game_access.h"
#include "game_symbols.h"

#include <cstring>

namespace {

constexpr int kPopupStateCount = 27;
constexpr size_t kPopupStateSize = 0x40;

void* popup_state_list() {
    if (g_base == 0) return nullptr;
    void** got = reinterpret_cast<void**>(g_base + G_POPUP_STATE_LIST_GOT_VMA);
    return got == nullptr ? nullptr : *got;
}

uint8_t* find_popup_state(uintptr_t enter_vma) {
    uint8_t* list = static_cast<uint8_t*>(popup_state_list());
    if (list == nullptr) return nullptr;
    for (int i = 0; i < kPopupStateCount; ++i) {
        uintptr_t enter = *reinterpret_cast<uintptr_t*>(list + i * kPopupStateSize + 0x10);
        if (enter == g_base + enter_vma) return list + i * kPopupStateSize;
    }
    return nullptr;
}

void set_control_proc(void* ctrl, uintptr_t proc) {
    *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ctrl) + CO_CONTROL_PROC) = proc;
}

void set_execute_proc(void* ctrl, UiClickProc proc) {
    if (proc == nullptr) return;
    uint8_t* data = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(ctrl) + CO_DATA);
    if (data != nullptr) {
        *reinterpret_cast<void**>(data + CB_EXECUTE_PROC) = reinterpret_cast<void*>(proc);
    }
}

}  // namespace

void ui_set_rect(void* ctrl, UiRect rect) {
    if (ctrl == nullptr) return;
    uint8_t* data = static_cast<uint8_t*>(ctrl);
    *reinterpret_cast<int64_t*>(data + CO_RECT_X) = rect.x;
    *reinterpret_cast<int64_t*>(data + CO_RECT_Y) = rect.y;
    *reinterpret_cast<int64_t*>(data + CO_RECT_W) = rect.w;
    *reinterpret_cast<int64_t*>(data + CO_RECT_H) = rect.h;
}

void* ui_create_root(UiRect rect) {
    if (g_base == 0 || fn_ctrl_create == nullptr || fn_calc_res_width == nullptr ||
        fn_calc_res_height == nullptr) {
        return nullptr;
    }
    void* root = fn_ctrl_create(0, nullptr, nullptr, nullptr);
    if (root == nullptr) return nullptr;
    ui_set_rect(root, rect);
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(root) + CO_TYPE) = 0;
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(root) + CO_ACTIVE) = 0x20;
    *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(root) + CO_PROC) =
        g_base + F_TOUCH_HANDLE_CONTROL_EVENT_PROC_VMA;
    set_control_proc(root, 0);
    return root;
}

void* ui_create_button(void* parent, UiRect rect, const char* text,
                       UiClickProc on_click, UiDrawProc draw_proc) {
    if (parent == nullptr || fn_ctrl_btn_create == nullptr || fn_ctrl_btn_set_text == nullptr ||
        fn_ctrl_set_active == nullptr) {
        return nullptr;
    }
    void* ctrl = fn_ctrl_btn_create(parent, nullptr);
    if (ctrl == nullptr) return nullptr;
    ui_set_rect(ctrl, rect);
    if (text != nullptr) fn_ctrl_btn_set_text(ctrl, const_cast<char*>(text));
    if (draw_proc != nullptr && fn_ctrl_btn_set_draw_proc != nullptr) {
        fn_ctrl_btn_set_draw_proc(ctrl, reinterpret_cast<void*>(draw_proc));
    }
    fn_ctrl_set_active(ctrl, 0x20);
    set_control_proc(ctrl, g_base + F_CONTROL_BUTTON_CONTROL_EVENT_PROC_VMA);
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctrl) + CO_EVENT_CALL_TYPE) = 0x200;
    set_execute_proc(ctrl, on_click);
    return ctrl;
}

void* ui_create_label(void* parent, UiRect rect, const char* text, UiDrawProc draw_proc) {
    if (parent == nullptr || fn_ctrl_btn_create == nullptr || fn_ctrl_btn_set_text == nullptr ||
        fn_ctrl_set_active == nullptr) {
        return nullptr;
    }
    void* ctrl = fn_ctrl_btn_create(parent, nullptr);
    if (ctrl == nullptr) return nullptr;
    ui_set_rect(ctrl, rect);
    if (text != nullptr) fn_ctrl_btn_set_text(ctrl, const_cast<char*>(text));
    if (draw_proc != nullptr && fn_ctrl_btn_set_draw_proc != nullptr) {
        fn_ctrl_btn_set_draw_proc(ctrl, reinterpret_cast<void*>(draw_proc));
    }
    fn_ctrl_set_active(ctrl, 0);
    return ctrl;
}

bool ui_hit_test(void* ctrl, int64_t x, int64_t y, UiRect rect) {
    if (ctrl == nullptr) return false;
    int64_t absolute_x = 0;
    int64_t absolute_y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        absolute_x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        absolute_y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    return x >= absolute_x && x < absolute_x + rect.w && y >= absolute_y && y < absolute_y + rect.h;
}

void ui_begin_frame(UiRect mask, UiRect panel, uint32_t panel_color) {
    if (fn_ui_get_refresh_lcd_flag != nullptr && fn_ui_set_refresh_lcd_flag != nullptr) {
        if (fn_ui_get_refresh_lcd_flag()) {
            if (fn_grp_save_lcd != nullptr) fn_grp_save_lcd();
            fn_ui_set_refresh_lcd_flag(0);
        } else if (fn_grp_restore_lcd != nullptr) {
            fn_grp_restore_lcd();
        }
    }
    if (fn_grpx_start == nullptr) return;
    fn_grpx_start();
    if (fn_grpx_fill_rect_alpha != nullptr) {
        fn_grpx_fill_rect_alpha(static_cast<int>(mask.x), static_cast<int>(mask.y),
                                static_cast<int>(mask.w), static_cast<int>(mask.h), 0xFF000000, 0x3c);
    }
    if (fn_grpx_fill_rect != nullptr) {
        fn_grpx_fill_rect(static_cast<int>(panel.x), static_cast<int>(panel.y),
                          static_cast<int>(panel.w), static_cast<int>(panel.h), panel_color);
    }
}

void ui_end_frame() {
    if (fn_grpx_end != nullptr) fn_grpx_end();
}

void ui_draw_text(void* ctrl, int x_offset, int y_offset, uint32_t color) {
    if (ctrl == nullptr || fn_ui_draw_string_in_width_with_font == nullptr) return;
    uint8_t* data = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(ctrl) + CO_DATA);
    if (data == nullptr || data[0] == 0) return;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* current_data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(current_data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(current_data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(current_data + CO_PARENT);
    }
    if (fn_grpx_set_font_color != nullptr) fn_grpx_set_font_color(color);
    fn_ui_draw_string_in_width_with_font(reinterpret_cast<char*>(data), static_cast<int>(x) + x_offset,
                                         static_cast<int>(y) + y_offset, 0x1000, 1, color, 0, 0);
}

void ui_draw_button_background(void* ctrl, UiRect size, uint32_t color) {
    if (ctrl == nullptr || fn_grpx_fill_rect == nullptr) return;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    fn_grpx_fill_rect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(size.w),
                      static_cast<int>(size.h), color);
}

bool ui_popup_state_inject(UiPopupStateHandle* handle, uintptr_t enter_vma,
                           const UiPopupStateHooks& hooks) {
    if (handle == nullptr || g_base == 0 || handle->entry != nullptr) return false;
    uint8_t* entry = find_popup_state(enter_vma);
    if (entry == nullptr) return false;
    std::memcpy(handle->backup, entry, kPopupStateSize);
    *reinterpret_cast<uintptr_t*>(entry + 0x10) = reinterpret_cast<uintptr_t>(hooks.enter);
    *reinterpret_cast<uintptr_t*>(entry + 0x18) = reinterpret_cast<uintptr_t>(hooks.process);
    *reinterpret_cast<uintptr_t*>(entry + 0x28) = reinterpret_cast<uintptr_t>(hooks.f3);
    *reinterpret_cast<uintptr_t*>(entry + 0x30) = reinterpret_cast<uintptr_t>(hooks.f4);
    *reinterpret_cast<uintptr_t*>(entry + 0x38) = reinterpret_cast<uintptr_t>(hooks.event);
    handle->entry = entry;
    handle->state_id = *reinterpret_cast<int32_t*>(entry);
    return true;
}

void ui_popup_state_restore(UiPopupStateHandle* handle) {
    if (handle == nullptr || handle->entry == nullptr) return;
    std::memcpy(handle->entry, handle->backup, kPopupStateSize);
    handle->entry = nullptr;
    handle->state_id = -1;
}
