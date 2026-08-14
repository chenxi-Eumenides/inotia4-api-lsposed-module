package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 游戏整体：/api/system/game（api-reference §0.2）。复合 + info/frame + 顶层 snapshot。
 */
@RestController
@RequestMapping("/api/system/game")
class GameController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(ApiServices.info::game)

    @GetMapping("/api/system/snapshot")
    fun snapshot(): String = ControllerGuard.guard(ApiServices.info::gameSnapshot)

    @GetMapping("/info")
    fun info(): String = ControllerGuard.guard(ApiServices.info::gameInfo)

    @GetMapping("/frame")
    fun frame(): String = ControllerGuard.guard(ApiServices.info::gameFrame)
}
