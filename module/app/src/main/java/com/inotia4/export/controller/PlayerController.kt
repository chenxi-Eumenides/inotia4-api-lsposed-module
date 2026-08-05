package com.inotia4.export.controller

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.StaticData
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONArray
import org.json.JSONObject

@RestController
@RequestMapping("/api")
class PlayerController {

    @GetMapping("/player")
    fun player(): String = NativeBridge.nativeGetPlayerJson()

    @GetMapping("/player/party")
    fun party(): String = withItemNames(NativeBridge.nativeGetPartyJson())

    @GetMapping("/inventory")
    fun inventory(): String = withItemNames(NativeBridge.nativeGetInventoryJson())

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

    @GetMapping("/player/mercenaries")
    fun mercenaries(): String = NativeBridge.nativeGetMercenariesJson()

    @GetMapping("/path")
    fun path(@RequestParam("tx") tx: Int, @RequestParam("ty") ty: Int): String =
        NativeBridge.nativeGetPathJson(tx, ty)

    @GetMapping("/events")
    fun events(): String = NativeBridge.nativeGetEventsJson()

    // ---- 操作端点（POST，v0.3.0，签名见 docs/notes/control-capability.md §5）----

    @PostMapping("/player/money")
    fun money(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val amount = o.optLong("amount", -1)
        if (amount < 0) return "{\"ok\":false,\"error\":\"amount required\"}"
        val res = when (o.optString("action", "set")) {
            "add" -> NativeBridge.nativeOpAddMoney(amount)
            "minus" -> NativeBridge.nativeOpMinusMoney(amount)
            else -> NativeBridge.nativeOpSetMoney(amount)
        }
        return attachPlayer(res)
    }

    @PostMapping("/player/{role}/experience")
    fun experience(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val amount = o.optLong("amount", -1)
        if (amount < 0) return "{\"ok\":false,\"error\":\"amount required\"}"
        val res = if (o.optString("action", "add") == "set")
            NativeBridge.nativeOpSetExperience(role, amount)
        else NativeBridge.nativeOpAddExperience(role, amount)
        return attachParty(res)
    }

    @PostMapping("/player/{role}/status-point")
    fun statusPoint(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val points = o.optInt("points", -1)
        if (points < 0) return "{\"ok\":false,\"error\":\"points required\"}"
        return attachParty(NativeBridge.nativeOpSetStatusPoint(role, points))
    }

    @PostMapping("/player/{role}/auto-attack")
    fun autoAttack(@PathVariable("role") role: Int, @RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        return attachParty(NativeBridge.nativeOpSetAutoAttack(role, if (o.optBoolean("on")) 1 else 0))
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

    @PostMapping("/teleport")
    fun teleport(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val mapId = o.optInt("mapId", 0)
        val x = o.optInt("x", -1)
        val y = o.optInt("y", -1)
        if (x < 0 || y < 0) return "{\"ok\":false,\"error\":\"x/y required\"}"
        return attachPlayer(NativeBridge.nativeOpTeleport(mapId, x, y))
    }

    @PostMapping("/inventory/remove")
    fun removeItem(@RequestBody body: String): String {
        val o = parseBody(body) ?: return BAD_BODY
        val category = o.optInt("category", -1)
        if (category < 0) return "{\"ok\":false,\"error\":\"category required\"}"
        return attachInventory(NativeBridge.nativeOpRemoveItem(category))
    }

    private fun parseBody(body: String): JSONObject? = try {
        JSONObject(body)
    } catch (e: Exception) {
        LogFile.logError("parse body failed", e)
        null
    }

    private fun attachPlayer(op: String): String = attach(op) { NativeBridge.nativeGetPlayerJson() }

    private fun attachParty(op: String): String = attach(op) {
        withItemNames(NativeBridge.nativeGetPartyJson())
    }

    private fun attachInventory(op: String): String = attach(op) {
        withItemNames(NativeBridge.nativeGetInventoryJson())
    }

    private fun attachSkills(op: String): String = attach(op) { NativeBridge.nativeGetSkillsJson() }

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

    private fun withItemNames(json: String): String {
        return try {
            if (json.trimStart().startsWith("[")) {
                val arr = JSONArray(json)
                for (i in 0 until arr.length()) {
                    val role = arr.optJSONObject(i) ?: continue
                    injectAttrNames(role)
                    val eq = role.optJSONArray("equipment") ?: continue
                    for (e in 0 until eq.length()) {
                        eq.optJSONObject(e)?.let { injectItemName(it) }
                    }
                }
                arr.toString()
            } else {
                val root = JSONObject(json)
                if (root.has("bags")) {
                    val bags = root.getJSONArray("bags")
                    for (b in 0 until bags.length()) {
                        val items = bags.getJSONObject(b).optJSONArray("items") ?: continue
                        for (i in 0 until items.length()) {
                            injectItemName(items.getJSONObject(i))
                        }
                    }
                }
                root.toString()
            }
        } catch (e: Exception) {
            LogFile.logError("withItemNames failed", e)
            json
        }
    }

    private fun injectItemName(item: JSONObject) {
        val category = item.optInt("category", -1)
        if (category >= 0) {
            StaticData.itemName(category)?.let { item.put("name", it) }
        }
    }

    private fun injectAttrNames(role: JSONObject) {
        val attrs = JSONArray()
        val mainNames = listOf("力量", "敏捷", "体力", "智力", "精力")
        val mainStats = role.optJSONArray("mainStats")
        if (mainStats != null) {
            for (i in 0 until mainStats.length()) {
                if (i >= mainNames.size) break
                val obj = JSONObject()
                obj.put("id", i)
                obj.put("name", mainNames[i])
                obj.put("value", mainStats.optInt(i))
                attrs.put(obj)
            }
        }
        role.optInt("statusPoint", -1).takeIf { it >= 0 }?.let {
            val obj = JSONObject()
            obj.put("id", -1)
            obj.put("name", "能力点")
            obj.put("value", it)
            attrs.put(obj)
        }
        val stats = role.optJSONObject("stats")
        if (stats != null) {
            for ((id, name) in listOf(30 to "HP上限", 31 to "MP上限")) {
                val obj = JSONObject()
                obj.put("id", id)
                obj.put("name", name)
                obj.put("value", stats.optInt(id.toString(), 0))
                attrs.put(obj)
            }
        }
        if (attrs.length() > 0) role.put("attrs", attrs)
    }

    private companion object {
        const val BAD_BODY = "{\"ok\":false,\"error\":\"bad body\"}"
    }
}
