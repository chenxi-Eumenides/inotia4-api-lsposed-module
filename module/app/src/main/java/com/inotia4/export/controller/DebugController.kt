package com.inotia4.export.controller

import com.inotia4.export.NativeBridge
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 调试端点：/api/debug/ui、/api/debug/path（architecture §9.1 登记）。
 * 路由统一全路径写法（无类级 RequestMapping，见 architecture §3）。
 */
@RestController
class DebugController {

    @GetMapping("/api/debug/ui")
    fun ui(): String = NativeBridge.nativeGetDebugUiJson()

    @GetMapping("/api/debug/path")
    fun path(@RequestParam("tx") tx: Int, @RequestParam("ty") ty: Int): String =
        NativeBridge.nativeDebugPathJson(tx, ty)
}
