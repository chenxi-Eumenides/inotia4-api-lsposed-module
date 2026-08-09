package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class SaveController {

    @GetMapping("/api/info/save/slots")
    fun slots(): String = ControllerGuard.guard { ApiServices.info.saveSlots() }

    @PostMapping("/api/action/save/save")
    fun save(): String = ControllerGuard.guard { ApiServices.action.save() }

    @PostMapping("/api/action/save/enter-slot")
    fun enterSlot(@RequestBody body: String): String {
        val slot = try {
            JSONObject(body).optInt("slot", -1)
        } catch (e: Exception) {
            -1
        }
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required (0-2)\"}"
        return ControllerGuard.guard { ApiServices.action.enterSlot(slot) }
    }

    @PostMapping("/api/action/save/load")
    fun load(): String = """{"ok":false,"error":"not implemented"}"""
}
