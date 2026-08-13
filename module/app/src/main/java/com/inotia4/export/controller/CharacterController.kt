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

    @PostMapping("/api/character/grow/skill")
    fun skill(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val actionId = o.optInt("action_id", -1)
        if (actionId < 0) return "{\"ok\":false,\"error\":\"action_id required\"}"
        val level = o.optInt("level", 1)
        return ControllerGuard.guard { ApiServices.action.learnSkill(0, actionId, level) }
    }

    @PostMapping("/api/character/grow/{role}/stat")
    fun stat(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val attr = o.optInt("attr", -1)
        if (attr < 0) return "{\"ok\":false,\"error\":\"attr required (0=力量 1=敏捷 2=体力 3=智力 4=精力)\"}"
        return ControllerGuard.guard { ApiServices.action.addStat(role, attr) }
    }

    @PostMapping("/api/character/grow/{role}/stat-reset")
    fun statReset(@PathVariable("role") role: Int): String =
        ControllerGuard.guard { ApiServices.action.statReset(role) }

    @PostMapping("/api/character/grow/{role}/skill-reset")
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

    @PostMapping("/api/op/character/{role}/level")
    fun opSetLevel(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val level = o.optInt("level", -1)
        if (level < 1) return "{\"ok\":false,\"error\":\"level required\"}"
        return ControllerGuard.guard { NativeBridge.nativeOpSetLevel(role, level) }
    }

    @PostMapping("/api/op/character/{role}/set_attr")
    fun opSetAttr(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        // 批量设置基础属性（骰子 SetStatBase 路径）：{"stats": {"力量":10,"敏捷":7}} 或 {"stats":{"0":10,"3":7}}，可只传部分
        val stats = o.optJSONObject("stats") ?: return "{\"ok\":false,\"error\":\"stats required (object: 属性名/索引 → 值)\"}"
        val mainNames = listOf("力量", "敏捷", "体力", "智力", "精力")
        val pairs = mutableListOf<Pair<Int, Int>>()
        val keys = stats.keys()
        while (keys.hasNext()) {
            val k = keys.next()
            val idx = when (k) {
                "0", "1", "2", "3", "4" -> k.toInt()
                else -> mainNames.indexOf(k)
            }
            if (idx < 0) return "{\"ok\":false,\"error\":\"bad attr: $k (0-4 或 力量/敏捷/体力/智力/精力)\"}"
            val v = stats.optInt(k, -1)
            if (v < 0 || v > 255) return "{\"ok\":false,\"error\":\"bad value for $k (0-255)\"}"
            pairs.add(idx to v)
        }
        if (pairs.isEmpty()) return "{\"ok\":false,\"error\":\"stats empty\"}"
        val sb = StringBuilder("[")
        for ((idx, v) in pairs) {
            val r = NativeBridge.nativeOpSetAttr(role, idx, v)
            if (r.contains("\"ok\":false")) return "{\"ok\":false,\"error\":\"set attr $idx failed\"}"
            if (sb.length > 1) sb.append(',')
            sb.append("{\"attr\":$idx,\"value\":$v}")
        }
        sb.append(']')
        return "{\"ok\":true,\"set\":$sb}"
    }

    @PostMapping("/api/op/inventory/add")
    fun opAddItem(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val category = o.optInt("category", -1)
        val count = o.optInt("count", 1)
        if (category < 0) return "{\"ok\":false,\"error\":\"category required\"}"
        return ControllerGuard.guard { NativeBridge.nativeOpAddItem(category, count) }
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
