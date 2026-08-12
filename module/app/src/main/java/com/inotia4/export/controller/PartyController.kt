package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RestController

@RestController
class PartyController {

    @GetMapping("/api/character/party")
    fun composite(): String = ControllerGuard.guard(ApiServices.info::party)

    @GetMapping("/api/character/party/count")
    fun count(): String = ControllerGuard.guard(ApiServices.info::partyCount)

    @GetMapping("/api/character/party/leader")
    fun leader(): String = ControllerGuard.guard(ApiServices.info::partyLeader)

    @GetMapping("/api/character/party/{slot}")
    fun member(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMember(slot) }

    @GetMapping("/api/character/party/{slot}/id")
    fun memberId(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberId(slot) }

    @GetMapping("/api/character/party/{slot}/name")
    fun memberName(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberName(slot) }

    @GetMapping("/api/character/party/{slot}/level")
    fun memberLevel(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberLevel(slot) }

    @GetMapping("/api/character/party/{slot}/exp")
    fun memberExp(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberExp(slot) }

    @GetMapping("/api/character/party/{slot}/hp")
    fun memberHp(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberHp(slot) }

    @GetMapping("/api/character/party/{slot}/mp")
    fun memberMp(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberMp(slot) }

    @GetMapping("/api/character/party/{slot}/stats")
    fun memberStats(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberStats(slot) }

    @GetMapping("/api/character/party/{slot}/stats/{attr}")
    fun memberStat(@PathVariable("slot") slot: Int, @PathVariable("attr") attr: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberStat(slot, attr) }

    @GetMapping("/api/character/party/{slot}/equipment")
    fun memberEquipment(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberEquipment(slot) }

    @GetMapping("/api/character/party/{slot}/equipment/{equipSlot}")
    fun memberEquip(@PathVariable("slot") slot: Int, @PathVariable("equip_slot") equipSlot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberEquip(slot, equipSlot) }

    @GetMapping("/api/character/party/{slot}/skills")
    fun memberSkills(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberSkills(slot) }

    @GetMapping("/api/character/party/{slot}/skills/list")
    fun memberSkillList(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.partyMemberSkillList(slot) }
}
