package com.inotia4.export.controller

import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RestController

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
// OP 定稿端点（api-reference §8.2）：结构已定稿，待权限机制与底层实现，全部占位返回 not implemented
@RestController
class OpController {

    @PostMapping("/api/op/quest/accept")
    fun questAccept(): String = NOT_IMPL

    @PostMapping("/api/op/quest/complete")
    fun questComplete(): String = NOT_IMPL

    @PostMapping("/api/op/character/{role}/status-point")
    fun statusPoint(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/character/{role}/skill-point")
    fun skillPoint(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/character/{role}/skill-level")
    fun skillLevel(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/party/swap")
    fun partySwap(): String = NOT_IMPL

    @PostMapping("/api/op/inventory/set-slot")
    fun setSlot(): String = NOT_IMPL

    @PostMapping("/api/op/inventory/set-equip")
    fun setEquip(): String = NOT_IMPL

    @PostMapping("/api/op/inventory/money")
    fun money(): String = NOT_IMPL

    @PostMapping("/api/op/craft/mix-direct")
    fun mixDirect(): String = NOT_IMPL

    @PostMapping("/api/op/combat/{role}/heal")
    fun heal(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/combat/{role}/rest")
    fun rest(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/combat/{role}/revive")
    fun revive(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/combat/{role}/hate")
    fun hate(@PathVariable("role") role: Int): String = NOT_IMPL

    @PostMapping("/api/op/movement/teleport")
    fun teleport(): String = NOT_IMPL

    private companion object {
        const val NOT_IMPL = """{"ok":false,"error":"not implemented"}"""
    }
}
