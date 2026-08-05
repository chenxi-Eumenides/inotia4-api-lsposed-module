package com.inotia4.export

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

object StaticData {

    @Volatile
    private var appContext: Context? = null

    private val cache = HashMap<String, String>()

    @Volatile
    private var itemNames: Map<Int, String>? = null

    @Volatile
    private var texts: List<String>? = null

    fun attach(context: Context) {
        appContext = context
    }

    fun text(id: Int): String? {
        val t = texts ?: synchronized(this) {
            texts ?: loadTexts().also { texts = it }
        }
        return t.getOrNull(id)
    }

    private fun loadTexts(): List<String> {
        val json = read("text/zh-Hans.json") ?: return emptyList()
        return try {
            val arr = JSONArray(json)
            List(arr.length()) { arr.optString(it, "") }
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun read(path: String): String? {
        cache[path]?.let { return it }
        val ctx = appContext ?: return null
        val content = try {
            ctx.assets.open("static-data/$path").bufferedReader().use { it.readText() }
        } catch (e: Exception) {
            return null
        }
        if (cache.size < 64) cache[path] = content
        return content
    }

    fun itemName(itemId: Int): String? {
        val m = itemNames ?: synchronized(this) {
            itemNames ?: buildItemNames().also { itemNames = it }
        }
        return m[itemId]
    }

    private fun buildItemNames(): Map<Int, String> {
        val json = read("tables/ITEMDATABASE.json") ?: return emptyMap()
        return try {
            val records = JSONObject(json).getJSONArray("records")
            val map = HashMap<Int, String>(records.length())
            for (i in 0 until records.length()) {
                map[i] = records.getJSONObject(i).optString("text_0", "")
            }
            map
        } catch (e: Exception) {
            emptyMap()
        }
    }
}
