package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class PartyActionController {

    @PostMapping("/api/action/party/include")
    fun include(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mercSlot = o.optInt("mercenarySlot", -1)
        if (mercSlot < 0) return "{\"ok\":false,\"error\":\"mercenarySlot required\"}"
        return ControllerGuard.guard { ApiServices.action.includeParty(mercSlot) }
    }

    @PostMapping("/api/action/party/exclude")
    fun exclude(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mercSlot = o.optInt("mercenarySlot", -1)
        if (mercSlot < 0) return "{\"ok\":false,\"error\":\"mercenarySlot required\"}"
        return ControllerGuard.guard { ApiServices.action.excludeParty(mercSlot) }
    }

    @PostMapping("/api/action/party/discharge")
    fun discharge(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mercSlot = o.optInt("mercenarySlot", -1)
        if (mercSlot < 0) return "{\"ok\":false,\"error\":\"mercenarySlot required\"}"
        return ControllerGuard.guard { ApiServices.action.discharge(mercSlot) }
    }

    @PostMapping("/api/action/party/withdraw")
    fun withdraw(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mercSlot = o.optInt("mercenarySlot", -1)
        val equipSlot = o.optInt("equipSlot", -1)
        if (mercSlot < 0 || equipSlot < 0) return "{\"ok\":false,\"error\":\"mercenarySlot/equipSlot required\"}"
        return ControllerGuard.guard { ApiServices.action.withdraw(mercSlot, equipSlot) }
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
