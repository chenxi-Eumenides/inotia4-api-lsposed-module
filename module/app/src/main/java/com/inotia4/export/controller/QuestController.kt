package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 任务：/api/quest（api-reference §0.2）。复合 + active/list/list/{id}/completed。
 */
@RestController
@RequestMapping("/api/quest")
class QuestController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(ApiServices.info::quest)

    @GetMapping("/active")
    fun active(): String = ControllerGuard.guard(ApiServices.info::questActive)

    @GetMapping("/list")
    fun list(): String = ControllerGuard.guard(ApiServices.info::questList)

    @GetMapping("/list/{id}")
    fun listId(@PathVariable("id") id: Int): String =
        ControllerGuard.guard { ApiServices.info.questListId(id) }

    @GetMapping("/completed")
    fun completed(): String = ControllerGuard.guard(ApiServices.info::questCompleted)
}
