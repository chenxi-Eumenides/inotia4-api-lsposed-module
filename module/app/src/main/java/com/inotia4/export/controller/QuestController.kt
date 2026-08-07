package com.inotia4.export.controller

import com.inotia4.export.service.InfoService
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 任务：/api/info/quest（api-spec §0.2）。复合 + active/list/list/{id}/completed。
 */
@RestController
@RequestMapping("/api/info/quest")
class QuestController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(InfoService::quest)

    @GetMapping("/active")
    fun active(): String = ControllerGuard.guard(InfoService::questActive)

    @GetMapping("/list")
    fun list(): String = ControllerGuard.guard(InfoService::questList)

    @GetMapping("/list/{id}")
    fun listId(@PathVariable("id") id: Int): String =
        ControllerGuard.guard { InfoService.questListId(id) }

    @GetMapping("/completed")
    fun completed(): String = ControllerGuard.guard(InfoService::questCompleted)
}
