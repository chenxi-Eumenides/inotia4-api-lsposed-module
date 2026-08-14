package com.inotia4.export

import android.content.Context
import org.json.JSONObject

/**
 * 模块配置文件组件（v0.5.17）。
 *
 * 配置文件：模块 APK assets 根目录 `config.json`（勿放 static-data/ 下——
 * package_assets.py 会整体重建该目录）。assets 只读，运行时修改仅影响本次运行，
 * 不写回文件；重启进程后以文件内容为准。
 *
 * 当前配置项：
 * - listenAddress：HTTP 服务监听地址，默认 0.0.0.0
 * - listenPort：HTTP 服务监听端口，默认 8088
 * - stackLimitIncrease：是否启用游戏物品堆叠上限增加（99→999）。
 *   默认 false；目前仅提供配置选项与读写能力，实际生效逻辑未实现（预留）。
 * - jewelBatchMix：是否启用宝石批量合成。
 *   默认 false；目前仅提供配置选项与读写能力，实际生效逻辑未实现（预留）。
 *
 * 线程安全：配置可能被 API 请求线程/启动线程并发读写，字段用 @Volatile 保护。
 */
object ModuleConfig {

    private const val CONFIG_FILE = "config.json"

    const val DEFAULT_LISTEN_ADDRESS = "0.0.0.0"
    const val DEFAULT_LISTEN_PORT = 8088
    const val DEFAULT_STACK_LIMIT_INCREASE = false
    const val DEFAULT_JEWEL_BATCH_MIX = false

    @Volatile
    private var loaded = false

    /** HTTP 监听地址（默认 0.0.0.0） */
    @Volatile
    var listenAddress: String = DEFAULT_LISTEN_ADDRESS
        private set

    /** HTTP 监听端口（默认 8088，合法范围 1-65535） */
    @Volatile
    var listenPort: Int = DEFAULT_LISTEN_PORT
        private set

    /** 是否启用游戏物品堆叠上限增加（99→999，默认 false，预留未实现） */
    @Volatile
    var stackLimitIncrease: Boolean = DEFAULT_STACK_LIMIT_INCREASE
        private set

    /** 是否启用宝石批量合成（默认 false，预留未实现） */
    @Volatile
    var jewelBatchMix: Boolean = DEFAULT_JEWEL_BATCH_MIX
        private set

    /** 从 assets/config.json 加载配置；缺失/解析失败时回退默认值（幂等） */
    @Synchronized
    fun load(context: Context) {
        if (loaded) return
        val content = try {
            context.assets.open(CONFIG_FILE).bufferedReader().use { it.readText() }
        } catch (e: Exception) {
            LogFile.log("$CONFIG_FILE missing, using defaults")
            loaded = true
            return
        }
        try {
            val json = JSONObject(content)
            json.optString("listenAddress", DEFAULT_LISTEN_ADDRESS).let {
                if (it.isNotBlank()) listenAddress = it
            }
            json.optInt("listenPort", DEFAULT_LISTEN_PORT).let {
                if (it in 1..65535) listenPort = it
            }
            stackLimitIncrease = json.optBoolean("stackLimitIncrease", DEFAULT_STACK_LIMIT_INCREASE)
            jewelBatchMix = json.optBoolean("jewelBatchMix", DEFAULT_JEWEL_BATCH_MIX)
            LogFile.log(
                "config loaded: listenAddress=$listenAddress listenPort=$listenPort " +
                    "stackLimitIncrease=$stackLimitIncrease jewelBatchMix=$jewelBatchMix"
            )
        } catch (t: Throwable) {
            LogFile.logError("config parse failed, using defaults", t)
        }
        loaded = true
    }

    // ---- 运行时修改（不写回文件，重启后以文件为准） ----

    fun setListenAddress(address: String) {
        if (address.isNotBlank()) listenAddress = address
    }

    fun setListenPort(port: Int) {
        if (port in 1..65535) listenPort = port
    }

    fun setStackLimitIncrease(enabled: Boolean) {
        stackLimitIncrease = enabled
    }

    fun setJewelBatchMix(enabled: Boolean) {
        jewelBatchMix = enabled
    }

    /**
     * 应用配置（v0.5.19，配置端点调用）：仅更新 JSON 中出现的字段，
     * 校验失败时整体不生效（原子性），返回 null=成功、否则错误消息。
     */
    @Synchronized
    fun apply(json: JSONObject): String? {
        var newAddress = listenAddress
        var newPort = listenPort
        var newStack = stackLimitIncrease
        var newJewel = jewelBatchMix
        if (json.has("listenAddress")) {
            val a = json.optString("listenAddress")
            if (a.isBlank()) return "listenAddress required"
            newAddress = a
        }
        if (json.has("listenPort")) {
            val p = json.optInt("listenPort", -1)
            if (p !in 1..65535) return "listenPort must be 1-65535"
            newPort = p
        }
        if (json.has("stackLimitIncrease")) newStack = json.optBoolean("stackLimitIncrease", newStack)
        if (json.has("jewelBatchMix")) newJewel = json.optBoolean("jewelBatchMix", newJewel)
        listenAddress = newAddress
        listenPort = newPort
        stackLimitIncrease = newStack
        jewelBatchMix = newJewel
        return null
    }

    /** 当前生效配置序列化（配置端点 GET 返回用） */
    fun toJson(): JSONObject = JSONObject().apply {
        put("listenAddress", listenAddress)
        put("listenPort", listenPort)
        put("stackLimitIncrease", stackLimitIncrease)
        put("jewelBatchMix", jewelBatchMix)
    }
}
