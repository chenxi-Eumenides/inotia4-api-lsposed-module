#pragma once

#include <string>

// 对话域（parse 域）：NPC 对话框选项 + 对话内容。

std::string data_npc_dialog_options_json();
std::string data_dialog_content_json();

// 对话操作：NPC 交互 + 对话选择分发器 + 剧情推进/跳过。
std::string data_op_npc_interact();
std::string data_op_npc_dialog_select(int index);
std::string data_op_dialog_select(const std::string& action, int index);
std::string data_op_dialog_ok();
std::string data_op_dialog_cancel();
std::string story_next();
std::string story_skip();
