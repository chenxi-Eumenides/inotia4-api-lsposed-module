package com.inotia4.export.controller

import com.inotia4.export.NativeBridge
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

@RestController
@RequestMapping("/api")
class PlayerController {

    @GetMapping("/player")
    fun player(): String = NativeBridge.nativeGetPlayerJson()

    @GetMapping("/player/party")
    fun party(): String = NativeBridge.nativeGetPartyJson()

    @GetMapping("/inventory")
    fun inventory(): String = NativeBridge.nativeGetInventoryJson()

    @GetMapping("/map")
    fun map(): String = NativeBridge.nativeGetMapJson()

    @GetMapping("/quest")
    fun quest(): String = """{"activeQuest":${NativeBridge.nativeGetActiveQuest()}}"""

    @GetMapping("/units")
    fun units(): String = NativeBridge.nativeGetUnitsJson()

    @GetMapping("/ui")
    fun ui(): String = NativeBridge.nativeGetUiJson()

    @GetMapping("/player/skills")
    fun skills(): String = NativeBridge.nativeGetSkillsJson()
}
