package com.inotia4.export.service

// API 服务层接口（v0.4.0 P0-3 重构）

// API 服务层接口（v0.4.0 P0-3 重构）
interface InfoApiService {
    fun ready(): Boolean

    fun currentMap(): String
    fun currentMapId(): String
    fun currentMapTile(): String
    fun currentMapUnits(): String
    fun currentMapEnemies(): String
    fun currentMapInteractives(): String
    fun currentMapDrops(): String

    fun party(): String
    fun partyCount(): String
    fun partyLeader(): String
    fun partyMember(slot: Int): String
    fun partyMemberId(slot: Int): String
    fun partyMemberName(slot: Int): String
    fun partyMemberLevel(slot: Int): String
    fun partyMemberExp(slot: Int): String
    fun partyMemberHp(slot: Int): String
    fun partyMemberMp(slot: Int): String
    fun partyMemberStats(slot: Int): String
    fun partyMemberStat(slot: Int, attr: Int): String
    fun partyMemberEquipment(slot: Int): String
    fun partyMemberEquip(slot: Int, equipSlot: Int): String
    fun partyMemberSkills(slot: Int): String
    fun partyMemberSkillList(slot: Int): String

    fun mercenary(): String
    fun mercenaryList(): String
    fun mercenarySlot(slot: Int): String

    fun inventory(): String
    fun inventoryMoney(): String
    fun inventoryItems(): String
    fun bagInfo(bag: Int): String
    fun bagSlot(bag: Int, slot: Int): String

    fun quest(): String
    fun questActive(): String
    fun questList(): String
    fun questListId(id: Int): String
    fun questCompleted(): String

    fun ui(): String
    fun uiScreen(): String
    fun uiPanel(): String
    fun uiDialog(): String
    fun uiDialogActive(): String
    fun uiDialogText(): String
    fun uiDialogButtons(): String
    fun uiDialogOk(): String
    fun uiDialogCancel(): String

    fun game(): String
    fun gameSnapshot(): String
    fun gameInfo(): String

    fun events(since: Long?): String
    fun health(): String
    fun saveSlots(): String
    fun npcDialogOptions(): String
    fun shopItems(): String
}

// API 服务层接口（v0.4.0 P0-3 重构）
interface ActionApiService {
    fun move(x: Int, y: Int): String
    fun moveCancel(): String
    fun walk(direction: Int): String
    fun walkStop(): String
    fun useItem(bag: Int, slot: Int): String
    fun sellItem(bag: Int, slot: Int): String
    fun moveItem(bag: Int, slot: Int, count: Int, toBag: Int, toSlot: Int): String
    fun equip(role: Int, bag: Int, slot: Int): String
    fun equipByCategory(role: Int, category: Int): String
    fun unequip(role: Int, slot: Int): String
    fun autoAttack(role: Int, on: Boolean): String
    fun skillUsage(role: Int, on: Boolean): String
    fun learnSkill(role: Int, actionId: Int, level: Int): String
    fun addStat(role: Int, attr: Int): String
    fun statReset(role: Int): String
    fun skillReset(role: Int): String
    fun cast(role: Int, actionId: Int): String
    fun questQuit(questId: Int): String
    fun save(): String
    fun mainMenu(): String
    fun enterSlot(slot: Int): String
    fun npcInteract(): String
    fun npcDialogNext(): String
    fun npcDialogSelect(index: Int): String
    fun shopBuy(slot: Int): String
    fun jewel(role: Int, bag: Int, slot: Int, equipSlot: Int): String
    fun switchPlayer(slot: Int): String
    fun discardItem(bag: Int, slot: Int): String
    fun includeParty(mercenarySlot: Int): String
    fun excludeParty(mercenarySlot: Int): String
    fun discharge(mercenarySlot: Int): String
    fun withdraw(mercenarySlot: Int, equipSlot: Int): String
    fun dialogOk(): String
    fun dialogCancel(): String
    fun getPath(tx: Int, ty: Int): String
    fun attack(role: Int, targetSlot: Int): String
    fun stopCombat(role: Int): String
}
