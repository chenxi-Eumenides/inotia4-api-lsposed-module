package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 服务健康：/api/health（api-reference §0.5）。返回服务运行状态与版本信息。
 */
@RestController
@RequestMapping("/api/health")
class HealthController {

    @GetMapping("/")
    fun health(): String = ControllerGuard.guard(ApiServices.info::health)
}
