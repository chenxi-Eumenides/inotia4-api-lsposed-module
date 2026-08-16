package com.inotia4.export.controller

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ApiException
import com.inotia4.export.util.ControllerGuard
import com.inotia4.export.util.JsonUtil
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import com.yanzhenjie.andserver.http.StatusCode

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class CharacterController {

    @PostMapping("/api/character/grow/add_skill")
    fun skill(@RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        val actionId = o.optInt("action_id", -1)
        if (actionId < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "action_id required")
        val level = o.optInt("level", 1)
        return ControllerGuard.guard { ApiServices.action.learnSkill(0, actionId, level) }
    }

    @PostMapping("/api/character/grow/{role}/add_stat")
    fun stat(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        // 批量加点：{"attrs":{"strength":1,"agility":2}}，属性名英文或索引 0-4，可只传部分
        val attrs = o.optJSONObject("attrs") ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "attrs required (object: strength/agility/vitality/intelligence/spirit 或 0-4 → 数量)")
        val mainNames = listOf("strength", "agility", "vitality", "intelligence", "spirit")
        val pairs = mutableListOf<Pair<Int, Int>>()
        val keys = attrs.keys()
        while (keys.hasNext()) {
            val k = keys.next()
            val idx = when (k) {
                "0", "1", "2", "3", "4" -> k.toInt()
                else -> mainNames.indexOf(k)
            }
            if (idx < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "bad attr: $k (0-4 或 strength/agility/vitality/intelligence/spirit)")
            val v = attrs.optInt(k, -1)
            if (v <= 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "bad value for $k (must be positive)")
            pairs.add(idx to v)
        }
        if (pairs.isEmpty()) throw ApiException(StatusCode.SC_BAD_REQUEST, "attrs empty")
        return ControllerGuard.guard { ApiServices.action.addStat(role, pairs) }
    }

    @PostMapping("/api/character/grow/reset_stat")
    fun statReset(): String = ControllerGuard.guard { ApiServices.action.statReset(0) }

    @PostMapping("/api/character/grow/reset_skill")
    fun skillReset(): String = ControllerGuard.guard { ApiServices.action.skillReset(0) }

    // ---- OP: 角色属性直写 ----

    @PostMapping("/api/op/character/{role}/hp")
    fun opSetHp(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        val hp = o.optInt("hp", -1)
        if (hp < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "hp required")
        return ControllerGuard.guard { LogFile.op("POST /api/op/character/{role}/hp", "role=$role,hp=$hp") { NativeBridge.nativeOpSetHp(role, hp) } }
    }

    @PostMapping("/api/op/character/{role}/mp")
    fun opSetMp(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        val mp = o.optInt("mp", -1)
        if (mp < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "mp required")
        return ControllerGuard.guard { LogFile.op("POST /api/op/character/{role}/mp", "role=$role,mp=$mp") { NativeBridge.nativeOpSetMp(role, mp) } }
    }

    @PostMapping("/api/op/character/{role}/experience")
    fun opSetExp(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        val exp = o.optLong("exp", -1)
        if (exp < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "exp required")
        return ControllerGuard.guard { LogFile.op("POST /api/op/character/{role}/experience", "role=$role,exp=$exp") { NativeBridge.nativeOpSetExperience(role, exp) } }
    }

    @PostMapping("/api/op/character/{role}/level")
    fun opSetLevel(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        if (!o.has("level")) throw ApiException(StatusCode.SC_BAD_REQUEST, "level required")
        val level = o.optInt("level", 0)
        val force = o.optBoolean("force", false)
        if (!force && (level < 1 || level > 105)) throw ApiException(StatusCode.SC_BAD_REQUEST, "level 1-105 (game max); force=true 跳过限制")
        return ControllerGuard.guard { LogFile.op("POST /api/op/character/{role}/level", "role=$role,level=$level,force=$force") { NativeBridge.nativeOpSetLevel(role, level, force) } }
    }

    @PostMapping("/api/op/character/{role}/set_attr")
    fun opSetAttr(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        // 批量设置基础属性（骰子 SetStatBase 路径）：{"stats": {"strength":10,"agility":7}} 或 {"stats":{"0":10,"3":7}}，可只传部分
        val stats = o.optJSONObject("stats") ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "stats required (object: 属性名/索引 → 值)")
        val mainNames = listOf("strength", "agility", "vitality", "intelligence", "spirit")
        val pairs = mutableListOf<Pair<Int, Int>>()
        val keys = stats.keys()
        while (keys.hasNext()) {
            val k = keys.next()
            val idx = when (k) {
                "0", "1", "2", "3", "4" -> k.toInt()
                else -> mainNames.indexOf(k)
            }
            if (idx < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "bad attr: $k (0-4 或 strength/agility/vitality/intelligence/spirit)")
            val v = stats.optInt(k, -1)
            if (v < 0 || v > 255) throw ApiException(StatusCode.SC_BAD_REQUEST, "bad value for $k (0-255)")
            pairs.add(idx to v)
        }
        if (pairs.isEmpty()) throw ApiException(StatusCode.SC_BAD_REQUEST, "stats empty")
        return ControllerGuard.guard {
            LogFile.op("POST /api/op/character/{role}/set_attr", "role=$role,stats=$pairs") {
                val sb = StringBuilder("[")
                for ((idx, v) in pairs) {
                    val r = NativeBridge.nativeOpSetAttr(role, idx, v)
                    if (r.contains("\"ok\":false")) return@op JsonUtil.err("set attr $idx failed", 500)
                    if (sb.length > 1) sb.append(',')
                    sb.append("{\"attr\":$idx,\"value\":$v}")
                }
                sb.append(']')
                "{\"ok\":true,\"set\":$sb}"
            }
        }
    }

    @PostMapping("/api/op/inventory/add")
    fun opAddItem(@RequestBody body: String): String {
        val o = JsonUtil.parseBody(body) ?: throw ApiException(StatusCode.SC_BAD_REQUEST, "bad request")
        val category = o.optInt("category", -1)
        val count = o.optInt("count", 1)
        if (category < 0) throw ApiException(StatusCode.SC_BAD_REQUEST, "category required")
        return ControllerGuard.guard { LogFile.op("POST /api/op/inventory/add", "category=$category,count=$count") { NativeBridge.nativeOpAddItem(category, count) } }
    }
}
