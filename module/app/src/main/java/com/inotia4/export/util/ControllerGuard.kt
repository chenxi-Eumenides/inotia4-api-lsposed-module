package com.inotia4.export.util

import com.inotia4.export.NativeBridge

/**
 * controller 公共守卫：native 未就绪时返回 503 语义串（architecture §9.3-9）。
 */
object ControllerGuard {

    fun ready(): Boolean = NativeBridge.ready

    fun guard(f: () -> String): String {
        if (!NativeBridge.ready) return JsonUtil.NOT_READY
        return try {
            f()
        } catch (t: Throwable) {
            JsonUtil.NOT_READY
        }
    }
}
