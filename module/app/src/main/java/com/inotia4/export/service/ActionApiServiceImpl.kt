package com.inotia4.export.service

import com.inotia4.export.NativeBridge
import com.inotia4.export.util.JsonUtil
import org.json.JSONObject

/**
 * 合法操作服务实现（ActionApiService 接口的唯一实现，v0.4.0 迁移自 PlayerController 操作编排）。
 * 操作调用 + 快照 attach 全部在此，controller 只做路由与参数解析。
 */
class ActionApiServiceImpl : ActionApiService {

    override fun move(x: Int, y: Int): String = attachPlayer(NativeBridge.nativeOpMove(x, y))

    override fun walk(direction: Int): String = attachPlayer(NativeBridge.nativeOpWalk(direction))

    override fun walkStop(): String = NativeBridge.nativeOpWalkStop()

    override fun useItem(bag: Int, slot: Int): String = attachInventory(NativeBridge.nativeOpUseItem(bag, slot))

    override fun diceAccept(): String = attachPlayer(NativeBridge.nativeOpDiceAccept())

    override fun diceReject(): String = NativeBridge.nativeOpDiceReject()

    override fun sellItem(bag: Int, slot: Int): String = attachInventory(NativeBridge.nativeOpSellItem(bag, slot))

    override fun moveItem(bag: Int, slot: Int, count: Int, toBag: Int, toSlot: Int): String =
        attachInventory(NativeBridge.nativeOpMoveItem(bag, slot, count, toBag, toSlot))

    override fun equip(role: Int, bag: Int, slot: Int): String = attachParty(NativeBridge.nativeOpEquip(role, bag, slot))

    override fun equipByCategory(role: Int, category: Int): String {
        val pos = findItemSlot(category) ?: return """{"ok":false,"error":"item not found"}"""
        return attachParty(NativeBridge.nativeOpEquip(role, pos.first, pos.second))
    }

    override fun unequip(role: Int, slot: Int): String = attachParty(NativeBridge.nativeOpUnequip(role, slot))

    override fun autoAttack(role: Int, on: Boolean): String =
        attachParty(NativeBridge.nativeOpSetAutoAttack(role, if (on) 1 else 0))

    override fun skillUsage(role: Int, on: Boolean): String =
        attachParty(NativeBridge.nativeOpSetSkillUsage(role, if (on) 1 else 0))

    override fun learnSkill(role: Int, actionId: Int, level: Int): String =
        attachSkills(NativeBridge.nativeOpLearnAction(role, actionId, level))

    override fun addStat(role: Int, attr: Int): String =
        attachPlayer(NativeBridge.nativeOpAddStat(role, attr))

    override fun statReset(role: Int): String = attachPlayer(NativeBridge.nativeOpStatReset(role))

    override fun skillReset(role: Int): String = attachPlayer(NativeBridge.nativeOpSkillReset(role))

    override fun cast(role: Int, actionId: Int): String =
        attachParty(NativeBridge.nativeOpCast(role, actionId))

    override fun questQuit(questId: Int): String = attachPlayer(NativeBridge.nativeOpQuestQuit(questId))

    override fun save(): String = attachPlayer(NativeBridge.nativeOpSave())

    override fun mainMenu(): String = attachPlayer(NativeBridge.nativeOpMainMenu())

    override fun enterSlot(slot: Int): String = attachPlayer(NativeBridge.nativeOpEnterSlot(slot))

    override fun npcInteract(): String = NativeBridge.nativeOpNpcInteract()

    override fun npcDialogNext(): String = NativeBridge.nativeOpNpcDialogNext()

    override fun npcDialogSelect(index: Int): String = NativeBridge.nativeOpNpcDialogSelect(index)

    override fun dialogSelect(action: String, index: Int): String =
        NativeBridge.nativeOpDialogSelect(action, index)

    override fun shopBuy(slot: Int): String = attachInventory(NativeBridge.nativeOpShopBuy(slot))

    override fun jewel(role: Int, bag: Int, slot: Int, equipSlot: Int): String =
        attachParty(NativeBridge.nativeOpJewel(role, bag, slot, equipSlot))

    override fun switchPlayer(slot: Int): String = attachPlayer(NativeBridge.nativeOpSwitchPlayer(slot))

    override fun discardItem(bag: Int, slot: Int): String = attachInventory(NativeBridge.nativeOpDiscardItem(bag, slot))

    override fun includeParty(mercenarySlot: Int): String = attachParty(NativeBridge.nativeOpIncludeParty(mercenarySlot))

    override fun excludeParty(mercenarySlot: Int): String = attachParty(NativeBridge.nativeOpExcludeParty(mercenarySlot))

    override fun discharge(mercenarySlot: Int): String = attachParty(NativeBridge.nativeOpDischarge(mercenarySlot))

    override fun withdraw(mercenarySlot: Int, equipSlot: Int): String =
        attachParty(NativeBridge.nativeOpWithdraw(mercenarySlot, equipSlot))

    override fun dialogOk(): String = NativeBridge.nativeOpDialogOk()

    override fun dialogCancel(): String = NativeBridge.nativeOpDialogCancel()

    override fun getPath(tx: Int, ty: Int): String = NativeBridge.nativeGetPathJson(tx, ty)

    override fun attack(role: Int, targetSlot: Int): String =
        attachParty(NativeBridge.nativeOpAttack(role, targetSlot))

    override fun stopCombat(role: Int): String =
        attach(NativeBridge.nativeOpStopCombat(role)) { NativeBridge.nativeGetPlayerJson() }

    private fun attachPlayer(op: String): String =
        attach(op) { NativeBridge.nativeGetPlayerJson() }

    private fun attachParty(op: String): String =
        attach(op) { NativeBridge.nativeGetPartyJson() }

    private fun attachInventory(op: String): String =
        attach(op) { NativeBridge.nativeGetInventoryJson() }

    private fun attachSkills(op: String): String =
        attach(op) { NativeBridge.nativeGetSkillsJson() }

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
