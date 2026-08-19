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

bool ui_get_rect(void* ctrl, UiRect* rect) {
    if (ctrl == nullptr || rect == nullptr) return false;
    uint8_t* data = static_cast<uint8_t*>(ctrl);
    rect->x = *reinterpret_cast<int64_t*>(data + CO_RECT_X);
    rect->y = *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
    rect->w = *reinterpret_cast<int64_t*>(data + CO_RECT_W);
    rect->h = *reinterpret_cast<int64_t*>(data + CO_RECT_H);
    return true;
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

void* find_child_in_area(void* parent, UiRect area, int depth) {
    if (parent == nullptr || depth > 4 || fn_ctrl_get_count == nullptr || fn_ctrl_get_child == nullptr) {
        return nullptr;
    }
    uint32_t count = fn_ctrl_get_count(parent);
    for (uint32_t i = 0; i < count; ++i) {
        void* child = fn_ctrl_get_child(parent, i);
        UiRect rect{};
        if (child == nullptr || !ui_get_rect(child, &rect)) continue;
        uint8_t* data = static_cast<uint8_t*>(child);
        bool is_button = *reinterpret_cast<uint32_t*>(data + CO_TYPE) == 3 &&
                         *reinterpret_cast<void**>(data + CO_DATA) != nullptr;
        if (is_button && rect.x >= area.x && rect.y >= area.y && rect.x + rect.w <= area.x + area.w &&
            rect.y + rect.h <= area.y + area.h) {
            return child;
        }
        void* nested = find_child_in_area(child, area, depth + 1);
        if (nested != nullptr) return nested;
    }
    return nullptr;
}

void* ui_find_child_in_area(void* parent, UiRect area) {
    return find_child_in_area(parent, area, 0);
}

bool ui_clone_button_style(void* target, void* source, UiClickProc on_click) {
    if (target == nullptr || source == nullptr) return false;
    uint8_t* target_data = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(target) + CO_DATA);
    uint8_t* source_data = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(source) + CO_DATA);
    if (target_data == nullptr || source_data == nullptr) return false;
    std::memcpy(target_data, source_data, CB_SIZE);
    *reinterpret_cast<void**>(target_data + CB_EXECUTE_PROC) = reinterpret_cast<void*>(on_click);
    return true;
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

void ui_begin_frame() {
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
}

void ui_fill_rect_alpha(UiRect rect, uint32_t color, uint32_t alpha) {
    if (fn_grpx_fill_rect_alpha == nullptr) return;
    fn_grpx_fill_rect_alpha(static_cast<int>(rect.x), static_cast<int>(rect.y),
                            static_cast<int>(rect.w), static_cast<int>(rect.h), color, alpha);
}

void ui_begin_frame(UiRect mask, UiRect panel, uint32_t panel_color) {
    ui_begin_frame();
    ui_fill_rect_alpha(mask, 0xFF000000, 0x3c);
    ui_fill_rect_alpha(panel, panel_color, 0x50);
}

void ui_end_frame() {
    if (fn_grpx_end != nullptr) fn_grpx_end();
}

void ui_draw_panel_decor(UiRect panel, const int64_t* separator_y, size_t separator_count,
                          uint32_t color) {
    if (fn_grpx_fill_rect == nullptr) return;
    constexpr int64_t kLine = 3;
    fn_grpx_fill_rect(static_cast<int>(panel.x), static_cast<int>(panel.y),
                      static_cast<int>(panel.w), static_cast<int>(kLine), color);
    fn_grpx_fill_rect(static_cast<int>(panel.x), static_cast<int>(panel.y + panel.h - kLine),
                      static_cast<int>(panel.w), static_cast<int>(kLine), color);
    for (size_t i = 0; i < separator_count; ++i) {
        fn_grpx_fill_rect(static_cast<int>(panel.x), static_cast<int>(separator_y[i]),
                          static_cast<int>(panel.w), static_cast<int>(kLine), color);
    }
}

void ui_draw_vertical_line(int64_t x, int64_t y, int64_t h, uint32_t color, int thickness) {
    if (fn_grpx_fill_rect == nullptr || thickness <= 0 || h <= 0) return;
    fn_grpx_fill_rect(static_cast<int>(x), static_cast<int>(y), thickness, static_cast<int>(h), color);
}

void ui_draw_text(void* ctrl, int x_offset, int y_offset, uint32_t color) {
    if (ctrl == nullptr) return;
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
    if (fn_grpx_set_font_color_rgb != nullptr && fn_grpx_draw_string_with_font != nullptr) {
        fn_grpx_set_font_color_rgb(static_cast<int32_t>(color & 0xff),
                                   static_cast<int32_t>((color >> 8) & 0xff),
                                   static_cast<int32_t>((color >> 16) & 0xff));
        fn_grpx_draw_string_with_font(reinterpret_cast<char*>(data), static_cast<int>(x) + x_offset,
                                     static_cast<int>(y) + y_offset, 0, 1);
    } else if (fn_ui_draw_string_in_width_with_font != nullptr) {
        fn_ui_draw_string_in_width_with_font(reinterpret_cast<char*>(data), static_cast<int>(x) + x_offset,
                                             static_cast<int>(y) + y_offset, 0x1000, 1, color, 0, 0);
    } else if (fn_ui_draw_string_halign != nullptr) {
        fn_ui_draw_string_halign(reinterpret_cast<char*>(data), static_cast<int>(x) + x_offset,
                                 static_cast<int>(y) + y_offset, 1, 1);
    }
}

void ui_draw_text_centered(void* ctrl, int y_offset, uint32_t color) {
    if (ctrl == nullptr) return;
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
    uint8_t* ctrl_data = static_cast<uint8_t*>(ctrl);
    x += *reinterpret_cast<int64_t*>(ctrl_data + CO_RECT_W) / 2;
    if (fn_grpx_set_font_color_rgb != nullptr) {
        fn_grpx_set_font_color_rgb(static_cast<int32_t>(color & 0xff),
                                   static_cast<int32_t>((color >> 8) & 0xff),
                                   static_cast<int32_t>((color >> 16) & 0xff));
    }
    if (fn_ui_draw_string_halign != nullptr) {
        fn_ui_draw_string_halign(reinterpret_cast<char*>(data), static_cast<int>(x),
                                 static_cast<int>(y) + y_offset, 1, 1);
        return;
    }
    ui_draw_text(ctrl, 0, y_offset, color);
}

bool ui_load_image_unit(int32_t unit) {
    if (fn_imgsys_unit_load == nullptr) return false;
    fn_imgsys_unit_load(unit);
    return fn_imgsys_get_group != nullptr && fn_imgsys_get_group(unit) != nullptr;
}

void ui_unload_image_unit(int32_t unit) {
    if (fn_imgsys_unit_unload != nullptr) fn_imgsys_unit_unload(unit);
}

bool ui_draw_control_image_part(void* ctrl, int32_t unit, int32_t loc, int32_t type, int32_t flip) {
    if (ctrl == nullptr || fn_imgsys_get_group == nullptr || fn_imgsys_get_loc == nullptr ||
        fn_grpx_draw_part == nullptr) {
        return false;
    }
    void* group = fn_imgsys_get_group(unit);
    void* part = fn_imgsys_get_loc(unit, loc);
    if (group == nullptr || part == nullptr) return false;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    fn_grpx_draw_part(group, static_cast<int32_t>(x), static_cast<int32_t>(y), part, type, flip, 0);
    return true;
}

bool ui_draw_control_image_part_centered(void* ctrl, int32_t unit, int32_t loc, int32_t type,
                                         int32_t flip) {
    if (ctrl == nullptr) return false;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    uint8_t* data = static_cast<uint8_t*>(ctrl);
    x += *reinterpret_cast<int64_t*>(data + CO_RECT_W) / 2;
    y += *reinterpret_cast<int64_t*>(data + CO_RECT_H) / 2;
    return ui_draw_image_part(unit, loc, static_cast<int32_t>(x), static_cast<int32_t>(y), type, flip);
}

bool ui_draw_image_part(int32_t unit, int32_t loc, int32_t x, int32_t y, int32_t type, int32_t flip) {
    if (fn_imgsys_get_group == nullptr || fn_imgsys_get_loc == nullptr || fn_grpx_draw_part == nullptr) {
        return false;
    }
    void* group = fn_imgsys_get_group(unit);
    void* part = fn_imgsys_get_loc(unit, loc);
    if (group == nullptr || part == nullptr) return false;
    fn_grpx_draw_part(group, x, y, part, type, flip, 0);
    return true;
}

bool ui_draw_control_title_image_part(void* ctrl, int32_t loc) {
    if (ctrl == nullptr || fn_get_group_title_img_type == nullptr || fn_imgsys_get_group == nullptr ||
        fn_imgsys_get_loc == nullptr || fn_grpx_draw_part == nullptr) {
        return false;
    }
    int32_t unit = fn_get_group_title_img_type();
    void* group = fn_imgsys_get_group(unit);
    void* part = fn_imgsys_get_loc(unit, loc);
    if (group == nullptr || part == nullptr) return false;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    uint8_t* data = static_cast<uint8_t*>(ctrl);
    x += *reinterpret_cast<int64_t*>(data + CO_RECT_W) / 2;
    y += *reinterpret_cast<int64_t*>(data + CO_RECT_H) / 2;
    fn_grpx_draw_part(group, static_cast<int32_t>(x), static_cast<int32_t>(y), part, 2, 1, 0);
    return true;
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

void ui_draw_button_border(void* ctrl, UiRect size, uint32_t color, int thickness) {
    if (ctrl == nullptr || fn_grpx_fill_rect == nullptr || thickness <= 0) return;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    fn_grpx_fill_rect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(size.w), thickness, color);
    fn_grpx_fill_rect(static_cast<int>(x), static_cast<int>(y + size.h - thickness),
                      static_cast<int>(size.w), thickness, color);
    fn_grpx_fill_rect(static_cast<int>(x), static_cast<int>(y), thickness, static_cast<int>(size.h), color);
    fn_grpx_fill_rect(static_cast<int>(x + size.w - thickness), static_cast<int>(y),
                      thickness, static_cast<int>(size.h), color);
}

void ui_draw_control_alpha_overlay(void* ctrl, UiRect size, uint32_t color, uint32_t alpha) {
    if (ctrl == nullptr || fn_grpx_fill_rect_alpha == nullptr) return;
    int64_t x = 0;
    int64_t y = 0;
    for (void* current = ctrl; current != nullptr;) {
        uint8_t* data = static_cast<uint8_t*>(current);
        x += *reinterpret_cast<int64_t*>(data + CO_RECT_X);
        y += *reinterpret_cast<int64_t*>(data + CO_RECT_Y);
        current = *reinterpret_cast<void**>(data + CO_PARENT);
    }
    fn_grpx_fill_rect_alpha(static_cast<int>(x), static_cast<int>(y), static_cast<int>(size.w),
                            static_cast<int>(size.h), color, alpha);
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

void* ui_popup_state_callback(uintptr_t enter_vma, size_t offset) {
    uint8_t* entry = find_popup_state(enter_vma);
    if (entry == nullptr || offset >= kPopupStateSize) return nullptr;
    return *reinterpret_cast<void**>(entry + offset);
}

void ui_popup_state_restore(UiPopupStateHandle* handle) {
    if (handle == nullptr || handle->entry == nullptr) return;
    std::memcpy(handle->entry, handle->backup, kPopupStateSize);
    handle->entry = nullptr;
    handle->state_id = -1;
}
