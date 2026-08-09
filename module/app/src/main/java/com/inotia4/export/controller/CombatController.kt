package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class CombatController {

    @PostMapping("/api/action/combat/{role}/config/auto-attack")
    fun autoAttack(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        return ControllerGuard.guard { ApiServices.action.autoAttack(role, o.optBoolean("on")) }
    }

    @PostMapping("/api/action/combat/{role}/config/skill-usage")
    fun skillUsage(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        return ControllerGuard.guard { ApiServices.action.skillUsage(role, o.optBoolean("on")) }
    }

    @PostMapping("/api/action/combat/{role}/switch")
    fun switchPlayer(@PathVariable("role") role: Int): String =
        ControllerGuard.guard { ApiServices.action.switchPlayer(role) }

    @PostMapping("/api/action/combat/{role}/attack")
    fun attack(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val targetSlot = o.optInt("targetSlot", -1)
        if (targetSlot < 0) return "{\"ok\":false,\"error\":\"targetSlot required\"}"
        return ControllerGuard.guard { ApiServices.action.attack(role, targetSlot) }
    }

    @PostMapping("/api/action/combat/{role}/stop")
    fun stop(@PathVariable("role") role: Int): String =
        ControllerGuard.guard { ApiServices.action.stopCombat(role) }

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        null
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
