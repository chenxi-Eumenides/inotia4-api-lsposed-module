package com.inotia4.export.controller

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

/**
 * 合法操作端点（v0.4.0 API 重构）：与信息获取端点分离。
 * 合法操作 = 玩家在游戏内能做的事（见 docs/notes/player-operations.md）。
 * OP 操作（改数据/强行操作）走未来 /api/op/ 组（需权限），不在此暴露。
 */
@RestController
@RequestMapping("/api/action")
class OperationController {

    @PostMapping("/player/money")
    fun money(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val amount = o.optLong("amount", -1)
        if (amount < 0) return "{\"ok\":false,\"error\":\"amount required\"}"
        val res = when (o.optString("action", "add")) {
            "minus" -> NativeBridge.nativeOpMinusMoney(amount)
            else -> NativeBridge.nativeOpAddMoney(amount)
        }
        return attachPlayer(res)
    }

    @PostMapping("/player/move")
    fun move(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val x = o.optInt("x", -1)
        val y = o.optInt("y", -1)
        if (x < 0 || y < 0) return "{\"ok\":false,\"error\":\"x/y required\"}"
        return attachPlayer(NativeBridge.nativeOpMove(x, y))
    }

    @PostMapping("/player/use-item")
    fun useItem(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        if (bag < 0 || slot < 0) return "{\"ok\":false,\"error\":\"bag/slot required\"}"
        return attachInventory(NativeBridge.nativeOpUseItem(bag, slot))
    }

    @PostMapping("/player/{role}/equip")
    fun equip(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        val category = o.optInt("category", -1)
        val res = if (category >= 0) {
            findItemSlot(category)?.let { (b, s) ->
                NativeBridge.nativeOpEquip(role, b, s)
            } ?: "{\"ok\":false,\"error\":\"item not found\"}"
        } else if (bag >= 0 && slot >= 0) {
            NativeBridge.nativeOpEquip(role, bag, slot)
        } else {
            "{\"ok\":false,\"error\":\"bag+slot or category required\"}"
        }
        return attachParty(res)
    }

    @PostMapping("/player/{role}/unequip")
    fun unequip(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val slot = o.optInt("slot", -1)
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required\"}"
        return attachParty(NativeBridge.nativeOpUnequip(role, slot))
    }

    @PostMapping("/player/{role}/auto-attack")
    fun autoAttack(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        return attachParty(NativeBridge.nativeOpSetAutoAttack(role, if (o.optBoolean("on")) 1 else 0))
    }

    @PostMapping("/player/{role}/skill")
    fun skill(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val actionId = o.optInt("actionId", -1)
        if (actionId < 0) return "{\"ok\":false,\"error\":\"actionId required\"}"
        val level = o.optInt("level", 1)
        return attachSkills(NativeBridge.nativeOpLearnAction(role, actionId, level))
    }

    @PostMapping("/player/switch")
    fun switch(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val slot = o.optInt("slot", -1)
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required\"}"
        return attachPlayer(NativeBridge.nativeOpSwitchPlayer(slot))
    }

    @PostMapping("/inventory/discard")
    fun discard(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        if (bag < 0 || slot < 0) return "{\"ok\":false,\"error\":\"bag/slot required\"}"
        return attachInventory(NativeBridge.nativeOpDiscardItem(bag, slot))
    }

    @PostMapping("/inventory/sell")
    fun sell(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val bag = o.optInt("bag", -1)
        val slot = o.optInt("slot", -1)
        val price = o.optLong("price", -1)
        if (bag < 0 || slot < 0 || price < 0)
            return "{\"ok\":false,\"error\":\"bag/slot/price required\"}"
        return attachPlayer(NativeBridge.nativeOpSellItem(bag, slot, price))
    }

    @PostMapping("/party/include")
    fun includeParty(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mercSlot = o.optInt("mercenarySlot", -1)
        if (mercSlot < 0) return "{\"ok\":false,\"error\":\"mercenarySlot required\"}"
        return attachParty(NativeBridge.nativeOpIncludeParty(mercSlot))
    }

    @PostMapping("/party/exclude")
    fun excludeParty(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mercSlot = o.optInt("mercenarySlot", -1)
        if (mercSlot < 0) return "{\"ok\":false,\"error\":\"mercenarySlot required\"}"
        return attachParty(NativeBridge.nativeOpExcludeParty(mercSlot))
    }

    @PostMapping("/teleport")
    fun teleport(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mapId = o.optInt("mapId", 0)
        val x = o.optInt("x", -1)
        val y = o.optInt("y", -1)
        if (x < 0 || y < 0) return "{\"ok\":false,\"error\":\"x/y required\"}"
        return attachPlayer(NativeBridge.nativeOpTeleport(mapId, x, y))
    }

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        LogFile.logError("parse body failed", e)
        null
    }

    private fun attachPlayer(op: String): String =
        attach(op) { NativeBridge.nativeGetPlayerJson() }

    private fun attachParty(op: String): String =
        attach(op) { NativeBridge.nativeGetPartyJson() }

    private fun attachInventory(op: String): String =
        attach(op) { NativeBridge.nativeGetInventoryJson() }

    private fun attachSkills(op: String): String =
        attach(op) { NativeBridge.nativeGetSkillsJson() }

    private fun attach(op: String, latest: () -> String): String {
        return try {
            val obj = JSONObject(op)
            if (obj.optBoolean("ok", false)) obj.put("state", JSONObject(latest()))
            obj.toString()
        } catch (e: Exception) {
            op
        }
    }

    private fun findItemSlot(category: Int): Pair<Int, Int>? {
        val inv = try {
            JSONObject(NativeBridge.nativeGetInventoryJson())
        } catch (e: Exception) {
            return null
        }
        val bags = inv.optJSONArray("bags") ?: return null
        for (b in 0 until bags.length()) {
            val bag = bags.getJSONObject(b)
            val items = bag.optJSONArray("items") ?: continue
            for (i in 0 until items.length()) {
                val item = items.getJSONObject(i)
                if (item.optInt("category", -1) == category) return b to item.optInt("slot", -1)
            }
        }
        return null
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
