package com.inotia4.export.service

import com.inotia4.export.BuildConfig
import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.StaticData
import com.inotia4.export.util.JsonUtil
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * 信息查询服务实现（InfoApiService 接口的唯一实现，v0.4.0 迁移自 InfoService）。
 * 字段提取、名称注入、快照组装全部在此，调用层只做路由与参数透传。
 */
class InfoApiServiceImpl : InfoApiService {

    override fun ready(): Boolean = NativeBridge.ready

    override fun currentMapExits(): String {
        val mj = mapJson()
        if (isNativeError(mj)) return mj
        val exits = JsonUtil.parseObj(mj)?.optJSONArray("exits") ?: return "{\"exits\":[]}"
        return JsonUtil.wrap("exits", exits)
    }

    override fun currentMapId(): String {
        val mj = mapJson()
        if (isNativeError(mj)) return mj
        val mapId = JsonUtil.parseObj(mj)?.optInt("map_id", -1) ?: -1
        // W3 (v0.5.3)：单值端点注入 id_name（MAPINFOBASE 联查，与复合端点 map_data.name 一致）
        val out = JSONObject().put("map_id", mapId)
        attachMapStatic(mapId)?.let { out.put("id_name", it.optString("name", "")) }
        return out.toString()
    }

    override fun currentMapUnits(): String {
        val uj = unitsJson()
        if (isNativeError(uj)) return uj
        return uj
    }

    override fun currentMapEnemies(): String {
        val ej = enemiesJson()
        if (isNativeError(ej)) return ej
        return ej
    }

    override fun currentMapInteractives(): String {
        val ij = interactivesJson()
        if (isNativeError(ij)) return ij
        return ij
    }

    override fun currentMapDrops(): String {
        val dj = dropsJson()
        if (isNativeError(dj)) return dj
        return dj
    }

    override fun currentMapDistance(tx: Int, ty: Int): String = NativeBridge.nativeDistanceJson(tx, ty)

    override fun party(): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        return withItemNames(pj)
    }

    override fun partyCount(): String {
        val pj = playerJson()
        if (isNativeError(pj)) return pj
        return JsonUtil.wrap("count", JsonUtil.parseObj(pj)?.optInt("party_count", -1) ?: -1)
    }

    override fun partyLeader(): String {
        val pj = playerJson()
        if (isNativeError(pj)) return pj
        val pj2 = partyJson()
        if (isNativeError(pj2)) return pj2
        val p = partyArr() ?: return JsonUtil.NOT_FOUND
        val leaderSlot = JsonUtil.parseObj(pj)?.optInt("main_mercenary_slot", 0) ?: 0
        val m = if (leaderSlot in 0 until p.length()) p.optJSONObject(leaderSlot) else null
        return m?.let { withItemNames(it.toString()) } ?: JsonUtil.NOT_FOUND
    }

    override fun partyMember(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        val p = partyArr() ?: return JsonUtil.NOT_FOUND
        val m = p.optJSONObject(slot) ?: return JsonUtil.NOT_FOUND
        return withItemNames(m.toString())
    }

    override fun partyMemberId(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        val m = memberObj(slot) ?: return JsonUtil.NOT_FOUND
        val classIdx = m.optInt("class_idx", -1)
        if (classIdx < 0) return JsonUtil.NOT_FOUND
        val out = JSONObject().put("id", classIdx)
        StaticData.className(classIdx)?.let { out.put("id_name", it) }
        return out.toString()
    }

    override fun partyMemberName(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        return memberString(slot, "name")
    }

    override fun partyMemberLevel(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        return memberInt(slot, "level")
    }

    // v0.5.13：状态聚合端点（api-reference §2.1 party/{slot}/status），数据源 member json + skills json
    override fun partyMemberStatus(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        val m = memberObj(slot) ?: return JsonUtil.NOT_FOUND
        val out = JSONObject()
        m.optInt("hp", -1).takeIf { it >= 0 }?.let { out.put("hp", it) }
        m.optInt("max_hp", -1).takeIf { it >= 0 }?.let { out.put("max_hp", it) }
        m.optInt("mp", -1).takeIf { it >= 0 }?.let { out.put("mp", it) }
        m.optInt("max_mp", -1).takeIf { it >= 0 }?.let { out.put("max_mp", it) }
        m.optInt("exp", -1).takeIf { it >= 0 }?.let { out.put("exp", it) }
        m.optInt("exp_next", -1).takeIf { it >= 0 }?.let { out.put("exp_next", it) }
        m.optInt("status_point", -1).takeIf { it >= 0 }?.let { out.put("attribute_points", it) }
        skillsArr()?.optJSONObject(slot)?.optInt("skill_points", -1)?.takeIf { it >= 0 }?.let { out.put("skill_points", it) }
        return JsonUtil.wrap("status", out)
    }

    override fun partyMemberStats(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        return JsonUtil.wrap("stats", memberObj(slot)?.optJSONObject("stats"))
    }

    override fun partyMemberEquipment(slot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        val eq = memberObj(slot)?.optJSONArray("equipment") ?: return JsonUtil.NOT_FOUND
        val arr = JSONArray()
        for (i in 0 until eq.length()) {
            eq.optJSONObject(i)?.let { injectItemName(it, true); arr.put(it) }
        }
        return JsonUtil.wrap("equipment", arr)
    }

    override fun partyMemberEquip(slot: Int, equipSlot: Int): String {
        val pj = partyJson()
        if (isNativeError(pj)) return pj
        val eq = memberObj(slot)?.optJSONArray("equipment") ?: return JsonUtil.NOT_FOUND
        val it = eq.optJSONObject(equipSlot) ?: return JsonUtil.NOT_FOUND
        injectItemName(it, true)
        return it.toString()
    }

    override fun partyMemberSkills(slot: Int): String {
        val sj = skillsJson()
        if (isNativeError(sj)) return sj
        val s = JsonUtil.parseArr(sj) ?: return JsonUtil.NOT_FOUND
        val obj = s.optJSONObject(slot) ?: return JsonUtil.NOT_FOUND
        injectSkillNames(obj)
        return obj.toString()
    }

    // v0.5.1：技能名注入（StaticData.skillName = 技能信息表 rec+0 text_id = 1220+action）
    private fun injectSkillNames(role: JSONObject) {
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

    override fun mercenary(): String {
        val mj = mercenariesJson()
        if (isNativeError(mj)) return mj
        return mj
    }

    override fun mercenaryList(): String {
        val mj = mercenariesJson()
        if (isNativeError(mj)) return mj
        val arr = JsonUtil.parseArr(mj) ?: return JsonUtil.wrap("slots", JSONArray())
        val slots = JSONArray()
        for (i in 0 until arr.length()) {
            arr.optJSONObject(i)?.optInt("slot", -1)?.takeIf { it >= 0 }?.let { slots.put(it) }
        }
        return JsonUtil.wrap("slots", slots)
    }

    override fun mercenarySlot(slot: Int): String {
        val mj = mercenariesJson()
        if (isNativeError(mj)) return mj
        val arr = JsonUtil.parseArr(mj) ?: return JsonUtil.NOT_FOUND
        for (i in 0 until arr.length()) {
            val m = arr.optJSONObject(i) ?: continue
            if (m.optInt("slot", -1) == slot) return m.toString()
        }
        return JsonUtil.NOT_FOUND
    }

    override fun inventory(): String {
        val ij = inventoryJson()
        if (isNativeError(ij)) return ij
        return withItemNames(ij)
    }

    override fun inventoryMoney(): String {
        val pj = playerJson()
        if (isNativeError(pj)) return pj
        return JsonUtil.wrap("money", JsonUtil.parseObj(pj)?.optLong("money", -1) ?: -1L)
    }

    override fun inventoryItems(): String {
        val ij = inventoryJson()
        if (isNativeError(ij)) return ij
        val bags = JsonUtil.parseObj(ij)?.optJSONArray("bags") ?: return JsonUtil.wrap("items", JSONArray())
        val items = JSONArray()
        for (b in 0 until bags.length()) {
            val bag = bags.optJSONObject(b) ?: continue
            val bagItems = bag.optJSONArray("items") ?: continue
            for (i in 0 until bagItems.length()) {
                val it = bagItems.optJSONObject(i) ?: continue
                it.put("bag", bag.optInt("bag", -1))
                injectItemName(it)
                items.put(it)
            }
        }
        return JsonUtil.wrap("items", items)
    }

    override fun bagInfo(bag: Int): String {
        val ij = inventoryJson()
        if (isNativeError(ij)) return ij
        val bags = JsonUtil.parseObj(ij)?.optJSONArray("bags") ?: return JsonUtil.NOT_FOUND
        for (b in 0 until bags.length()) {
            val o = bags.optJSONObject(b) ?: continue
            if (o.optInt("bag", -1) == bag) {
                return JsonUtil.wrap("bag" to o.optInt("bag", -1),
                    "capacity" to o.optInt("capacity", -1),
                    "slot_count" to o.optInt("slot_count", -1))
            }
        }
        return JsonUtil.NOT_FOUND
    }

    override fun bagSlot(bag: Int, slot: Int): String {
        val ij = inventoryJson()
        if (isNativeError(ij)) return ij
        val bags = JsonUtil.parseObj(ij)?.optJSONArray("bags") ?: return JsonUtil.NOT_FOUND
        for (b in 0 until bags.length()) {
            val o = bags.optJSONObject(b) ?: continue
            if (o.optInt("bag", -1) != bag) continue
            val items = o.optJSONArray("items") ?: return JsonUtil.NOT_FOUND
            for (i in 0 until items.length()) {
                val it = items.optJSONObject(i) ?: continue
                if (it.optInt("slot", -1) == slot) {
                    injectItemName(it)
                    return it.toString()
                }
            }
            return "null"
        }
        return JsonUtil.NOT_FOUND
    }

    override fun quest(): String {
        // v0.5.13：与细分端点保持一致——active/list/completed 分别取 questActive/questList/questCompleted 的 quests 数组
        // 非 world 状态下 native 返回 {"error":...}——复合端点诚实转发首个错误，不伪造空数组
        val a = questActive()
        if (isNativeError(a)) return a
        val l = questList()
        if (isNativeError(l)) return l
        val c = questCompleted()
        if (isNativeError(c)) return c
        val active = JsonUtil.parseObj(a)?.optJSONArray("quests")
        val list = JsonUtil.parseObj(l)?.optJSONArray("quests")
        val completed = JsonUtil.parseObj(c)?.optJSONArray("quests")
        return JsonUtil.wrap(
            "active" to (active ?: JSONArray()),
            "list" to (list ?: JSONArray()),
            "completed" to (completed ?: JSONArray())
        )
    }

    override fun questActive(): String {
        val json = NativeBridge.nativeQuestActive()
        if (isNativeError(json)) return json
        return try {
            val root = JSONObject(json)
            val arr = root.optJSONArray("quests") ?: return json
            for (i in 0 until arr.length()) {
                val q = arr.optJSONObject(i) ?: continue
                val qid = q.optInt("quest_id", -1)
                if (qid < 0) continue
                val data = StaticData.questData(qid) ?: continue
                val name = data.optString("name").takeIf { it.isNotEmpty() }
                if (name != null) q.put("id_name", name)
                injectQuestFields(q, data, listOf("group_id", "name", "detail", "is_side", "is_mainline"))
            }
            root.toString()
        } catch (e: Exception) {
            json
        }
    }

    override fun questList(): String {
        val json = NativeBridge.nativeQuestList()
        if (isNativeError(json)) return json
        return try {
            val root = JSONObject(json)
            val arr = root.optJSONArray("quests") ?: return json
            for (i in 0 until arr.length()) {
                val q = arr.optJSONObject(i) ?: continue
                val qid = q.optInt("quest_id", -1)
                if (qid < 0) continue
                val data = StaticData.questData(qid) ?: continue
                injectQuestFields(
                    q, data,
                    listOf(
                        "group_id", "group_name", "name", "detail", "accepted_dialog",
                        "delivered_dialog", "class_req", "reward_hint", "side_flag",
                        "is_side", "is_mainline", "hidden"
                    )
                )
                val rewards = data.optJSONArray("rewards")
                if (rewards != null) q.put("rewards", rewards)
            }
            root.toString()
        } catch (e: Exception) {
            json
        }
    }

    override fun questListId(id: Int): String = JsonUtil.NOT_FOUND

    override fun questCompleted(): String {
        val json = NativeBridge.nativeQuestCompleted()
        if (isNativeError(json)) return json
        return try {
            val root = JSONObject(json)
            val arr = root.optJSONArray("quests") ?: return json
            for (i in 0 until arr.length()) {
                val q = arr.optJSONObject(i) ?: continue
                val qid = q.optInt("quest_id", -1)
                if (qid < 0) continue
                val data = StaticData.questData(qid) ?: continue
                val name = data.optString("name").takeIf { it.isNotEmpty() }
                if (name != null) q.put("name", name)
                injectQuestFields(q, data, listOf("group_id", "detail", "is_side", "is_mainline"))
            }
            root.toString()
        } catch (e: Exception) {
            json
        }
    }

    /** 从 QUESTS.json 解析产物注入字段（v0.5.37） */
    private fun injectQuestFields(target: JSONObject, data: JSONObject, keys: List<String>) {
        for (key in keys) {
            if (data.has(key)) target.put(key, data.get(key))
        }
    }

    override fun ui(): String = gamestateJson()

    override fun uiScreen(): String = JsonUtil.wrap("screen", screenName())

    override fun uiPanel(): String {
        val s = screenName()
        val panel = if (s in PANELS) s else null
        return JsonUtil.wrap("panel", panel)
    }

    override fun uiDialog(): String {
        // v0.5.13：native 完整检测（popup/story/npc/wipeout/npc_quest/面板态），与 gamestate 的
        // dialog_active/dialog 同源（data_dialog_content_json），一体同步；补齐 active 字段（type!=none）
        val json = NativeBridge.nativeDialogContent()
        if (isNativeError(json)) return json
        return try {
            val obj = JSONObject(json)
            if (!obj.has("active")) {
                obj.put("active", obj.optString("type", "none") != "none")
            }
            obj.toString()
        } catch (e: Exception) {
            json
        }
    }

    override fun game(): String {
        return JsonUtil.wrap("snapshot" to JsonUtil.parseObj(snapshotJson()), "info" to JsonUtil.parseObj(gameInfo()))
    }

    override fun gameSnapshot(): String = withItemNames(snapshotJson())

    override fun gameFrame(): String = JsonUtil.wrap("frame", NativeBridge.nativeGetFrameCount())

    override fun gameInfo(): String {
        val slots: Any? = JsonUtil.parseObj(NativeBridge.nativeSaveSlotsJson())?.opt("slots")
        val currentSlot = JsonUtil.parseObj(NativeBridge.nativeCurrentSaveSlot())?.optInt("current_save_slot", -1) ?: -1
        return JsonUtil.wrap(
            "version" to BuildConfig.VERSION_NAME,
            "game" to screenName(),
            "logged_in" to null,
            "save_slots" to slots,
            "current_save_slot" to currentSlot,
            "package_name" to PKG_NAME,
            "base" to NativeBridge.nativeGetBaseAddr()
        )
    }

    override fun events(since: Long?): String = NativeBridge.nativeGetEventsJson()

    override fun npcDialogOptions(): String {
        val json = NativeBridge.nativeNpcDialogOptions()
        if (isNativeError(json)) return json
        return json
    }

    override fun shopItems(): String {
        val json = NativeBridge.nativeShopItems()
        if (isNativeError(json)) return json
        return try {
            val root = JSONObject(json)
            val arr = root.optJSONArray("items") ?: return json
            for (i in 0 until arr.length()) {
                val it = arr.optJSONObject(i) ?: continue
                val category = it.optInt("category", -1)
                if (category >= 0) StaticData.itemName(category)?.let { n -> it.put("name", n) }
            }
            root.toString()
        } catch (e: Exception) {
            json
        }
    }

    override fun health(): String = JsonUtil.wrap("ok" to true)

    override fun exportSaveFile(slot: Int): String {
        if (slot < 0 || slot > 2) return """{"error":"slot must be 0-2"}"""
        val dataDir = StaticData.dataDir() ?: return """{"error":"data dir unavailable"}"""
        // 存档路径 /data/data/<pkg>/<uid 哈希目录>/save{slot}.dat（目录名随 UID 变化，扫描定位）
        val dirs = File(dataDir).listFiles() ?: return """{"error":"save file not found"}"""
        for (d in dirs) {
            if (!d.isDirectory) continue
            val f = File(d, "save$slot.dat")
            if (!f.isFile) continue
            return try {
                val bytes = f.readBytes()
                val root = JSONObject()
                root.put("ok", true)
                root.put("slot", slot)
                root.put("path", f.absolutePath)
                root.put("size", bytes.size)
                root.put("name", f.name)
                root.put("content", android.util.Base64.encodeToString(bytes, android.util.Base64.NO_WRAP))
                root.toString()
            } catch (e: Exception) {
                LogFile.logError("exportSaveFile failed", e)
                """{"error":"read failed: ${e.message}"}"""
            }
        }
        return """{"error":"save file not found"}"""
    }

    /**
     * native 数据函数在非 world 状态（主菜单等）下返回 {"error":"..."}（与写操作 game_in_world() 一致）。
     * Kotlin 侧不自判游戏状态，只诚实转发：顶层含 "error" 字段 → 原样返回。顶层判定避免嵌套字段误报。
     */
    private fun isNativeError(json: String): Boolean = JsonUtil.parseObj(json)?.has("error") == true

    private fun playerJson(): String = NativeBridge.nativeGetPlayerJson()

    private fun partyJson(): String = NativeBridge.nativeGetPartyJson()

    private fun partyArr(): JSONArray? = JsonUtil.parseArr(partyJson())

    private fun inventoryJson(): String = NativeBridge.nativeGetInventoryJson()

    private fun mapJson(): String = NativeBridge.nativeGetMapJson()

    private fun unitsJson(): String = NativeBridge.nativeGetUnitsJson()

    private fun enemiesJson(): String = NativeBridge.nativeGetEnemiesJson()

    private fun interactivesJson(): String = NativeBridge.nativeGetInteractivesJson()

    private fun dropsJson(): String = NativeBridge.nativeGetDropsJson()

    private fun gamestateJson(): String = NativeBridge.nativeGetGamestateJson()

    private fun snapshotJson(): String = NativeBridge.nativeGetSnapshotJson()

    private fun skillsJson(): String = NativeBridge.nativeGetSkillsJson()

    private fun skillsArr(): JSONArray? = JsonUtil.parseArr(skillsJson())

    private fun mercenariesJson(): String = NativeBridge.nativeGetMercenariesJson()

    private fun memberObj(slot: Int): JSONObject? = partyArr()?.optJSONObject(slot)

    private fun memberInt(slot: Int, key: String): String {
        val m = memberObj(slot) ?: return JsonUtil.NOT_FOUND
        if (!m.has(key)) return JsonUtil.NOT_FOUND
        return JsonUtil.wrap(key, m.optInt(key, -1))
    }

    private fun memberString(slot: Int, key: String): String {
        val m = memberObj(slot) ?: return JsonUtil.NOT_FOUND
        if (!m.has(key)) return JsonUtil.NOT_FOUND
        return JsonUtil.wrap(key, m.optString(key, ""))
    }

    private fun filterUnits(units: JSONArray, type: Int): JSONArray {
        val arr = JSONArray()
        for (i in 0 until units.length()) {
            val u = units.optJSONObject(i) ?: continue
            if (u.optInt("type", -1) == type) arr.put(u)
        }
        return arr
    }

    private fun attachMapStatic(mapId: Int): JSONObject? {
        if (mapId < 0) return null
        val tables = JsonUtil.parseObj(StaticData.read("tables/MAPINFOBASE.json")) ?: return null
        val records = tables.optJSONArray("records") ?: return null
        if (mapId >= records.length()) return null
        val r = records.optJSONObject(mapId) ?: return null
        val out = JSONObject()
        out.put("text_id", r.optJSONArray("u16")?.optInt(0, -1) ?: -1)
        out.put("name", r.optString("text_0", ""))
        r.optJSONArray("u16")?.let { out.put("u16", it) }
        r.optString("hex", "")?.let { if (it.isNotEmpty()) out.put("hex", it) }
        return out
    }

    private fun screenName(): String =
        JsonUtil.parseObj(gamestateJson())?.optString("screen", "loading") ?: "loading"

    private fun withItemNames(json: String): String {
        return try {
            val trimmed = json.trimStart()
            if (trimmed.startsWith("[")) {
                val arr = JSONArray(json)
                for (i in 0 until arr.length()) {
                    val role = arr.optJSONObject(i) ?: continue
                    injectAttrNames(role)
                    injectEquipmentNames(role)
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

    private fun injectEquipmentNames(role: JSONObject) {
        val eq = role.optJSONArray("equipment") ?: return
        for (e in 0 until eq.length()) {
            eq.optJSONObject(e)?.let { injectItemName(it, true) }
        }
    }

    private fun injectItemName(item: JSONObject, equipOverride: Boolean? = null) {
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
    private fun injectItemOptions(item: JSONObject) {
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
    private fun injectSocketEnchant(item: JSONObject) {
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

    private fun injectAttrNames(role: JSONObject) {
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

    companion object {
        private const val PKG_NAME =
            "com.com2us.inotia4.normal.freefull.google.global.android.common"

        private val PANELS = setOf(
            "character_info", "inventory", "skills", "mercenary", "quests", "settings",
            "shop", "craft", "npc", "npc_quest", "npc_rest", "npc_revive", "save_slot",
            "character_select", "options", "shortcut", "world_map", "input_count", "choice",
            "wipeout", "daily_reward", "in_app", "ui_panel"
        )
    }
}
