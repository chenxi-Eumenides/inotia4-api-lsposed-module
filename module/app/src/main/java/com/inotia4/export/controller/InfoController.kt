package com.inotia4.export.controller

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.StaticData
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONArray
import org.json.JSONObject

/**
 * 游戏动态信息获取端点（GET，只读）：玩家状态/背包/地图/单位/UI/事件。
 * API 分层：/api/info 动态信息（本类）、/api/data 静态数据（DataController）、
 * /api/action 玩家操作（PlayerController）、/api/op OP 操作（未来 OpController）。
 */
@RestController
@RequestMapping("/api/info")
class InfoController {

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

    @GetMapping("/gamestate")
    fun gamestate(): String = NativeBridge.nativeGetGamestateJson()

    @GetMapping("/snapshot")
    fun snapshot(): String {
        val raw = NativeBridge.nativeGetSnapshotJson()
        return try {
            val root = JSONObject(raw)
            val party = root.optJSONArray("party")
            if (party != null) {
                for (p in 0 until party.length()) {
                    val member = party.optJSONObject(p) ?: continue
                    val eq = member.optJSONArray("equipment") ?: continue
                    for (e in 0 until eq.length()) {
                        eq.optJSONObject(e)?.let { injectItemName(it) }
                    }
                }
            }
            root.toString()
        } catch (e: Exception) {
            LogFile.logError("snapshot name injection failed", e)
            raw
        }
    }

    @GetMapping("/player/skills")
    fun skills(): String = NativeBridge.nativeGetSkillsJson()

    @GetMapping("/player/mercenaries")
    fun mercenaries(): String = NativeBridge.nativeGetMercenariesJson()

    @GetMapping("/path")
    fun path(@RequestParam("tx") tx: Int, @RequestParam("ty") ty: Int): String =
        NativeBridge.nativeGetPathJson(tx, ty)

    @GetMapping("/events")
    fun events(): String = NativeBridge.nativeGetEventsJson()

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
}
