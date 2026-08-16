package com.inotia4.export.controller

import com.inotia4.export.LogFile
import com.inotia4.export.util.ApiException
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RestController
import com.yanzhenjie.andserver.http.StatusCode

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
// OP 定稿端点（api-reference §8.2）：结构已定稿，待权限机制与底层实现，全部占位返回 not implemented
@RestController
class OpController {

    private fun notImpl(): Nothing = throw ApiException(StatusCode.SC_NOT_IMPLEMENTED, "not implemented")

    @PostMapping("/api/op/quest/accept")
    fun questAccept(): String = LogFile.op("POST /api/op/quest/accept", "") { notImpl() }

    @PostMapping("/api/op/quest/complete")
    fun questComplete(): String = LogFile.op("POST /api/op/quest/complete", "") { notImpl() }

    @PostMapping("/api/op/character/{role}/status-point")
    fun statusPoint(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/character/{role}/status-point", "role=$role") { notImpl() }

    @PostMapping("/api/op/character/{role}/skill-point")
    fun skillPoint(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/character/{role}/skill-point", "role=$role") { notImpl() }

    @PostMapping("/api/op/character/{role}/skill-level")
    fun skillLevel(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/character/{role}/skill-level", "role=$role") { notImpl() }

    @PostMapping("/api/op/party/swap")
    fun partySwap(): String = LogFile.op("POST /api/op/party/swap", "") { notImpl() }

    @PostMapping("/api/op/inventory/set-slot")
    fun setSlot(): String = LogFile.op("POST /api/op/inventory/set-slot", "") { notImpl() }

    @PostMapping("/api/op/inventory/set-equip")
    fun setEquip(): String = LogFile.op("POST /api/op/inventory/set-equip", "") { notImpl() }

    @PostMapping("/api/op/inventory/money")
    fun money(): String = LogFile.op("POST /api/op/inventory/money", "") { notImpl() }

    @PostMapping("/api/op/craft/mix-direct")
    fun mixDirect(): String = LogFile.op("POST /api/op/craft/mix-direct", "") { notImpl() }

    @PostMapping("/api/op/combat/{role}/heal")
    fun heal(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/combat/{role}/heal", "role=$role") { notImpl() }

    @PostMapping("/api/op/combat/{role}/rest")
    fun rest(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/combat/{role}/rest", "role=$role") { notImpl() }

    @PostMapping("/api/op/combat/{role}/revive")
    fun revive(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/combat/{role}/revive", "role=$role") { notImpl() }

    @PostMapping("/api/op/combat/{role}/hate")
    fun hate(@PathVariable("role") role: Int): String =
        LogFile.op("POST /api/op/combat/{role}/hate", "role=$role") { notImpl() }

    @PostMapping("/api/op/movement/teleport")
    fun teleport(): String = LogFile.op("POST /api/op/movement/teleport", "") { notImpl() }
}
