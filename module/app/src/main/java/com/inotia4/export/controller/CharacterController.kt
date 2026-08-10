package com.inotia4.export.controller

import com.inotia4.export.NativeBridge
import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class CharacterController {

    @PostMapping("/api/action/character/skill")
    fun skill(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val actionId = o.optInt("actionId", -1)
        if (actionId < 0) return "{\"ok\":false,\"error\":\"actionId required\"}"
        val level = o.optInt("level", 1)
        return ControllerGuard.guard { ApiServices.action.learnSkill(0, actionId, level) }
    }

    @PostMapping("/api/action/character/{role}/stat")
    fun stat(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val attr = o.optInt("attr", -1)
        if (attr < 0) return "{\"ok\":false,\"error\":\"attr required (0=力量 1=敏捷 2=体力 3=智力 4=精力)\"}"
        return ControllerGuard.guard { ApiServices.action.addStat(role, attr) }
    }

    @PostMapping("/api/action/character/{role}/stat-reset")
    fun statReset(@PathVariable("role") role: Int): String =
        ControllerGuard.guard { ApiServices.action.statReset(role) }

    @PostMapping("/api/action/character/{role}/skill-reset")
    fun skillReset(@PathVariable("role") role: Int): String =
        ControllerGuard.guard { ApiServices.action.skillReset(role) }

    // ---- OP: 角色属性直写 ----

    @PostMapping("/api/op/character/{role}/hp")
    fun opSetHp(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val hp = o.optInt("hp", -1)
        if (hp < 0) return "{\"ok\":false,\"error\":\"hp required\"}"
        return ControllerGuard.guard { NativeBridge.nativeOpSetHp(role, hp) }
    }

    @PostMapping("/api/op/character/{role}/mp")
    fun opSetMp(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mp = o.optInt("mp", -1)
        if (mp < 0) return "{\"ok\":false,\"error\":\"mp required\"}"
        return ControllerGuard.guard { NativeBridge.nativeOpSetMp(role, mp) }
    }

    @PostMapping("/api/op/character/{role}/experience")
    fun opSetExp(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val exp = o.optLong("exp", -1)
        if (exp < 0) return "{\"ok\":false,\"error\":\"exp required\"}"
        return ControllerGuard.guard { NativeBridge.nativeOpSetExperience(role, exp) }
    }

    @PostMapping("/api/op/character/{role}/attr/{index}")
    fun opSetAttr(@PathVariable("role") role: Int, @PathVariable("index") index: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val value = o.optInt("value", -1)
        if (value < 0) return "{\"ok\":false,\"error\":\"value required\"}"
        if (index < 0 || index > 4) return "{\"ok\":false,\"error\":\"attr index 0-4 (力/敏/体/智/精)\"}"
        return ControllerGuard.guard { NativeBridge.nativeOpSetAttr(role, index, value) }
    }

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        null
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
