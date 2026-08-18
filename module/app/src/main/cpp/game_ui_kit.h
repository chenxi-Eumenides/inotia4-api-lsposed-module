#pragma once

#include <cstddef>
#include <cstdint>

struct UiRect {
    int64_t x;
    int64_t y;
    int64_t w;
    int64_t h;
};

using UiClickProc = void (*)(void* ctrl);
using UiDrawProc = void (*)(void* ctrl);

void* ui_create_root(UiRect rect);
void* ui_create_button(void* parent, UiRect rect, const char* text,
                       UiClickProc on_click, UiDrawProc draw_proc);
void* ui_create_label(void* parent, UiRect rect, const char* text, UiDrawProc draw_proc);
void ui_set_rect(void* ctrl, UiRect rect);
bool ui_hit_test(void* ctrl, int64_t x, int64_t y, UiRect rect);

void ui_begin_frame(UiRect mask, UiRect panel, uint32_t panel_color);
void ui_end_frame();
void ui_draw_text(void* ctrl, int x_offset, int y_offset, uint32_t color);
void ui_draw_button_background(void* ctrl, UiRect size, uint32_t color);

struct UiPopupStateHooks {
    void (*enter)();
    void (*process)();
    void (*f3)();
    void (*f4)();
    uint32_t (*event)(uint64_t event, uint64_t param, uint64_t param2);
};

struct UiPopupStateHandle {
    uint8_t* entry = nullptr;
    int32_t state_id = -1;
    uint8_t backup[0x40] = {};
};

bool ui_popup_state_inject(UiPopupStateHandle* handle, uintptr_t enter_vma,
                           const UiPopupStateHooks& hooks);
void ui_popup_state_restore(UiPopupStateHandle* handle);
