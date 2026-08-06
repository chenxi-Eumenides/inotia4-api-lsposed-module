package com.inotia4.export.controller

import com.inotia4.export.NativeBridge
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

@RestController
@RequestMapping("/api/debug")
class DebugController {

    @GetMapping("/ui")
    fun ui(): String = NativeBridge.nativeGetDebugUiJson()
}
