package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// NPC 交互端点（v0.4.13）：interact/dialog-next/dialog-select（POST）+ dialog/options（GET）
// controller 只做路由 + 参数解析，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束）
@RestController
class NpcController {

    @PostMapping("/api/action/npc/interact")
    fun interact(): String = ControllerGuard.guard { ApiServices.action.npcInteract() }

    @PostMapping("/api/action/npc/dialog/next")
    fun dialogNext(): String = ControllerGuard.guard { ApiServices.action.npcDialogNext() }

    @PostMapping("/api/action/npc/dialog/select")
    fun dialogSelect(@RequestBody body: String): String {
        val o = try {
            JSONObject(body)
        } catch (e: Exception) {
            return "{\"ok\":false,\"error\":\"bad body\"}"
        }
        val index = o.optInt("index", -1)
        if (index < 0) return "{\"ok\":false,\"error\":\"index required\"}"
        return ControllerGuard.guard { ApiServices.action.npcDialogSelect(index) }
    }

    @GetMapping("/api/info/npc/dialog/options")
    fun dialogOptions(): String = ControllerGuard.guard { ApiServices.info.npcDialogOptions() }
}
