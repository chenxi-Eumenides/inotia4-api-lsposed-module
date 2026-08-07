package com.inotia4.export.controller

import com.inotia4.export.service.InfoService
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 当前地图信息：/api/info/current-map（api-spec §0.2）。复合 + 简单子端点。
 */
@RestController
@RequestMapping("/api/info/current-map")
class CurrentMapController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(InfoService::currentMap)

    @GetMapping("/id")
    fun id(): String = ControllerGuard.guard(InfoService::currentMapId)

    @GetMapping("/tile")
    fun tile(): String = ControllerGuard.guard(InfoService::currentMapTile)

    @GetMapping("/units")
    fun units(): String = ControllerGuard.guard(InfoService::currentMapUnits)

    @GetMapping("/enemies")
    fun enemies(): String = ControllerGuard.guard(InfoService::currentMapEnemies)

    @GetMapping("/interactives")
    fun interactives(): String = ControllerGuard.guard(InfoService::currentMapInteractives)

    @GetMapping("/drops")
    fun drops(): String = ControllerGuard.guard(InfoService::currentMapDrops)
}
