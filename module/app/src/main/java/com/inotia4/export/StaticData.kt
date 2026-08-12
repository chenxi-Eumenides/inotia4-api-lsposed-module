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
    private var optionNames: Map<Int, String>? = null

    fun attach(context: Context) {
        appContext = context
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

    /** 品质前缀（v0.4.62 frida 真机 rarity 0-15 实测）：rarity 位 → 品级前缀 */
    fun rarityPrefix(rarity: Int): String = when (rarity) {
        0 -> "生锈的 "
        1 -> "陈旧的 "
        3 -> "太古的 "
        4 -> "锐利的 "
        5 -> "打磨的 "
        6 -> "工匠的 "
        7 -> "钢铁 "
        8 -> "钛金 "
        9 -> "秘银 "
        else -> ""
    }

    /** 词缀名称（v0.4.62 静态表解析）：词缀 id → text 表名称（力/敏/体/智...） */
    fun optionName(optionId: Int): String? {
        val m = optionNames ?: synchronized(this) {
            optionNames ?: buildOptionNames().also { optionNames = it }
        }
        return m[optionId]
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

    private fun buildOptionNames(): Map<Int, String> {
        val json = read("tables/ITEMOPTINFOBASE.json") ?: return emptyMap()
        return try {
            val records = JSONObject(json).getJSONArray("records")
            val text = read("text/zh-Hans.json") ?: return emptyMap()
            val textArr = JSONArray(text)
            val map = HashMap<Int, String>(records.length())
            for (i in 0 until records.length()) {
                val u16 = records.getJSONObject(i).optJSONArray("u16") ?: continue
                val textId = u16.optInt(0, -1)
                if (textId in 0 until textArr.length()) {
                    val name = textArr.optString(textId)
                    if (name.isNotEmpty()) map[i] = name
                }
            }
            map
        } catch (e: Exception) {
            emptyMap()
        }
    }

    /**
     * 附魔/强化名称（2026-08-12）：enchantId → ITEMENCHANTBASE 记录(enchantId-1) +0x00 卷轴类别 →
     * ITEMDATABASE 物品名。enchantId=0（无附魔）或无记录时返回 null。
     */
    fun enchantName(enchantId: Int): String? {
        if (enchantId <= 0) return null
        val m = enchantNames ?: synchronized(this) {
            enchantNames ?: buildEnchantNames().also { enchantNames = it }
        }
        return m[enchantId]
    }

    @Volatile
    private var enchantNames: Map<Int, String>? = null

    private fun buildEnchantNames(): Map<Int, String> {
        val ench = read("tables/ITEMENCHANTBASE.json") ?: return emptyMap()
        return try {
            val records = JSONObject(ench).getJSONArray("records")
            val map = HashMap<Int, String>(records.length())
            for (i in 0 until records.length()) {
                val u16 = records.getJSONObject(i).optJSONArray("u16") ?: continue
                val scrollCat = u16.optInt(0, -1)
                if (scrollCat >= 0) {
                    val name = itemName(scrollCat) ?: continue
                    map[i + 1] = name
                }
            }
            map
        } catch (e: Exception) {
            emptyMap()
        }
    }
}
