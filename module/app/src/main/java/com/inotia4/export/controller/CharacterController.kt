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

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        null
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
