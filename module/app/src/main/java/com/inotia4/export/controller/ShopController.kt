package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class ShopController {

    @GetMapping("/api/item/shop/items")
    fun items(): String = ControllerGuard.guard(ApiServices.info::shopItems)

    @PostMapping("/api/item/shop/buy_item")
    fun buy(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val slot = o.optInt("slot", -1)
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required\"}"
        return ControllerGuard.guard { ApiServices.action.shopBuy(slot) }
    }

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        null
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
