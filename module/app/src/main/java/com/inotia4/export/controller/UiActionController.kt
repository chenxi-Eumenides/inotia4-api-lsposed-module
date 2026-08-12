package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class UiActionController {

    @PostMapping("/api/ui/dialog/ok")
    fun dialogOk(): String = ControllerGuard.guard { ApiServices.action.dialogOk() }

    @PostMapping("/api/ui/dialog/cancel")
    fun dialogCancel(): String = ControllerGuard.guard { ApiServices.action.dialogCancel() }

    @PostMapping("/api/ui/main-menu")
    fun mainMenu(): String = ControllerGuard.guard { ApiServices.action.mainMenu() }

    @PostMapping("/api/ui/panel/close")
    fun panelClose(): String = ControllerGuard.guard { ApiServices.action.panelClose() }

    @PostMapping("/api/ui/panel/open")
    fun panelOpen(@RequestBody body: String): String {
        val panel = try {
            JSONObject(body).optString("panel", "")
        } catch (e: Exception) {
            ""
        }
        if (panel.isBlank()) return """{"ok":false,"error":"panel required"}"""
        return ControllerGuard.guard { ApiServices.action.panelOpen(panel) }
    }
}
