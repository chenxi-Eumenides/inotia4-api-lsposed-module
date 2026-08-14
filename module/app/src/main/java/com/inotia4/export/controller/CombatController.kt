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

    @PostMapping("/api/character/combat/{role}/set_auto_attack")
    fun autoAttack(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        if (!o.has("on")) return "{\"ok\":false,\"error\":\"on required\"}"
        return ControllerGuard.guard { ApiServices.action.autoAttack(role, o.optBoolean("on")) }
    }

    // ⏳ 占位：文档请求 {action_id, mode:off/normal/high}（单技能 AI 档位），native 仅支持全局布尔开关，
    // 单技能档位编码（技能链表节点 +0x07）缺失前置探索，暂返回 not implemented
    @PostMapping("/api/character/combat/{role}/set_skill_usage")
    fun skillUsage(@PathVariable("role") role: Int, @RequestBody body: String): String =
        """{"ok":false,"error":"not implemented"}"""

    @PostMapping("/api/character/combat/switch_player")
    fun switchPlayer(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val slot = o.optInt("slot", -1)
        if (slot < 0 || slot > 2) return "{\"ok\":false,\"error\":\"slot required (0-2)\"}"
        return ControllerGuard.guard { ApiServices.action.switchPlayer(slot) }
    }

    @PostMapping("/api/character/combat/{role}/cast_skill")
    fun cast(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val actionId = o.optInt("action_id", -1)
        if (actionId < 0) return "{\"ok\":false,\"error\":\"action_id required\"}"
        return ControllerGuard.guard { ApiServices.action.cast(role, actionId) }
    }

    @PostMapping("/api/character/combat/{role}/attack_target")
    fun attack(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val targetSlot = o.optInt("target_slot", -1)
        if (targetSlot < 0) return "{\"ok\":false,\"error\":\"target_slot required\"}"
        return ControllerGuard.guard { ApiServices.action.attack(role, targetSlot) }
    }

    @PostMapping("/api/character/combat/{role}/stop_combat")
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
