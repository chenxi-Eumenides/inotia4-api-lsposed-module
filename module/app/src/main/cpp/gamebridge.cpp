#include <jni.h>

#include "game_access.h"
#include "game_data.h"

// JNI 导出层：仅做参数传递与字符串转换，逻辑在 game_access / game_data。

extern "C" JNIEXPORT jboolean JNICALL
Java_com_inotia4_export_NativeBridge_nativeInit(JNIEnv*, jclass) {
    return bridge_init() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetBaseAddr(JNIEnv*, jclass) {
    return static_cast<jlong>(g_base);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetFrameCount(JNIEnv*, jclass) {
    return static_cast<jlong>(data_frame_count());
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
Java_com_inotia4_export_NativeBridge_nativeGetGamestateJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_gamestate_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetDebugUiJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_debug_ui_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetSnapshotJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_snapshot_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetSkillsJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_skills_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetMercenariesJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_mercenaries_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetPathJson(JNIEnv* env, jclass, jint tx, jint ty) {
    return env->NewStringUTF(data_path_json(static_cast<int>(tx), static_cast<int>(ty)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeDistanceJson(JNIEnv* env, jclass, jint tx, jint ty) {
    return env->NewStringUTF(data_distance_json(static_cast<int32_t>(tx), static_cast<int32_t>(ty)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetMoney(JNIEnv* env, jclass, jlong money) {
    return env->NewStringUTF(data_op_set_money(static_cast<int64_t>(money)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpAddMoney(JNIEnv* env, jclass, jlong delta) {
    return env->NewStringUTF(data_op_add_money(static_cast<int64_t>(delta)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpMinusMoney(JNIEnv* env, jclass, jlong delta) {
    return env->NewStringUTF(data_op_minus_money(static_cast<int64_t>(delta)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetExperience(JNIEnv* env, jclass, jint role, jlong exp) {
    return env->NewStringUTF(data_op_set_experience(static_cast<int>(role), static_cast<int64_t>(exp)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetLevel(JNIEnv* env, jclass, jint role, jint level) {
    return env->NewStringUTF(data_op_set_level(static_cast<int>(role), static_cast<int32_t>(level)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpAddExperience(JNIEnv* env, jclass, jint role, jlong delta) {
    return env->NewStringUTF(data_op_add_experience(static_cast<int>(role), static_cast<int64_t>(delta)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetStatusPoint(JNIEnv* env, jclass, jint role, jint points) {
    return env->NewStringUTF(data_op_set_status_point(static_cast<int>(role), static_cast<int32_t>(points)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpAddStat(JNIEnv* env, jclass, jint role, jint attr) {
    return env->NewStringUTF(data_op_add_stat(static_cast<int>(role), static_cast<int32_t>(attr)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpStatReset(JNIEnv* env, jclass, jint role) {
    return env->NewStringUTF(data_op_stat_reset(static_cast<int>(role)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSkillReset(JNIEnv* env, jclass, jint role) {
    return env->NewStringUTF(data_op_skill_reset(static_cast<int>(role)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpCast(JNIEnv* env, jclass, jint role, jint actionId) {
    return env->NewStringUTF(data_op_cast(static_cast<int>(role), static_cast<int32_t>(actionId)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpQuestQuit(JNIEnv* env, jclass, jint questId) {
    return env->NewStringUTF(data_op_quest_quit(static_cast<int32_t>(questId)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSave(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_save().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpMainMenu(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_main_menu().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpEnterSlot(JNIEnv* env, jclass, jint slot) {
    return env->NewStringUTF(data_op_enter_slot(static_cast<int32_t>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeRecoverAfterHiveBlock(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_recover_after_hive_block().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeSaveSlotsJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_save_slots_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpNpcInteract(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_npc_interact().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeShopItems(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_shop_items_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpShopBuy(JNIEnv* env, jclass, jint slot) {
    return env->NewStringUTF(data_op_shop_buy(static_cast<int>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeNpcDialogOptions(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_npc_dialog_options_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpNpcDialogNext(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_npc_dialog_next().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpNpcDialogSelect(JNIEnv* env, jclass, jint index) {
    return env->NewStringUTF(data_op_npc_dialog_select(static_cast<int>(index)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeDialogContent(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_dialog_content_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDialogSelect(JNIEnv* env, jclass, jstring action, jint index) {
    const char* a = action != nullptr ? env->GetStringUTFChars(action, nullptr) : nullptr;
    std::string s = a != nullptr ? a : "";
    if (a != nullptr) env->ReleaseStringUTFChars(action, a);
    return env->NewStringUTF(data_op_dialog_select(s, static_cast<int>(index)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpJewel(JNIEnv* env, jclass, jint role, jint bag, jint slot, jint equipSlot) {
    return env->NewStringUTF(data_op_jewel(static_cast<int>(role), static_cast<int>(bag),
        static_cast<int>(slot), static_cast<int>(equipSlot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetAutoAttack(JNIEnv* env, jclass, jint role, jint onoff) {
    return env->NewStringUTF(data_op_set_auto_attack(static_cast<int>(role), static_cast<int32_t>(onoff)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetSkillUsage(JNIEnv* env, jclass, jint role, jint onoff) {
    return env->NewStringUTF(data_op_set_skill_usage(static_cast<int>(role), static_cast<int32_t>(onoff)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpEquip(JNIEnv* env, jclass, jint role, jint bag, jint slot) {
    return env->NewStringUTF(data_op_equip(static_cast<int>(role), static_cast<int>(bag), static_cast<int>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpUnequip(JNIEnv* env, jclass, jint role, jint slot) {
    return env->NewStringUTF(data_op_unequip(static_cast<int>(role), static_cast<int32_t>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSwitchPlayer(JNIEnv* env, jclass, jint slot) {
    return env->NewStringUTF(data_op_switch_player(static_cast<int32_t>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpPartySwap(JNIEnv* env, jclass, jint a, jint b) {
    return env->NewStringUTF(data_op_party_swap(static_cast<int32_t>(a), static_cast<int32_t>(b)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpTeleport(JNIEnv* env, jclass, jint mapId, jint x, jint y) {
    return env->NewStringUTF(data_op_teleport(static_cast<int32_t>(mapId), static_cast<int32_t>(x), static_cast<int32_t>(y)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpRemoveItem(JNIEnv* env, jclass, jint category) {
    return env->NewStringUTF(data_op_remove_item(static_cast<int32_t>(category)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpLearnAction(JNIEnv* env, jclass, jint role, jint actionId, jint level) {
    return env->NewStringUTF(data_op_learn_action(static_cast<int>(role), static_cast<int32_t>(actionId), static_cast<int32_t>(level)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeGetEventsJson(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_events_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpMove(JNIEnv* env, jclass, jint x, jint y) {
    return env->NewStringUTF(data_op_move(static_cast<int32_t>(x), static_cast<int32_t>(y)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpWalk(JNIEnv* env, jclass, jint direction) {
    return env->NewStringUTF(data_op_walk(static_cast<int32_t>(direction)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpWalkStop(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_walk_stop().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpAttack(JNIEnv* env, jclass, jint role, jint target_slot) {
    return env->NewStringUTF(data_op_attack(static_cast<int32_t>(role), static_cast<int32_t>(target_slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpStopCombat(JNIEnv* env, jclass, jint role) {
    return env->NewStringUTF(data_op_stop_combat(static_cast<int32_t>(role)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDialogOk(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_dialog_ok().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDialogCancel(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_dialog_cancel().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpUseItem(JNIEnv* env, jclass, jint bag, jint slot) {
    return env->NewStringUTF(data_op_use_item(static_cast<int>(bag), static_cast<int>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDiceAccept(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_dice_accept().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDiceReject(JNIEnv* env, jclass) {
    return env->NewStringUTF(data_op_dice_reject().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDiscardItem(JNIEnv* env, jclass, jint bag, jint slot) {
    return env->NewStringUTF(data_op_discard_item(static_cast<int>(bag), static_cast<int>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSellItem(JNIEnv* env, jclass, jint bag, jint slot) {
    return env->NewStringUTF(data_op_sell_item(static_cast<int>(bag), static_cast<int>(slot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpMoveItem(JNIEnv* env, jclass, jint bag, jint slot, jint count, jint toBag, jint toSlot) {
    return env->NewStringUTF(data_op_move_item(static_cast<int>(bag), static_cast<int>(slot),
        static_cast<int>(count), static_cast<int>(toBag), static_cast<int>(toSlot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpIncludeParty(JNIEnv* env, jclass, jint mercSlot) {
    return env->NewStringUTF(data_op_include_party(static_cast<int>(mercSlot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpExcludeParty(JNIEnv* env, jclass, jint mercSlot) {
    return env->NewStringUTF(data_op_exclude_party(static_cast<int>(mercSlot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpDischarge(JNIEnv* env, jclass, jint mercSlot) {
    return env->NewStringUTF(data_op_discharge(static_cast<int>(mercSlot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpWithdraw(JNIEnv* env, jclass, jint mercSlot, jint equipSlot) {
    return env->NewStringUTF(data_op_withdraw(static_cast<int>(mercSlot), static_cast<int32_t>(equipSlot)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetHp(JNIEnv* env, jclass, jint role, jint hp) {
    return env->NewStringUTF(data_op_set_hp(static_cast<int>(role), static_cast<int32_t>(hp)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetMp(JNIEnv* env, jclass, jint role, jint mp) {
    return env->NewStringUTF(data_op_set_mp(static_cast<int>(role), static_cast<int32_t>(mp)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpSetAttr(JNIEnv* env, jclass, jint role, jint attrIndex, jint value) {
    return env->NewStringUTF(data_op_set_attr(static_cast<int>(role), static_cast<int32_t>(attrIndex), static_cast<int32_t>(value)).c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_inotia4_export_NativeBridge_nativeOpAddItem(JNIEnv* env, jclass, jint category, jint count) {
    return env->NewStringUTF(data_op_add_item(static_cast<int32_t>(category), static_cast<int32_t>(count)).c_str());
}
