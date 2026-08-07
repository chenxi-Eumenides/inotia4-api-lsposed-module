package com.inotia4.export.controller

import com.inotia4.export.service.InfoService
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RestController

@RestController
class PartyController {

    @GetMapping("/api/info/party")
    fun composite(): String = ControllerGuard.guard(InfoService::party)

    @GetMapping("/api/info/party/count")
    fun count(): String = ControllerGuard.guard(InfoService::partyCount)

    @GetMapping("/api/info/party/leader")
    fun leader(): String = ControllerGuard.guard(InfoService::partyLeader)

    @GetMapping("/api/info/party/{slot}")
    fun member(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMember(slot) }

    @GetMapping("/api/info/party/{slot}/id")
    fun memberId(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberId(slot) }

    @GetMapping("/api/info/party/{slot}/name")
    fun memberName(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberName(slot) }

    @GetMapping("/api/info/party/{slot}/level")
    fun memberLevel(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberLevel(slot) }

    @GetMapping("/api/info/party/{slot}/exp")
    fun memberExp(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberExp(slot) }

    @GetMapping("/api/info/party/{slot}/hp")
    fun memberHp(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberHp(slot) }

    @GetMapping("/api/info/party/{slot}/mp")
    fun memberMp(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberMp(slot) }

    @GetMapping("/api/info/party/{slot}/stats")
    fun memberStats(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberStats(slot) }

    @GetMapping("/api/info/party/{slot}/stats/{attr}")
    fun memberStat(@PathVariable("slot") slot: Int, @PathVariable("attr") attr: Int): String =
        ControllerGuard.guard { InfoService.partyMemberStat(slot, attr) }

    @GetMapping("/api/info/party/{slot}/equipment")
    fun memberEquipment(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberEquipment(slot) }

    @GetMapping("/api/info/party/{slot}/equipment/{equipSlot}")
    fun memberEquip(@PathVariable("slot") slot: Int, @PathVariable("equipSlot") equipSlot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberEquip(slot, equipSlot) }

    @GetMapping("/api/info/party/{slot}/skills")
    fun memberSkills(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberSkills(slot) }

    @GetMapping("/api/info/party/{slot}/skills/list")
    fun memberSkillList(@PathVariable("slot") slot: Int): String =
        ControllerGuard.guard { InfoService.partyMemberSkillList(slot) }
}
