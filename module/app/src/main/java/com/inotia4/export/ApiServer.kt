package com.inotia4.export

import android.content.Context
import android.util.Log
import com.inotia4.export.StaticData
import com.yanzhenjie.andserver.AndServer
import com.yanzhenjie.andserver.Server
import java.util.concurrent.TimeUnit

object ApiServer {

    private const val PORT = 8088
    private var server: Server? = null

    @Synchronized
    fun start(context: Context, moduleApkPath: String?) {
        if (server?.isRunning == true) return
        StaticData.attach(context)
        // AndServer 通过 context.getAssets() 扫描 .andserver 文件定位注册类。
        // LSPosed 注入场景下 context 是游戏进程的，assets 为游戏 APK；需把模块 APK 加入 AssetManager。
        if (moduleApkPath != null) {
            try {
                val m = android.content.res.AssetManager::class.java
                    .getMethod("addAssetPath", String::class.java)
                m.invoke(context.assets, moduleApkPath)
                LogFile.log("module assets added: $moduleApkPath")
            } catch (t: Throwable) {
                LogFile.logError("addAssetPath failed", t)
            }
        }
        // P0#瓦片矩阵（2026-08-12）：加载静态瓦片矩阵入 native（替代运行时读内存）
        // 必须在 addAssetPath 之后（tiles.json 在模块 APK assets 内）
        try {
            val tilesJson = StaticData.read("maps/tiles.json")
            if (tilesJson != null && NativeBridge.ready) {
                val ok = NativeBridge.nativeSetTilesData(tilesJson)
                LogFile.log("static tiles loaded: $ok")
            } else if (tilesJson == null) {
                LogFile.log("static tiles read failed: maps/tiles.json missing")
            }
        } catch (t: Throwable) {
            LogFile.logError("load static tiles failed", t)
        }
        try {
            server = AndServer.webServer(context)
                .port(PORT)
                .timeout(10, TimeUnit.SECONDS)
                .listener(object : Server.ServerListener {
                    override fun onStarted() {
                        Log.i(TAG, "AndServer started on 0.0.0.0:$PORT")
                    }

                    override fun onStopped() {
                        Log.i(TAG, "AndServer stopped")
                    }

                    override fun onException(e: Exception) {
                        Log.e(TAG, "AndServer error", e)
                    }
                })
                .build()
            server?.startup()
        } catch (t: Throwable) {
            Log.e(TAG, "ApiServer start failed", t)
        }
    }

    @Synchronized
    fun stop() {
        server?.shutdown()
        server = null
    }

    private const val TAG = "Inotia4Export"
}
