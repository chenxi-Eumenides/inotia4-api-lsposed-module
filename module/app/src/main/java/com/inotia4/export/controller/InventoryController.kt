package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController

/**
 * 背包：/api/item/inventory（api-reference §0.2）。复合 + money/items/bag/{i}/info + bag/{i}/{slot}。
 */
@RestController
@RequestMapping("/api/item/inventory")
class InventoryController {

    @GetMapping("/")
        fun composite(): String = ControllerGuard.guard(ApiServices.info::inventory)

    @GetMapping("/money")
    fun money(): String = ControllerGuard.guard(ApiServices.info::inventoryMoney)

    @GetMapping("/items")
    fun items(): String = ControllerGuard.guard(ApiServices.info::inventoryItems)

    @GetMapping("/bag/{bag}/info")
    fun bagInfo(@PathVariable("bag") bag: Int): String =
        ControllerGuard.guard { ApiServices.info.bagInfo(bag) }

    @GetMapping("/bag/{bag}/{slot}")
    fun bagSlot(@PathVariable("bag") bag: Int, @PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.bagSlot(bag, slot) }
}
