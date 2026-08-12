package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 当前地图信息：/api/world/map（api-reference world 域）。复合 + 简单子端点。
 */
@RestController
@RequestMapping("/api/world/map")
class CurrentMapController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(ApiServices.info::currentMap)

    @GetMapping("/id")
    fun id(): String = ControllerGuard.guard(ApiServices.info::currentMapId)

    @GetMapping("/tile")
    fun tile(): String = ControllerGuard.guard(ApiServices.info::currentMapTile)

    @GetMapping("/exits")
    fun exits(): String = ControllerGuard.guard(ApiServices.info::currentMapExits)

    @GetMapping("/units")
    fun units(): String = ControllerGuard.guard(ApiServices.info::currentMapUnits)

    @GetMapping("/enemies")
    fun enemies(): String = ControllerGuard.guard(ApiServices.info::currentMapEnemies)

    @GetMapping("/interactives")
    fun interactives(): String = ControllerGuard.guard(ApiServices.info::currentMapInteractives)

    @GetMapping("/drops")
    fun drops(): String = ControllerGuard.guard(ApiServices.info::currentMapDrops)

    @GetMapping("/tiles")
    fun tiles(): String = ControllerGuard.guard(ApiServices.info::currentMapTiles)

    @GetMapping("/distance")
    fun distance(@RequestParam("tx") tx: Int, @RequestParam("ty") ty: Int): String =
        ControllerGuard.guard { ApiServices.info.currentMapDistance(tx, ty) }
}
