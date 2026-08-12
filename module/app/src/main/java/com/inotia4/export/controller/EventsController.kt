package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 事件流：/api/system/events（api-reference §0.2）。轮询差异检测，since 参数预留。
 */
@RestController
@RequestMapping("/api/system/events")
class EventsController {

    @GetMapping("/")
    fun events(@RequestParam("since", required = false) since: Long?): String =
        ControllerGuard.guard { ApiServices.info.events(since) }
}
