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
    external fun nativeGetBaseAddr(): Long
    external fun nativeGetActiveQuest(): Int
    external fun nativeGetPlayerJson(): String
    external fun nativeGetPartyJson(): String
    external fun nativeGetInventoryJson(): String
    external fun nativeGetMapJson(): String
    external fun nativeGetUnitsJson(): String
    external fun nativeGetGamestateJson(): String
    external fun nativeGetDebugUiJson(): String
    external fun nativeGetSnapshotJson(): String
    external fun nativeGetSkillsJson(): String
    external fun nativeGetMercenariesJson(): String
    external fun nativeGetPathJson(tx: Int, ty: Int): String

    external fun nativeOpSetMoney(money: Long): String
    external fun nativeOpAddMoney(delta: Long): String
    external fun nativeOpMinusMoney(delta: Long): String
    external fun nativeOpSetExperience(role: Int, exp: Long): String
    external fun nativeOpAddExperience(role: Int, delta: Long): String
    external fun nativeOpSetStatusPoint(role: Int, points: Int): String
    external fun nativeOpAddStat(role: Int, attr: Int): String
    external fun nativeOpStatReset(role: Int): String
    external fun nativeOpJewel(role: Int, bag: Int, slot: Int, equipSlot: Int): String
    external fun nativeOpSetAutoAttack(role: Int, onoff: Int): String
    external fun nativeOpEquip(role: Int, bag: Int, slot: Int): String
    external fun nativeOpUnequip(role: Int, slot: Int): String
    external fun nativeOpSwitchPlayer(slot: Int): String
    external fun nativeOpTeleport(mapId: Int, x: Int, y: Int): String
    external fun nativeOpRemoveItem(category: Int): String
    external fun nativeOpLearnAction(role: Int, actionId: Int, level: Int): String
    external fun nativeGetEventsJson(): String

    external fun nativeOpMove(x: Int, y: Int): String
    external fun nativeOpWalk(direction: Int): String
    external fun nativeOpWalkStop(): String
    external fun nativeOpMoveCancel(): String
    external fun nativeOpAttack(role: Int, targetSlot: Int): String
    external fun nativeOpStopCombat(role: Int): String
    external fun nativeOpUseItem(bag: Int, slot: Int): String
    external fun nativeOpDiscardItem(bag: Int, slot: Int): String
    external fun nativeOpSellItem(bag: Int, slot: Int): String
    external fun nativeOpMoveItem(bag: Int, slot: Int, count: Int, toBag: Int, toSlot: Int): String
    external fun nativeOpIncludeParty(mercSlot: Int): String
    external fun nativeOpExcludeParty(mercSlot: Int): String
    external fun nativeOpDischarge(mercSlot: Int): String
    external fun nativeOpWithdraw(mercSlot: Int, equipSlot: Int): String
    external fun nativeOpPartySwap(a: Int, b: Int): String
    external fun nativeOpDialogOk(): String
    external fun nativeOpDialogCancel(): String
}
