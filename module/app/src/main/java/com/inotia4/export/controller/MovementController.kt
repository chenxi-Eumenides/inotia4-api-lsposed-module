package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class MovementController {

    @PostMapping("/api/action/movement/move")
    fun move(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val x = o.optInt("x", -1)
        val y = o.optInt("y", -1)
        if (x < 0 || y < 0) return "{\"ok\":false,\"error\":\"x/y required\"}"
        return ControllerGuard.guard { ApiServices.action.move(x, y) }
    }

    @PostMapping("/api/action/movement/walk")
    fun walk(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val direction = o.optInt("direction", -1)
        if (direction !in 0..3) return "{\"ok\":false,\"error\":\"direction 0-3 required\"}"
        return ControllerGuard.guard { ApiServices.action.walk(direction) }
    }

    // v0.4.29 自研 BFS 寻路（替代游戏 CHAR_SearchPath：支持绕远路/可达性/最近可达点）
    @PostMapping("/api/action/movement/path")
    fun path(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val tx = o.optInt("tx", -1)
        val ty = o.optInt("ty", -1)
        if (tx < 0 || ty < 0) return "{\"ok\":false,\"error\":\"tx/ty required\"}"
        return ControllerGuard.guard { ApiServices.action.getPath(tx, ty) }
    }

    // 停止移动（v0.4.26 合并 walk_stop/move_cancel：两者语义等价=停后台线程+清 PATHLIST）
    @PostMapping("/api/action/movement/stop")
    fun stop(): String = ControllerGuard.guard { ApiServices.action.walkStop() }

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        null
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
