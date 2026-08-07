package com.inotia4.export.controller

import com.inotia4.export.service.InfoService
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 游戏整体：/api/info/game（api-spec §0.2）。复合 + snapshot/info。
 */
@RestController
@RequestMapping("/api/info/game")
class GameController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(InfoService::game)

    @GetMapping("/snapshot")
    fun snapshot(): String = ControllerGuard.guard(InfoService::gameSnapshot)

    @GetMapping("/info")
    fun info(): String = ControllerGuard.guard(InfoService::gameInfo)
}
