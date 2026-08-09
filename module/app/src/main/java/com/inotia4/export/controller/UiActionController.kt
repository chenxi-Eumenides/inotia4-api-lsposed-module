package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RestController

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class UiActionController {

    @PostMapping("/api/action/ui/dialog/ok")
    fun dialogOk(): String = ControllerGuard.guard { ApiServices.action.dialogOk() }

    @PostMapping("/api/action/ui/dialog/cancel")
    fun dialogCancel(): String = ControllerGuard.guard { ApiServices.action.dialogCancel() }

    @PostMapping("/api/action/ui/main-menu")
    fun mainMenu(): String = ControllerGuard.guard { ApiServices.action.mainMenu() }
}
