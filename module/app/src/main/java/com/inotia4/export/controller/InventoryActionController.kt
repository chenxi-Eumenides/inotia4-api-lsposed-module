package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class InventoryActionController {

    @PostMapping("/api/item/inventory/use-item")
    fun useItem(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        if (bag < 0 || slot < 0) return "{\"ok\":false,\"error\":\"bag/slot required\"}"
        return ControllerGuard.guard { ApiServices.action.useItem(bag, slot) }
    }

    @PostMapping("/api/item/inventory/dice-accept")
    fun diceAccept(): String = ControllerGuard.guard { ApiServices.action.diceAccept() }

    @PostMapping("/api/item/inventory/dice-reject")
    fun diceReject(): String = ControllerGuard.guard { ApiServices.action.diceReject() }

    @PostMapping("/api/item/inventory/discard")
    fun discard(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        if (bag < 0 || slot < 0) return "{\"ok\":false,\"error\":\"bag/slot required\"}"
        return ControllerGuard.guard { ApiServices.action.discardItem(bag, slot) }
    }

    @PostMapping("/api/item/inventory/sell")
    fun sell(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        if (bag < 0 || slot < 0) return "{\"ok\":false,\"error\":\"bag/slot required\"}"
        return ControllerGuard.guard { ApiServices.action.sellItem(bag, slot) }
    }

    @PostMapping("/api/item/inventory/move")
    fun move(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        val count = o.optInt("count", -1)
        val toBag = o.optInt("to_bag", -1)
        val toSlot = o.optInt("to_slot", -1)
        if (bag < 0 || slot < 0 || count <= 0 || toBag < 0 || toSlot < 0)
            return "{\"ok\":false,\"error\":\"bag/slot/count/to_bag/to_slot required\"}"
        return ControllerGuard.guard { ApiServices.action.moveItem(bag, slot, count, toBag, toSlot) }
    }

    @PostMapping("/api/item/inventory/{role}/jewel")
    fun jewel(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        val equipSlot = o.optInt("equip_slot", -1)
        if (bag < 0 || slot < 0 || equipSlot < 0)
            return "{\"ok\":false,\"error\":\"bag/slot/equip_slot required\"}"
        return ControllerGuard.guard { ApiServices.action.jewel(role, bag, slot, equipSlot) }
    }

    @PostMapping("/api/item/inventory/{role}/equip")
    fun equip(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        val category = o.optInt("category", -1)
        return if (category >= 0) {
            ControllerGuard.guard { ApiServices.action.equipByCategory(role, category) }
        } else if (bag >= 0 && slot >= 0) {
            ControllerGuard.guard { ApiServices.action.equip(role, bag, slot) }
        } else {
            "{\"ok\":false,\"error\":\"bag+slot or category required\"}"
        }
    }

    @PostMapping("/api/item/inventory/{role}/unequip")
    fun unequip(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val slot = o.optInt("slot", -1)
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required\"}"
        return ControllerGuard.guard { ApiServices.action.unequip(role, slot) }
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
