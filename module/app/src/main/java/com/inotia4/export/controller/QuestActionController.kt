package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class QuestActionController {

    @PostMapping("/api/quest/quit")
    fun quit(@RequestBody body: String): String {
        val o = try {
            JSONObject(body)
        } catch (e: Exception) {
            return "{\"ok\":false,\"error\":\"bad body\"}"
        }
        val questId = o.optInt("quest_id", -1)
        if (questId < 0) return "{\"ok\":false,\"error\":\"questId required\"}"
        return ControllerGuard.guard { ApiServices.action.questQuit(questId) }
    }
}
