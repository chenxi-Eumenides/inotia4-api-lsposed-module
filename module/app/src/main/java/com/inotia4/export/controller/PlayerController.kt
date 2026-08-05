package com.inotia4.export.controller

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.StaticData
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.RequestMapping
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
                LogFile.log("withItemNames array branch roles=${arr.length()}")
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
                LogFile.log("withItemNames object branch hasBags=${root.has("bags")}")
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
