package com.inotia4.export.controller

import com.inotia4.export.NativeBridge
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 调试端点：/api/debug/ui（architecture §9.1 登记）。
 * 路由统一全路径写法（无类级 RequestMapping，见 architecture §3）。
 */
@RestController
class DebugController {

    @GetMapping("/api/debug/ui")
    fun ui(): String = NativeBridge.nativeGetDebugUiJson()
}
