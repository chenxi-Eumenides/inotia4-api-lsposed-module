package com.inotia4.export.controller

import com.inotia4.export.ApiServer
import com.inotia4.export.ModuleConfig
import com.inotia4.export.util.JsonUtil
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 模块配置：GET/POST /api/system/config（api-reference §7.6）。
 * 配置为纯 Kotlin 层能力（不依赖 native），不走 ControllerGuard。
 */
@RestController
class ConfigController {

    @GetMapping("/api/system/config")
    fun get(): String = ModuleConfig.toJson().toString()

    @PostMapping("/api/system/config")
    fun set(@RequestBody body: String): String {
        val json = JsonUtil.parseObj(body) ?: return JsonUtil.BAD_REQUEST
        val oldAddress = ModuleConfig.listenAddress
        val oldPort = ModuleConfig.listenPort
        val err = ModuleConfig.apply(json)
        if (err != null) return """{"ok":false,"error":"$err"}"""
        val restartNeeded = ModuleConfig.listenAddress != oldAddress || ModuleConfig.listenPort != oldPort
        if (restartNeeded) ApiServer.restartDelayed()
        return ModuleConfig.toJson()
            .put("ok", true)
            .put("restart", restartNeeded)
            .toString()
    }
}
