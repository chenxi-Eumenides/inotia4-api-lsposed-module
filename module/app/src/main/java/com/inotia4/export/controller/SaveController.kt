package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RestController

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class SaveController {

    @PostMapping("/api/action/save/save")
    fun save(): String = ControllerGuard.guard { ApiServices.action.save() }

    @PostMapping("/api/action/save/load")
    fun load(): String = """{"ok":false,"error":"not implemented"}"""
}
