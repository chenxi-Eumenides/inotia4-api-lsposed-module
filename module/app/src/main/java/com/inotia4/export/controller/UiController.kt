package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 界面状态：/api/ui（api-reference §0.2）。复合 + screen/panel/dialog 子端点。
 */
@RestController
@RequestMapping("/api/ui")
class UiController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(ApiServices.info::ui)

    @GetMapping("/screen")
    fun screen(): String = ControllerGuard.guard(ApiServices.info::uiScreen)

    @GetMapping("/panel")
    fun panel(): String = ControllerGuard.guard(ApiServices.info::uiPanel)

    @GetMapping("/dialog")
    fun dialog(): String = ControllerGuard.guard(ApiServices.info::uiDialog)

    @GetMapping("/dialog/active")
    fun dialogActive(): String = ControllerGuard.guard(ApiServices.info::uiDialogActive)

    @GetMapping("/dialog/text")
    fun dialogText(): String = ControllerGuard.guard(ApiServices.info::uiDialogText)

    @GetMapping("/dialog/buttons")
    fun dialogButtons(): String = ControllerGuard.guard(ApiServices.info::uiDialogButtons)

    @GetMapping("/dialog/ok")
    fun dialogOk(): String = ControllerGuard.guard(ApiServices.info::uiDialogOk)

    @GetMapping("/dialog/cancel")
    fun dialogCancel(): String = ControllerGuard.guard(ApiServices.info::uiDialogCancel)
}
