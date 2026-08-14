package com.inotia4.export.controller

import com.inotia4.export.ApiServer
import com.inotia4.export.ModuleConfig
import com.inotia4.export.NativeBridge
import com.inotia4.export.util.JsonUtil
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 模块配置：GET /api/config/list + POST /api/config/set（api-reference §7.6）。
 * listenAddress/listenPort 为纯 Kotlin 层能力；stackLimitIncrease/jewelBatchMix
 * 变化时通知 native 生效（堆叠 patch/迁移、批量合成按钮注入）。
 */
@RestController
class ConfigController {

    @GetMapping("/api/config/list")
    fun list(): String = ModuleConfig.toJson().toString()

    @PostMapping("/api/config/set")
    fun set(@RequestBody body: String): String {
        val json = JsonUtil.parseObj(body) ?: return JsonUtil.BAD_REQUEST
        val oldAddress = ModuleConfig.listenAddress
        val oldPort = ModuleConfig.listenPort
        val oldStack = ModuleConfig.stackLimitIncrease
        val oldJewel = ModuleConfig.jewelBatchMix
        val err = ModuleConfig.apply(json)
        if (err != null) return """{"ok":false,"error":"$err"}"""
        val restartNeeded = ModuleConfig.listenAddress != oldAddress || ModuleConfig.listenPort != oldPort
        if (restartNeeded) ApiServer.restartDelayed()
        if (NativeBridge.ready) {
            if (ModuleConfig.stackLimitIncrease != oldStack) {
                NativeBridge.nativeSetStackLimitEnabled(ModuleConfig.stackLimitIncrease)
            }
            if (ModuleConfig.jewelBatchMix != oldJewel) {
                NativeBridge.nativeSetJewelBatchMix(ModuleConfig.jewelBatchMix)
            }
        }
        return ModuleConfig.toJson()
            .put("ok", true)
            .put("restart", restartNeeded)
            .toString()
    }
}
