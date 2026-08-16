package com.inotia4.export.service

import com.inotia4.export.LogFile
import com.inotia4.export.StaticData
import org.json.JSONArray
import org.json.JSONObject

/**
 * 名称/字段注入助手（v0.5.46 P1 收边，从 InfoApiServiceImpl 抽出）：
 * 7 个 inject* 纯函数式注入 + withItemNames 组装，无状态，仅依赖 StaticData 查询与 LogFile。
 */
object NameInjector {

    // v0.5.1：技能名注入（StaticData.skillName = 技能信息表 rec+0 text_id = 1220+action）
    fun injectSkillNames(role: JSONObject) {
        val skills = role.optJSONArray("skills") ?: return
        for (i in 0 until skills.length()) {
            val sk = skills.optJSONObject(i) ?: continue
            val actionId = sk.optInt("action_id", -1)
            if (actionId >= 0) {
                val name = StaticData.skillName(actionId)
                if (name != null) sk.put("skill_name", name)
            }
        }
    }

    /** 从 QUESTS.json 解析产物注入字段（v0.5.37） */
    fun injectQuestFields(target: JSONObject, data: JSONObject, keys: List<String>) {
        for (key in keys) {
            if (data.has(key)) target.put(key, data.get(key))
        }
    }

    fun injectEquipmentNames(role: JSONObject) {
        val eq = role.optJSONArray("equipment") ?: return
        for (e in 0 until eq.length()) {
            eq.optJSONObject(e)?.let { injectItemName(it, true) }
        }
    }

    fun injectItemName(item: JSONObject, equipOverride: Boolean? = null) {
        val category = item.optInt("category", -1)
        if (category >= 0) {
            val base = StaticData.itemName(category)
            if (base != null) {
                // v0.5.12：装备判定优先显式标记（party 装备路径无 count 字段），否则用 native equip 标志
                //（⑤ count 语义修正后装备 count=1，不能再以 count==100 判定）
                val isEquip = equipOverride ?: item.optBoolean("equip", false)
                // v0.5.12 ③修复：品级前缀用 raw_rarity（原始位 0-15，ITEMGRADEBASE 表），rarity=GetRarity 档位仅用于 tier
                val prefix = if (isEquip) StaticData.rarityPrefix(item.optInt("raw_rarity", -1)) else ""
                item.put("name", prefix + base)
                // v0.5.12 ⑥ 稀有度档位名（GetRarity 0-4 → 白绿蓝黄紫）
                if (item.has("rarity")) {
                    StaticData.rarityTierName(item.optInt("rarity", -1)).takeIf { it.isNotEmpty() }
                        ?.let { item.put("rarity_tier", it) }
                }
                // v0.5.12 ⑦ 静态词条名（ITEMSTATICOPTBASE，item_id=category 聚合）
                val staticOpts = StaticData.staticOptionNames(category)
                if (staticOpts.isNotEmpty()) item.put("static_options", JSONArray(staticOpts))
            }
        }
        injectItemOptions(item)
        injectSocketEnchant(item)
    }

    // v0.4.64：词缀名称/明细（optionIds 索引数组 + options 值数组，一一对应）
    fun injectItemOptions(item: JSONObject) {
        val optionIds = item.optJSONArray("option_ids")
        if (optionIds == null || optionIds.length() == 0) return
        val options = item.optJSONArray("options")
        val optNames = JSONArray()
        val optDetails = JSONArray()
        for (o in 0 until optionIds.length()) {
            val id = optionIds.optInt(o, -1)
            if (id < 0) continue
            val name = StaticData.optionName(id) ?: ""
            optNames.put(name)
            val value = if (options != null && o < options.length()) options.optInt(o) else 0
            optDetails.put(JSONObject().put("id", id).put("name", name).put("value", value))
        }
        item.put("option_names", optNames)
        item.put("options_detailed", optDetails)
    }

    // v0.4.64：宝石孔/附魔/混沌 拆解信息（native 已输出位域拆解字段，此处组装可读对象）
    fun injectSocketEnchant(item: JSONObject) {
        val hasSocket = item.has("socket_filled") || item.has("socket_total")
        if (hasSocket) {
            val info = JSONObject()
            info.put("filled", item.optInt("socket_filled", 0))
            info.put("total", item.optInt("socket_total", 0))
            item.put("socket_info", info)
        }
        val hasEnchant = item.has("enchant_id") || item.has("enchant_level") || item.has("chaos")
        if (hasEnchant) {
            val info = JSONObject()
            val eid = item.optInt("enchant_id", 0)
            info.put("id", eid)
            info.put("level", item.optInt("enchant_level", 0))
            info.put("chaos", item.optBoolean("chaos", false))
            StaticData.enchantName(eid)?.let { info.put("name", it) }
            item.put("enchant_info", info)
        }
        if (item.has("chaos_level") || item.has("chaos_rate")) {
            val info = JSONObject()
            info.put("level", item.optInt("chaos_level", 0))
            info.put("rate", item.optInt("chaos_rate", 0))
            item.put("chaos_info", info)
        }
    }

    /** 职业名称注入：class_idx 0-5 → CHARCLASSBASE 联查（StaticData.className，v0.5.1 实机验证） */
    fun injectClassName(role: JSONObject) {
        val classIdx = role.optInt("class_idx", -1)
        // 限定 0-5：type==2 装饰物该字段存 type 值（C_CLASS 注释），非真实职业索引，防误注入
        if (classIdx in 0..5) {
            StaticData.className(classIdx)?.let { role.put("class_name", it) }
        }
    }

    fun injectAttrNames(role: JSONObject) {
        val attrs = JSONArray()
        val mainNames = listOf("力量", "敏捷", "体力", "智力", "精力")
        val mainStats = role.optJSONArray("main_stats")
        if (mainStats != null) {
            for (i in 0 until mainStats.length()) {
                if (i >= mainNames.size) break
                attrs.put(JSONObject().put("id", i).put("name", mainNames[i]).put("value", mainStats.optInt(i)))
            }
        }
        role.optInt("status_point", -1).takeIf { it >= 0 }?.let {
            attrs.put(JSONObject().put("id", -1).put("name", "能力点").put("value", it))
        }
        val stats = role.optJSONObject("stats")
        if (stats != null) {
            for ((id, name) in listOf(30 to "HP上限", 31 to "MP上限")) {
                attrs.put(JSONObject().put("id", id).put("name", name).put("value", stats.optInt(id.toString(), 0)))
            }
        }
        if (attrs.length() > 0) role.put("attrs", attrs)
    }

    fun withItemNames(json: String): String {
        return try {
            val trimmed = json.trimStart()
            if (trimmed.startsWith("[")) {
                val arr = JSONArray(json)
                for (i in 0 until arr.length()) {
                    val role = arr.optJSONObject(i) ?: continue
                    injectClassName(role)
                    injectAttrNames(role)
                    injectEquipmentNames(role)
                }
                arr.toString()
            } else {
                val root = JSONObject(json)
                if (root.has("class_idx")) {
                    // 单角色对象（party/{slot} 等）：与数组分支同构注入
                    injectClassName(root)
                    injectAttrNames(root)
                    injectEquipmentNames(root)
                } else if (root.has("bags")) {
                    val bags = root.getJSONArray("bags")
                    for (b in 0 until bags.length()) {
                        val items = bags.getJSONObject(b).optJSONArray("items") ?: continue
                        for (i in 0 until items.length()) {
                            injectItemName(items.getJSONObject(i))
                        }
                    }
                } else if (root.has("party")) {
                    val party = root.optJSONArray("party")
                    if (party != null) {
                        for (p in 0 until party.length()) {
                            val member = party.optJSONObject(p) ?: continue
                            injectEquipmentNames(member)
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
}
