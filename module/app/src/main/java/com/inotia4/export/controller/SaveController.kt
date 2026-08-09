package com.inotia4.export.controller

import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RestController

// controller: 路由层，业务走 ApiServices。save/load 依赖 SAVE 链逆向（P1 待做），当前占位
@RestController
class SaveController {

    @PostMapping("/api/action/save/save")
    fun save(): String = """{"ok":false,"error":"not implemented"}"""

    @PostMapping("/api/action/save/load")
    fun load(): String = """{"ok":false,"error":"not implemented"}"""
}
