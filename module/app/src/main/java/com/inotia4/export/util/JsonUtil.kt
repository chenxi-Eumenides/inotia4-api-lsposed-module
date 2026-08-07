package com.inotia4.export.util

import org.json.JSONArray
import org.json.JSONObject

/**
 * 通用 JSON 工具：解析容错 + 错误响应构造。
 * controller/service 层复用的公共函数，避免各处重复 try-catch 与手写错误串。
 */
object JsonUtil {

    const val NOT_FOUND = """{"error":"not found"}"""
    const val NOT_READY = """{"error":"not ready"}"""
    const val BAD_REQUEST = """{"error":"bad request"}"""

    /** 解析对象，失败返回 null（不抛异常） */
    fun parseObj(json: String?): JSONObject? = try {
        if (json.isNullOrBlank()) null else JSONObject(json)
    } catch (e: Exception) {
        null
    }

    /** 解析数组，失败返回 null（不抛异常） */
    fun parseArr(json: String?): JSONArray? = try {
        if (json.isNullOrBlank()) null else JSONArray(json)
    } catch (e: Exception) {
        null
    }

    /** 构造 {"key": value} 简单对象 */
    fun wrap(key: String, value: Any?): String =
        JSONObject().put(key, value ?: JSONObject.NULL).toString()

    /** 构造 {"key": value, ...} 多字段对象 */
    fun wrap(vararg pairs: Pair<String, Any?>): String {
        val o = JSONObject()
        for ((k, v) in pairs) o.put(k, v ?: JSONObject.NULL)
        return o.toString()
    }
}
