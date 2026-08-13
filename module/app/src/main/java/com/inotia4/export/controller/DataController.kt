package com.inotia4.export.controller

import com.inotia4.export.StaticData
import com.inotia4.export.util.JsonUtil
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONArray
import org.json.JSONObject

@RestController
class DataController {

    @GetMapping("/api/world/maps/list")
    fun mapList(): String {
        val tables = JsonUtil.parseObj(StaticData.read("tables/MAPINFOBASE.json")) ?: return JsonUtil.NOT_FOUND
        val records = tables.optJSONArray("records") ?: return JsonUtil.NOT_FOUND
        val list = JSONArray()
        for (i in 0 until records.length()) {
            val r = records.optJSONObject(i) ?: continue
            val u16 = r.optJSONArray("u16")
            val mapId = u16?.optInt(0, -1) ?: -1
            if (mapId < 0) continue
            list.put(JSONObject().put("map_id", mapId).put("name", r.optString("text_0", "")))
        }
        return JsonUtil.wrap("maps", list)
    }

    @GetMapping("/api/world/maps/{mapId}")
    fun mapDetail(@PathVariable("mapId") mapId: Int): String {        val tables = JsonUtil.parseObj(StaticData.read("tables/MAPINFOBASE.json")) ?: return JsonUtil.NOT_FOUND
        val records = tables.optJSONArray("records") ?: return JsonUtil.NOT_FOUND
        // v0.4.28：真 mapId = MAPINFOBASE 记录下标（运行时 current_map_id 验证：30=影子丛林1/31=影子丛林2）
        if (mapId in 0 until records.length()) {
            val r = records.optJSONObject(mapId) ?: return JsonUtil.NOT_FOUND
            val textId = r.optJSONArray("u16")?.optInt(0, -1) ?: -1
            return JsonUtil.wrap("map_id" to mapId, "text_id" to textId,
                "name" to r.optString("text_0", ""), "raw" to r)
        }
        // 兼容旧语义：按 text_id 匹配
        for (i in 0 until records.length()) {
            val r = records.optJSONObject(i) ?: continue
            val u16 = r.optJSONArray("u16")
            if (u16?.optInt(0, -1) == mapId) {
                return JsonUtil.wrap("map_id" to mapId, "index" to i, "text_id" to mapId,
                    "name" to r.optString("text_0", ""), "raw" to r)
            }
        }
        return JsonUtil.NOT_FOUND
    }

    @GetMapping("/api/world/maps/{mapId}/tiles")
    fun mapTiles(@PathVariable("mapId") mapId: Int): String {
        val tiles = JsonUtil.parseObj(StaticData.read("maps/tiles.json")) ?: return JsonUtil.NOT_FOUND
        val entry = tiles.optJSONObject("m$mapId") ?: return "{\"error\":\"no tiles\"}"
        val raw = entry.optString("tiles", "")
        if (raw.isEmpty()) return "{\"error\":\"no tiles\"}"
        return JsonUtil.wrap("map_id" to mapId, "src" to "static", "size" to 64,
            "encoding" to "base64", "tiles" to raw)
    }

    @GetMapping("/api/system/tables")
    fun list(): String {
        val manifest = JsonUtil.parseObj(StaticData.read("manifest.json")) ?: return JsonUtil.NOT_FOUND
        return JsonUtil.wrap("tables", manifest.optJSONArray("tables") ?: JSONArray())
    }

    @GetMapping("/api/system/tables/{table}")
    fun table(@PathVariable("table") table: String): String =
        readTable(table.uppercase())

    @GetMapping("/api/system/tables/{table}/search")
    fun search(@PathVariable("table") table: String, @RequestParam("q") q: String): String =
        searchTable(table.uppercase(), q)

    @GetMapping("/api/system/story-events")
    fun events(): String = StaticData.read("reverse/events.json") ?: JsonUtil.NOT_FOUND

    @GetMapping("/api/system/text")
    fun text(@RequestParam("lang") lang: String): String =
        StaticData.read("text/${lang}.json") ?: JsonUtil.NOT_FOUND

    private fun readTable(name: String): String =
        StaticData.read("tables/${name}.json") ?: JsonUtil.NOT_FOUND

    private fun searchTable(name: String, q: String): String {
        val json = StaticData.read("tables/${name}.json") ?: return JsonUtil.NOT_FOUND
        val tables = JsonUtil.parseObj(json) ?: return JsonUtil.NOT_FOUND
        val records = tables.optJSONArray("records") ?: return JsonUtil.NOT_FOUND
        val keyword = q.trim().lowercase()
        if (keyword.isEmpty()) return JsonUtil.wrap("items", records)
        val out = JSONArray()
        for (i in 0 until records.length()) {
            val r = records.optJSONObject(i) ?: continue
            val name = r.optString("text_0", "")
            if (name.lowercase().contains(keyword)) {
                out.put(JSONObject().put("index", i).put("name", name).put("raw", r))
            }
        }
        return JsonUtil.wrap("items", out)
    }
}
