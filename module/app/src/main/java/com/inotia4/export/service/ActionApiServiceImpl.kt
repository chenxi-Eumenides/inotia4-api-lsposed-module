package com.inotia4.export.service

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.util.JsonUtil
import org.json.JSONObject

/**
 * 合法操作服务实现（ActionApiService 接口的唯一实现，v0.4.0 迁移自 PlayerController 操作编排）。
 * 操作调用 + 快照 attach 全部在此，controller 只做路由与参数解析。
 */
class ActionApiServiceImpl : ActionApiService {

    override fun move(x: Int, y: Int): String = logged("move", "x=$x,y=$y") { attachPlayer(NativeBridge.nativeOpMove(x, y)) }

    override fun walk(direction: Int): String = logged("walk", "dir=$direction") { attachPlayer(NativeBridge.nativeOpWalk(direction)) }

    override fun walkStop(): String = logged("walkStop", "") { NativeBridge.nativeOpWalkStop() }

    override fun interact(): String = logged("interact", "") { attachPlayer(NativeBridge.nativeOpInteract()) }

    override fun useItem(bag: Int, slot: Int): String = logged("useItem", "bag=$bag,slot=$slot") { attachInventory(NativeBridge.nativeOpUseItem(bag, slot)) }

    override fun diceAccept(): String = logged("diceAccept", "") { attachPlayer(NativeBridge.nativeOpDiceAccept()) }

    override fun diceReject(): String = logged("diceReject", "") { NativeBridge.nativeOpDiceReject() }

    override fun sellItem(bag: Int, slot: Int): String = logged("sellItem", "bag=$bag,slot=$slot") { attachInventory(NativeBridge.nativeOpSellItem(bag, slot)) }

    override fun moveItem(bag: Int, slot: Int, count: Int, toBag: Int, toSlot: Int): String =
        logged("moveItem", "bag=$bag,slot=$slot,count=$count,toBag=$toBag,toSlot=$toSlot") { attachInventory(NativeBridge.nativeOpMoveItem(bag, slot, count, toBag, toSlot)) }

    override fun equip(role: Int, bag: Int, slot: Int): String = logged("equip", "role=$role,bag=$bag,slot=$slot") { attachParty(NativeBridge.nativeOpEquip(role, bag, slot)) }

    override fun equipByCategory(role: Int, category: Int): String = logged("equipByCategory", "role=$role,category=$category") {
        val pos = findItemSlot(category) ?: return@logged """{"ok":false,"error":"item not found"}"""
        attachParty(NativeBridge.nativeOpEquip(role, pos.first, pos.second))
    }

    override fun unequip(role: Int, slot: Int): String = logged("unequip", "role=$role,slot=$slot") { attachParty(NativeBridge.nativeOpUnequip(role, slot)) }

    override fun autoAttack(role: Int, on: Boolean): String =
        logged("autoAttack", "role=$role,on=$on") { attachParty(NativeBridge.nativeOpSetAutoAttack(role, if (on) 1 else 0)) }

    override fun skillUsage(role: Int, on: Boolean): String =
        logged("skillUsage", "role=$role,on=$on") { attachParty(NativeBridge.nativeOpSetSkillUsage(role, if (on) 1 else 0)) }

    override fun learnSkill(role: Int, actionId: Int, level: Int): String =
        logged("learnSkill", "role=$role,actionId=$actionId,level=$level") { attachSkills(NativeBridge.nativeOpLearnAction(role, actionId, level)) }

    override fun addStat(role: Int, attr: Int): String =
        logged("addStat", "role=$role,attr=$attr") { attachPlayer(NativeBridge.nativeOpAddStat(role, attr)) }

    override fun statReset(role: Int): String = logged("statReset", "role=$role") { attachPlayer(NativeBridge.nativeOpStatReset(role)) }

    override fun skillReset(role: Int): String = logged("skillReset", "role=$role") { attachPlayer(NativeBridge.nativeOpSkillReset(role)) }

    override fun cast(role: Int, actionId: Int): String =
        logged("cast", "role=$role,actionId=$actionId") { attachParty(NativeBridge.nativeOpCast(role, actionId)) }

    override fun questQuit(questId: Int): String = logged("questQuit", "questId=$questId") { attachPlayer(NativeBridge.nativeOpQuestQuit(questId)) }

    override fun save(): String = logged("save", "") { attachPlayer(NativeBridge.nativeOpSave()) }

    override fun mainMenu(): String = logged("mainMenu", "") { attachPlayer(NativeBridge.nativeOpMainMenu()) }

    override fun enterSlot(slot: Int): String = logged("enterSlot", "slot=$slot") { attachPlayer(NativeBridge.nativeOpEnterSlot(slot)) }

    override fun createSlot(slot: Int, classIdx: Int): String =
        logged("createSlot", "slot=$slot,classIdx=$classIdx") { attachPlayer(NativeBridge.nativeOpCreateSlot(slot, classIdx)) }

    override fun panelClose(): String = logged("panelClose", "") { attachUi(NativeBridge.nativeOpPanelClose()) }

    override fun panelOpen(panel: String): String = logged("panelOpen", "panel=$panel") { attachUi(NativeBridge.nativeOpPanelOpen(panel)) }

    override fun npcInteract(): String = logged("npcInteract", "") { NativeBridge.nativeOpNpcInteract() }

    override fun npcDialogNext(): String = logged("npcDialogNext", "") { NativeBridge.nativeOpNpcDialogNext() }

    override fun npcDialogSelect(index: Int): String = logged("npcDialogSelect", "index=$index") { NativeBridge.nativeOpNpcDialogSelect(index) }

    override fun dialogSelect(action: String, index: Int): String =
        logged("dialogSelect", "action=$action,index=$index") { NativeBridge.nativeOpDialogSelect(action, index) }

    override fun shopBuy(slot: Int): String = logged("shopBuy", "slot=$slot") { attachInventory(NativeBridge.nativeOpShopBuy(slot)) }

    override fun jewel(role: Int, bag: Int, slot: Int, equipSlot: Int): String =
        logged("jewel", "role=$role,bag=$bag,slot=$slot,equipSlot=$equipSlot") { attachParty(NativeBridge.nativeOpJewel(role, bag, slot, equipSlot)) }

    override fun switchPlayer(slot: Int): String = logged("switchPlayer", "slot=$slot") { attachPlayer(NativeBridge.nativeOpSwitchPlayer(slot)) }

    override fun discardItem(bag: Int, slot: Int): String = logged("discardItem", "bag=$bag,slot=$slot") { attachInventory(NativeBridge.nativeOpDiscardItem(bag, slot)) }

    override fun includeParty(mercenarySlot: Int): String = logged("includeParty", "mercSlot=$mercenarySlot") { attachParty(NativeBridge.nativeOpIncludeParty(mercenarySlot)) }

    override fun excludeParty(mercenarySlot: Int): String = logged("excludeParty", "mercSlot=$mercenarySlot") { attachParty(NativeBridge.nativeOpExcludeParty(mercenarySlot)) }

    override fun discharge(mercenarySlot: Int): String = logged("discharge", "mercSlot=$mercenarySlot") { attachParty(NativeBridge.nativeOpDischarge(mercenarySlot)) }

    override fun withdraw(mercenarySlot: Int, equipSlot: Int): String =
        logged("withdraw", "mercSlot=$mercenarySlot,equipSlot=$equipSlot") { attachParty(NativeBridge.nativeOpWithdraw(mercenarySlot, equipSlot)) }

    override fun dialogOk(): String = logged("dialogOk", "") { NativeBridge.nativeOpDialogOk() }

    override fun dialogCancel(): String = logged("dialogCancel", "") { NativeBridge.nativeOpDialogCancel() }

    override fun getPath(tx: Int, ty: Int): String = logged("getPath", "tx=$tx,ty=$ty") { NativeBridge.nativeGetPathJson(tx, ty) }

    override fun attack(role: Int, targetSlot: Int): String =
        logged("attack", "role=$role,targetSlot=$targetSlot") { attachParty(NativeBridge.nativeOpAttack(role, targetSlot)) }

    override fun stopCombat(role: Int): String =
        logged("stopCombat", "role=$role") { attach(NativeBridge.nativeOpStopCombat(role)) { NativeBridge.nativeGetPlayerJson() } }

    private fun logged(name: String, params: String, block: () -> String): String {
        val t0 = System.nanoTime()
        val r = block()
        val ms = (System.nanoTime() - t0) / 1_000_000
        LogFile.log("svc.$name($params) -> ${r.take(300)} [${ms}ms]")
        return r
    }

    private fun attachPlayer(op: String): String =
        attach(op) { NativeBridge.nativeGetPlayerJson() }

    private fun attachParty(op: String): String =
        attach(op) { NativeBridge.nativeGetPartyJson() }

    private fun attachInventory(op: String): String =
        attach(op) { NativeBridge.nativeGetInventoryJson() }

    private fun attachSkills(op: String): String =
        attach(op) { NativeBridge.nativeGetSkillsJson() }

    private fun attachUi(op: String): String =
        attach(op) { NativeBridge.nativeGetGamestateJson() }

    private fun findItemSlot(category: Int): Pair<Int, Int>? {
        val inv = try {
            JSONObject(NativeBridge.nativeGetInventoryJson())
        } catch (e: Exception) {
            return null
        }
        val bags = inv.optJSONArray("bags") ?: return null
        for (b in 0 until bags.length()) {
            val bag = bags.getJSONObject(b)
            val items = bag.optJSONArray("items") ?: continue
            for (i in 0 until items.length()) {
                val item = items.getJSONObject(i)
                if (item.optInt("category", -1) == category) return b to item.optInt("slot", -1)
            }
        }
        return null
    }

    private fun attach(op: String, latest: () -> String): String {
        return try {
            val obj = JSONObject(op)
            if (obj.optBoolean("ok", false)) obj.put("state", JSONObject(latest()))
            obj.toString()
        } catch (e: Exception) {
            op
        }
    }
}
