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
}
