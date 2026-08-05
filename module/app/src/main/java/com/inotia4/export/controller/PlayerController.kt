package com.inotia4.export.controller

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

    private fun withItemNames(json: String): String {
        return try {
            if (json.trimStart().startsWith("[")) {
                val arr = JSONArray(json)
                for (i in 0 until arr.length()) {
                    val role = arr.getJSONObject(i)
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
            json
        }
    }

    private fun injectItemName(item: JSONObject) {
        val category = item.optInt("category", -1)
        if (category >= 0) {
            StaticData.itemName(category)?.let { item.put("name", it) }
        }
    }
}
