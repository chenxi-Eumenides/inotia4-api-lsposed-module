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

    @GetMapping("/api/data/map/list")
    fun mapList(): String {
        val tables = JsonUtil.parseObj(StaticData.read("tables/MAPINFOBASE.json")) ?: return JsonUtil.NOT_FOUND
        val records = tables.optJSONArray("records") ?: return JsonUtil.NOT_FOUND
        val list = JSONArray()
        for (i in 0 until records.length()) {
            val r = records.optJSONObject(i) ?: continue
            val u16 = r.optJSONArray("u16")
            val mapId = u16?.optInt(0, -1) ?: -1
            if (mapId < 0) continue
            list.put(JSONObject().put("mapId", mapId).put("name", r.optString("text_0", "")))
        }
        return JsonUtil.wrap("maps", list)
    }

    @GetMapping("/api/data/map/{mapId}")
    fun mapDetail(@PathVariable("mapId") mapId: Int): String {
        val tables = JsonUtil.parseObj(StaticData.read("tables/MAPINFOBASE.json")) ?: return JsonUtil.NOT_FOUND
        val records = tables.optJSONArray("records") ?: return JsonUtil.NOT_FOUND
        for (i in 0 until records.length()) {
            val r = records.optJSONObject(i) ?: continue
            val u16 = r.optJSONArray("u16")
            if (u16?.optInt(0, -1) == mapId) {
                return JsonUtil.wrap("mapId" to mapId, "name" to r.optString("text_0", ""), "raw" to r)
            }
        }
        return JsonUtil.NOT_FOUND
    }

    @GetMapping("/api/data/list")
    fun list(): String {
        val manifest = JsonUtil.parseObj(StaticData.read("manifest.json")) ?: return JsonUtil.NOT_FOUND
        return JsonUtil.wrap("tables", manifest.optJSONArray("tables") ?: JSONArray())
    }

    @GetMapping("/api/data/{table}")
    fun table(@PathVariable("table") table: String): String =
        readTable(table.uppercase())

    @GetMapping("/api/data/{table}/search")
    fun search(@PathVariable("table") table: String, @RequestParam("q") q: String): String =
        searchTable(table.uppercase(), q)

    @GetMapping("/api/data/events")
    fun events(): String = StaticData.read("reverse/events.json") ?: JsonUtil.NOT_FOUND

    @GetMapping("/api/data/text")
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
