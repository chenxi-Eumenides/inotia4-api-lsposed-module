package com.inotia4.export

private const val NATIVE_TAG = "Inotia4Export"

object NativeBridge {

    @Volatile
    var ready: Boolean = false
        private set

    init {
        try {
            System.loadLibrary("gamebridge")
        } catch (t: Throwable) {
            android.util.Log.e(NATIVE_TAG, "loadLibrary(gamebridge) failed", t)
        }
    }

    fun init(): Boolean {
        if (ready) return true
        ready = try {
            nativeInit()
        } catch (t: Throwable) {
            android.util.Log.e(NATIVE_TAG, "nativeInit failed", t)
            false
        }
        return ready
    }

    external fun nativeInit(): Boolean
    external fun nativeGetInitReport(): String
    external fun nativeGetActiveQuest(): Int
    external fun nativeGetPlayerJson(): String
    external fun nativeGetPartyJson(): String
    external fun nativeGetInventoryJson(): String
    external fun nativeGetMapJson(): String
    external fun nativeGetUnitsJson(): String
    external fun nativeGetUiJson(): String
    external fun nativeGetSkillsJson(): String
    external fun nativeGetMercenariesJson(): String
    external fun nativeGetPathJson(tx: Int, ty: Int): String

    external fun nativeOpSetMoney(money: Long): String
    external fun nativeOpAddMoney(delta: Long): String
    external fun nativeOpMinusMoney(delta: Long): String
    external fun nativeOpSetExperience(role: Int, exp: Long): String
    external fun nativeOpAddExperience(role: Int, delta: Long): String
    external fun nativeOpSetStatusPoint(role: Int, points: Int): String
    external fun nativeOpSetAutoAttack(role: Int, onoff: Int): String
    external fun nativeOpEquip(role: Int, bag: Int, slot: Int): String
    external fun nativeOpUnequip(role: Int, slot: Int): String
    external fun nativeOpSwitchPlayer(slot: Int): String
    external fun nativeOpTeleport(mapId: Int, x: Int, y: Int): String
    external fun nativeOpRemoveItem(category: Int): String
    external fun nativeOpLearnAction(role: Int, actionId: Int, level: Int): String
    external fun nativeGetEventsJson(): String
}
