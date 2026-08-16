package com.inotia4.export.service

import com.inotia4.export.LogFile
import com.inotia4.export.NativeBridge
import com.inotia4.export.util.JsonUtil

/**
 * OP 直写服务层接口（v0.5.46 P1 收边）：已实现 OP 端点统一入口。
 * 6 个方法对应 OpController 6 个已实现端点（路由迁移自 CharacterController），
 * 参数解析留在 controller，native 调用 + LogFile.op 记录在此。
 * 与 ActionApiService 同一形态：controller 只做路由与参数解析，业务走 service。
 */
interface OpApiService {
    fun setHp(role: Int, hp: Int): String
    fun setMp(role: Int, mp: Int): String
    fun setExperience(role: Int, exp: Long): String
    fun setLevel(role: Int, level: Int, force: Boolean): String
    fun setAttr(role: Int, stats: List<Pair<Int, Int>>): String
    fun addItem(category: Int, count: Int): String
}

/**
 * OP 直写服务实现（OpApiService 唯一实现，v0.5.46 迁移自 CharacterController 的 6 个 OP 端点）。
 * 每个操作经 LogFile.op 统一记录（端点/参数/结果/耗时），端点路径与 controller @PostMapping 一一对应。
 */
class OpApiServiceImpl : OpApiService {

    override fun setHp(role: Int, hp: Int): String =
        LogFile.op("POST /api/op/character/{role}/hp", "role=$role,hp=$hp") { NativeBridge.nativeOpSetHp(role, hp) }

    override fun setMp(role: Int, mp: Int): String =
        LogFile.op("POST /api/op/character/{role}/mp", "role=$role,mp=$mp") { NativeBridge.nativeOpSetMp(role, mp) }

    override fun setExperience(role: Int, exp: Long): String =
        LogFile.op("POST /api/op/character/{role}/experience", "role=$role,exp=$exp") {
            NativeBridge.nativeOpSetExperience(role, exp)
        }

    override fun setLevel(role: Int, level: Int, force: Boolean): String =
        LogFile.op("POST /api/op/character/{role}/level", "role=$role,level=$level,force=$force") {
            NativeBridge.nativeOpSetLevel(role, level, force)
        }

    // 批量设置基础属性（骰子 SetStatBase 路径）：stats 为属性索引/名字→值列表，循环调用 native，
    // 任一点失败即中断返回错误（批量循环逻辑从 controller 移入 impl，v0.5.46）
    override fun setAttr(role: Int, stats: List<Pair<Int, Int>>): String =
        LogFile.op("POST /api/op/character/{role}/set_attr", "role=$role,stats=$stats") {
            val sb = StringBuilder("[")
            for ((idx, v) in stats) {
                val r = NativeBridge.nativeOpSetAttr(role, idx, v)
                if (r.contains("\"ok\":false")) return@op JsonUtil.err("set attr $idx failed", 500)
                if (sb.length > 1) sb.append(',')
                sb.append("{\"attr\":$idx,\"value\":$v}")
            }
            sb.append(']')
            "{\"ok\":true,\"set\":$sb}"
        }

    override fun addItem(category: Int, count: Int): String =
        LogFile.op("POST /api/op/inventory/add", "category=$category,count=$count") {
            NativeBridge.nativeOpAddItem(category, count)
        }
}
