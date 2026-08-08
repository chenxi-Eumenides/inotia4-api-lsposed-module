package com.inotia4.export.service

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.StaticData
import com.inotia4.export.util.JsonUtil
import org.json.JSONArray
import org.json.JSONObject

/**
 * 信息端点提取层：从 native 复合 JSON 中提取简单端点字段（api-spec §0 新分层）。
 * controller 只做路由，字段提取与名称注入统一在此。native 未就绪返回 503 语义串。
 */
object InfoService {

    fun ready(): Boolean = NativeBridge.ready

    fun currentMap(): String {
        val root = JSONObject()
        JsonUtil.parseObj(mapJson())?.let { m ->
            root.put("mapId", m.optInt("mapId", -1))
            root.put("x", m.optInt("x", -1))
            root.put("y", m.optInt("y", -1))
            m.optJSONObject("tile")?.let { root.put("tile", it) }
        }
        JsonUtil.parseObj(unitsJson())?.optJSONArray("units")?.let { root.put("units", it) }
        root.put("enemies", JsonUtil.parseObj(filterUnits(2))?.optJSONArray("units") ?: JSONArray())
        root.put("interactives", JsonUtil.parseObj(filterUnits(1))?.optJSONArray("units") ?: JSONArray())
        root.put("drops", JSONArray())
        return root.toString()
    }

    fun currentMapId(): String = JsonUtil.wrap("mapId", JsonUtil.parseObj(mapJson())?.optInt("mapId", -1) ?: -1)

    fun currentMapTile(): String {
        val tile = JsonUtil.parseObj(mapJson())?.optJSONObject("tile") ?: return "{}"
        return JsonUtil.wrap("tile", tile)
    }

    fun currentMapUnits(): String = unitsJson()

    fun currentMapEnemies(): String = filterUnits(2)

    fun currentMapInteractives(): String = filterUnits(1)

    fun currentMapDrops(): String = """{"drops":[]}"""


    fun party(): String = withItemNames(partyJson())

    fun partyCount(): String =
        JsonUtil.wrap("count", JsonUtil.parseObj(playerJson())?.optInt("partyCount", -1) ?: -1)

    fun partyLeader(): String {
        val p = partyArr() ?: return JsonUtil.NOT_FOUND
        val leaderSlot = JsonUtil.parseObj(playerJson())?.optInt("mainMercenarySlot", 0) ?: 0
        val m = if (leaderSlot in 0 until p.length()) p.optJSONObject(leaderSlot) else null
        return m?.let { withItemNames(it.toString()) } ?: JsonUtil.NOT_FOUND
    }

    fun partyMember(slot: Int): String {
        val p = partyArr() ?: return JsonUtil.NOT_FOUND
        val m = p.optJSONObject(slot) ?: return JsonUtil.NOT_FOUND
        return withItemNames(m.toString())
    }

    fun partyMemberId(slot: Int): String = memberInt(slot, "type")

    fun partyMemberName(slot: Int): String = memberString(slot, "name")

    fun partyMemberLevel(slot: Int): String = memberInt(slot, "level")

    fun partyMemberExp(slot: Int): String = memberField(slot, "exp", "expNext")

    fun partyMemberHp(slot: Int): String = memberField(slot, "hp", "maxHp")

    fun partyMemberMp(slot: Int): String = memberField(slot, "mp", "maxMp")

    fun partyMemberStats(slot: Int): String = JsonUtil.wrap("stats", memberObj(slot)?.optJSONObject("stats"))

    fun partyMemberStat(slot: Int, attr: Int): String {
        val stats = memberObj(slot)?.optJSONObject("stats") ?: return JsonUtil.NOT_FOUND
        val v = stats.optInt(attr.toString(), -1)
        return JsonUtil.wrap("attr" to attr, "value" to v)
    }

    fun partyMemberEquipment(slot: Int): String {
        val eq = memberObj(slot)?.optJSONArray("equipment") ?: return JsonUtil.NOT_FOUND
        val arr = JSONArray()
        for (i in 0 until eq.length()) {
            eq.optJSONObject(i)?.let { injectItemName(it); arr.put(it) }
        }
        return JsonUtil.wrap("equipment", arr)
    }

    fun partyMemberEquip(slot: Int, equipSlot: Int): String {
        val eq = memberObj(slot)?.optJSONArray("equipment") ?: return JsonUtil.NOT_FOUND
        val it = eq.optJSONObject(equipSlot) ?: return JsonUtil.NOT_FOUND
        injectItemName(it)
        return it.toString()
    }

    fun partyMemberSkills(slot: Int): String {
        val s = skillsArr() ?: return JsonUtil.NOT_FOUND
        return s.optJSONObject(slot)?.toString() ?: JsonUtil.NOT_FOUND
    }

    fun partyMemberSkillList(slot: Int): String {
        val s = skillsArr() ?: return JsonUtil.NOT_FOUND
        val skills = s.optJSONObject(slot)?.optJSONArray("skills") ?: return JsonUtil.NOT_FOUND
        return JsonUtil.wrap("skills", skills)
    }


    fun mercenary(): String = mercenariesJson()

    fun mercenaryList(): String {
        val arr = JsonUtil.parseArr(mercenariesJson()) ?: return JsonUtil.wrap("slots", JSONArray())
        val slots = JSONArray()
        for (i in 0 until arr.length()) {
            arr.optJSONObject(i)?.optInt("slot", -1)?.takeIf { it >= 0 }?.let { slots.put(it) }
        }
        return JsonUtil.wrap("slots", slots)
    }

    fun mercenarySlot(slot: Int): String {
        val arr = JsonUtil.parseArr(mercenariesJson()) ?: return JsonUtil.NOT_FOUND
        for (i in 0 until arr.length()) {
            val m = arr.optJSONObject(i) ?: continue
            if (m.optInt("slot", -1) == slot) return m.toString()
        }
        return JsonUtil.NOT_FOUND
    }


    fun inventory(): String = withItemNames(inventoryJson())

    fun inventoryMoney(): String =
        JsonUtil.wrap("money", JsonUtil.parseObj(playerJson())?.optLong("money", -1) ?: -1L)

    fun inventoryItems(): String {
        val bags = JsonUtil.parseObj(inventoryJson())?.optJSONArray("bags") ?: return JsonUtil.wrap("items", JSONArray())
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

    fun bagInfo(bag: Int): String {
        val bags = JsonUtil.parseObj(inventoryJson())?.optJSONArray("bags") ?: return JsonUtil.NOT_FOUND
        for (b in 0 until bags.length()) {
            val o = bags.optJSONObject(b) ?: continue
            if (o.optInt("bag", -1) == bag) {
                return JsonUtil.wrap("bag" to o.optInt("bag", -1),
                    "capacity" to o.optInt("capacity", -1),
                    "slotCount" to o.optInt("slotCount", -1))
            }
        }
        return JsonUtil.NOT_FOUND
    }

    fun bagSlot(bag: Int, slot: Int): String {
        val bags = JsonUtil.parseObj(inventoryJson())?.optJSONArray("bags") ?: return JsonUtil.NOT_FOUND
        for (b in 0 until bags.length()) {
            val o = bags.optJSONObject(b) ?: continue
            if (o.optInt("bag", -1) != bag) continue
            val it = o.optJSONArray("items")?.optJSONObject(slot) ?: return JsonUtil.NOT_FOUND
            injectItemName(it)
            return it.toString()
        }
        return JsonUtil.NOT_FOUND
    }


    fun quest(): String {
        val active = JsonUtil.parseObj(playerJson())?.optInt("activeQuest", -1) ?: -1
        return JsonUtil.wrap("active" to active, "list" to JSONArray(), "completed" to JSONArray())
    }

    fun questActive(): String = JsonUtil.wrap("activeQuest", nativeActiveQuest())

    fun questList(): String = """{"quests":[]}"""

    fun questListId(id: Int): String = JsonUtil.NOT_FOUND

    fun questCompleted(): String = """{"quests":[]}"""


    fun ui(): String = gamestateJson()

    fun uiScreen(): String = JsonUtil.wrap("screen", screenName())

    fun uiPanel(): String {
        val s = screenName()
        val panel = if (s in PANELS) s else null
        return JsonUtil.wrap("panel", panel)
    }

    fun uiDialog(): String {
        val g = JsonUtil.parseObj(gamestateJson()) ?: return "{}"
        return JsonUtil.wrap("active" to g.optBoolean("dialogActive", false),
            "dialog" to g.optJSONObject("dialog"))
    }

    fun uiDialogActive(): String = JsonUtil.wrap("active", dialogObj()?.optBoolean("active", false) ?: false)

    fun uiDialogText(): String = JsonUtil.wrap("text", dialogInner()?.optString("text", ""))

    fun uiDialogButtons(): String = JsonUtil.wrap("buttons", dialogInner()?.optJSONArray("buttons") ?: JSONArray())

    fun uiDialogOk(): String = JsonUtil.wrap("hasOk", dialogInner()?.optBoolean("hasOk", false) ?: false)

    fun uiDialogCancel(): String = JsonUtil.wrap("hasCancel", dialogInner()?.optBoolean("hasCancel", false) ?: false)


    fun game(): String {
        return JsonUtil.wrap("snapshot" to JsonUtil.parseObj(snapshotJson()), "info" to JsonUtil.parseObj(gameInfo()))
    }

    fun gameSnapshot(): String = withItemNames(snapshotJson())

    fun gameInfo(): String = JsonUtil.wrap(
        "version" to MODULE_VERSION,
        "loggedIn" to null,
        "saveSlots" to JSONArray(),
        "packageName" to PKG_NAME,
        "base" to NativeBridge.nativeGetBaseAddr()
    )


    fun events(since: Long?): String = NativeBridge.nativeGetEventsJson()

    fun health(): String = JsonUtil.wrap(
        "ok" to true,
        "version" to MODULE_VERSION,
        "game" to screenName(),
        "base" to NativeBridge.nativeGetBaseAddr()
    )


    private fun nativeActiveQuest(): Int = NativeBridge.nativeGetActiveQuest()

    private fun playerJson(): String = NativeBridge.nativeGetPlayerJson()

    private fun partyJson(): String = NativeBridge.nativeGetPartyJson()

    private fun partyArr(): JSONArray? = JsonUtil.parseArr(partyJson())

    private fun inventoryJson(): String = NativeBridge.nativeGetInventoryJson()

    private fun mapJson(): String = NativeBridge.nativeGetMapJson()

    private fun unitsJson(): String = NativeBridge.nativeGetUnitsJson()

    private fun gamestateJson(): String = NativeBridge.nativeGetGamestateJson()

    private fun snapshotJson(): String = NativeBridge.nativeGetSnapshotJson()

    private fun skillsArr(): JSONArray? = JsonUtil.parseArr(NativeBridge.nativeGetSkillsJson())

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

    private fun memberField(slot: Int, key: String, key2: String): String {
        val m = memberObj(slot) ?: return JsonUtil.NOT_FOUND
        if (!m.has(key)) return JsonUtil.NOT_FOUND
        return JsonUtil.wrap(key to m.opt(key), key2 to m.opt(key2))
    }

    private fun filterUnits(status: Int): String {
        val units = JsonUtil.parseObj(unitsJson())?.optJSONArray("units") ?: return JsonUtil.wrap("units", JSONArray())
        val arr = JSONArray()
        for (i in 0 until units.length()) {
            val u = units.optJSONObject(i) ?: continue
            if (u.optInt("status", -1) == status) arr.put(u)
        }
        return JsonUtil.wrap("units", arr)
    }

    private fun screenName(): String =
        JsonUtil.parseObj(gamestateJson())?.optString("screen", "loading") ?: "loading"

    private fun dialogObj(): JSONObject? {
        val g = JsonUtil.parseObj(gamestateJson()) ?: return null
        if (!g.optBoolean("dialogActive", false)) return null
        return g.optJSONObject("dialog")
    }

    private fun dialogInner(): JSONObject? = dialogObj()


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
            eq.optJSONObject(e)?.let { injectItemName(it) }
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
                attrs.put(JSONObject().put("id", i).put("name", mainNames[i]).put("value", mainStats.optInt(i)))
            }
        }
        role.optInt("statusPoint", -1).takeIf { it >= 0 }?.let {
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

    private const val MODULE_VERSION = "0.3.14"

    private const val PKG_NAME =
        "com.com2us.inotia4.normal.freefull.google.global.android.common"

    private val PANELS = setOf(
        "character_info", "inventory", "skills", "mercenary", "quests", "settings",
        "shop", "craft", "npc", "npc_quest", "npc_rest", "npc_revive", "save_slot",
        "character_select", "options", "shortcut", "world_map", "input_count", "choice",
        "wipeout", "daily_reward", "in_app", "ui_panel"
    )
}
