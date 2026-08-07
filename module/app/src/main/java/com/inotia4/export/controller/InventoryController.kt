package com.inotia4.export.controller

import com.inotia4.export.service.InfoService
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 背包：/api/info/inventory（api-spec §0.2）。复合 + money/items/bag/{i}/info + bag/{i}/{slot}。
 */
@RestController
@RequestMapping("/api/info/inventory")
class InventoryController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(InfoService::inventory)

    @GetMapping("/money")
    fun money(): String = ControllerGuard.guard(InfoService::inventoryMoney)

    @GetMapping("/items")
    fun items(): String = ControllerGuard.guard(InfoService::inventoryItems)

    @GetMapping("/bag/{bag}/info")
    fun bagInfo(@PathVariable("bag") bag: Int): String =
        ControllerGuard.guard { InfoService.bagInfo(bag) }

    @GetMapping("/bag/{bag}/{slot}")
    fun bagSlot(@PathVariable("bag") bag: Int, @PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.bagSlot(bag, slot) }
}
