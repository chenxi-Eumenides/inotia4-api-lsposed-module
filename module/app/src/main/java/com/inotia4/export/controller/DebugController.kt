package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 调试端点：/api/debug/ui、/api/debug/path（architecture §9.1 登记）。
 * 路由统一全路径写法（无类级 RequestMapping，见 architecture §3）。
 * v0.5.46 收边：不再裸调 NativeBridge，经 InfoApiService（ControllerGuard 兜底 not ready/500）。
 */
@RestController
class DebugController {

    @GetMapping("/api/debug/ui")
    fun ui(): String = ControllerGuard.guard { ApiServices.info.debugUi() }

    @GetMapping("/api/debug/path")
    fun path(@RequestParam("tx") tx: Int, @RequestParam("ty") ty: Int): String =
        ControllerGuard.guard { ApiServices.info.debugPath(tx, ty) }
}
