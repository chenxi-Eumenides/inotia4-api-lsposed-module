package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RestController

@RestController
class MercenaryController {

    @GetMapping("/api/info/mercenary")
    fun composite(): String = ControllerGuard.guard(ApiServices.info::mercenary)

    @GetMapping("/api/info/mercenary/list")
    fun list(): String = ControllerGuard.guard(ApiServices.info::mercenaryList)

    @GetMapping("/api/info/mercenary/{slot}")
    fun slot(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.mercenarySlot(slot) }
}
