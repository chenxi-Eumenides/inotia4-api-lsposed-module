#pragma once

// Host 构建桩：替代 Android NDK 的 <android/log.h>。
// 被测源文件（game_json/game_tiles/game_nav）仅通过 MOVE_LOG/TILES_LOG 宏调用
// __android_log_print；host 侧无日志后端，退化为空操作。

#define ANDROID_LOG_VERBOSE 2
#define ANDROID_LOG_DEBUG 3
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6

#define __android_log_print(prio, tag, fmt, ...) ((void)0)
