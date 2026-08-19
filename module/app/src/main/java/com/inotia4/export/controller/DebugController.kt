package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 调试端点：/api/debug/ui、/api/debug/path、/api/debug/exp（architecture §9.1 登记）。
 * 路由统一全路径写法（无类级 RequestMapping，见 architecture §3）。
 * v0.5.46 收边：不再裸调 NativeBridge，经 InfoApiService（ControllerGuard 兜底 not ready/500）。
 * ui-exp v0.6.7：实验端点触发 5 种自定义 UI 方式（docs/system/ui.md §6）。
 */
@RestController
class DebugController {

    @GetMapping("/api/debug/ui")
    fun ui(): String = ControllerGuard.guard { ApiServices.info.debugUi() }

    @GetMapping("/api/debug/path")
    fun path(@RequestParam("tx") tx: Int, @RequestParam("ty") ty: Int): String =
        ControllerGuard.guard { ApiServices.info.debugPath(tx, ty) }

    @GetMapping("/api/debug/exp/status")
    fun expStatus(): String = ControllerGuard.guard { ApiServices.info.expStatus() }

    @PostMapping("/api/debug/exp/1")
    fun exp1(): String = ControllerGuard.guard { ApiServices.info.exp1BtnBehavior() }

    @PostMapping("/api/debug/exp/2")
    fun exp2(): String = ControllerGuard.guard { ApiServices.info.exp2AddControl() }

    @PostMapping("/api/debug/exp/3")
    fun exp3(@RequestBody body: String): String = ControllerGuard.guard { ApiServices.info.exp3CustomDialog(body) }

    @PostMapping("/api/debug/exp/4")
    fun exp4(): String = ControllerGuard.guard { ApiServices.info.exp4TextAppearance() }

    @PostMapping("/api/debug/exp/5")
    fun exp5(): String = ControllerGuard.guard { ApiServices.info.exp5NewPanel() }

    @PostMapping("/api/debug/exp/restore")
    fun expRestore(): String = ControllerGuard.guard { ApiServices.info.expRestoreAll() }

    @PostMapping("/api/debug/settings-ui/inject")
    fun settingsUiInject(): String = ControllerGuard.guard { ApiServices.info.settingsUiInject() }

    @GetMapping("/api/debug/settings-ui/status")
    fun settingsUiStatus(): String = ControllerGuard.guard { ApiServices.info.settingsUiStatus() }

    @PostMapping("/api/debug/settings-ui/restore")
    fun settingsUiRestore(): String = ControllerGuard.guard { ApiServices.info.settingsUiRestore() }

    @PostMapping("/api/debug/settings-ui/open-option")
    fun settingsUiOpenOption(): String = ControllerGuard.guard { ApiServices.info.settingsUiOpenOption() }

    @PostMapping("/api/debug/settings-ui/open-panel")
    fun settingsUiOpenPanel(): String = ControllerGuard.guard { ApiServices.info.settingsUiOpenPanel() }
}