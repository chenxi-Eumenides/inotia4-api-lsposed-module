#include <jni.h>

#include "game_access.h"
#include "game_data.h"

// JNI 导出层：仅做参数传递与字符串转换，逻辑在 game_access / game_data。

extern "C" JNIEXPORT jboolean JNICALL
Java_com_inotia4_export_NativeBridge_nativeInit(JNIEnv*, jclass) {
    return bridge_init() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetActiveQuest(JNIEnv*, jclass) {
    return data_active_quest();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetInitReport(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_init_report().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetPlayerJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_player_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetPartyJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_party_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetInventoryJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_inventory_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetMapJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_map_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetUnitsJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_units_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetUiJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_ui_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetSkillsJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_skills_json().c_str());
}
