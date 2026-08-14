package com.inotia4.export.controller

import com.inotia4.export.service.ApiServices
import com.inotia4.export.util.ControllerGuard
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PostMapping
import com.yanzhenjie.andserver.annotation.RequestBody
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController
import org.json.JSONObject

// controller: 路由层，业务走 ApiServices。路径首段必须静态（AndServer 处理器约束，architecture §3）
@RestController
class SaveController {

    @PostMapping("/api/system/save")
    fun save(): String = ControllerGuard.guard { ApiServices.action.save() }

    @PostMapping("/api/system/enter_slot")
    fun enterSlot(@RequestBody body: String): String {
        val slot = try {
            JSONObject(body).optInt("slot", -1)
        } catch (e: Exception) {
            -1
        }
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required (0-2)\"}"
        return ControllerGuard.guard { ApiServices.action.enterSlot(slot) }
    }

    @PostMapping("/api/system/create_slot")
    fun create(@RequestBody body: String): String {
        val slot = try {
            JSONObject(body).optInt("slot", -1)
        } catch (e: Exception) {
            -1
        }
        val classIdx = try {
            JSONObject(body).optInt("class_idx", -1)
        } catch (e: Exception) {
            -1
        }
        if (slot < 0) return "{\"ok\":false,\"error\":\"slot required (0-2)\"}"
        if (classIdx < 0) return "{\"ok\":false,\"error\":\"class_idx required (0-5)\"}"
        return ControllerGuard.guard { ApiServices.action.createSlot(slot, classIdx) }
    }

    @GetMapping("/api/system/export_save_file")
    fun exportSaveFile(@RequestParam("slot") slot: Int): String =
        ControllerGuard.guard { ApiServices.info.exportSaveFile(slot) }
}
