package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// 统一对话端点（v0.4.27，重构自 v0.4.13 npc 三件套）：
//   interact（开始交互）→ content（获取对话内容+选项）→ select（选择选项）
// 一套 API 覆盖：剧情对话（AVG）/ NPC 对话 / 任务框 / 弹窗
// controller 只做路由 + 参数解析，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束）
@RestController
class NpcController {

    @PostMapping("/api/ui/start_interact")
    fun interact(): String = ControllerGuard.guard { ApiServices.action.npcInteract() }

    @PostMapping("/api/ui/dialog/select")
    fun select(@RequestBody body: String): String {
        val o = try {
            JSONObject(body)
        } catch (e: Exception) {
            return "{\"ok\":false,\"error\":\"bad body\"}"
        }
        val action = o.optString("action", "")
        val index = o.optInt("index", -1)
        if (action.isEmpty() && index < 0) return "{\"ok\":false,\"error\":\"action or index required\"}"
        return ControllerGuard.guard { ApiServices.action.dialogSelect(action, index) }
    }
}
