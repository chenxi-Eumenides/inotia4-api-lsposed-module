#pragma once

#include <cstdint>
#include <string>
#include <jni.h>

// 模块设置 UI 域（ui-settings v0.6.9）：主菜单环境设置（OPTION）右上角注入入口按钮
// → 点击打开模块设置面板（3 布尔开关可切换 + 监听地址/端口只读显示）。
// 机制：OPTION root 子链动态绘制（UIOption_Draw 0xc4ed0 遍历 [0x306ff0] GetCount/GetChild）
// + 注入 INAP_GEMSHOP 死条目 state（exp6 先例）+ process 内 LCD/GRPX 绘制（exp6 验证色块）。
// 参考 docs/system/ui.md §6 + ui-experiments.md（exp6 文字解法：font=1 官方实证）。
// 链路：懒注入线程轮询 OPTION 打开 → ControlButton_Create 挂 [0x306ff0] 子链 →
// 点击 → UI_SetPopupProcessInfo(1, injected_state_id) 打开自定义面板。

std::string data_settings_ui_inject();      // 启用入口按钮注入（懒注入线程启动）
std::string data_settings_ui_status_json(); // 状态 JSON（注入标志/面板状态/配置项）
std::string data_settings_ui_restore();     // 还原 state 条目 + 清面板（控件树保留复用）
std::string data_settings_ui_open_option(); // 调试：打开主菜单环境设置（模拟点击环境设置按钮）

void settings_register_config_bridge(JNIEnv* env, jclass bridge_class);
