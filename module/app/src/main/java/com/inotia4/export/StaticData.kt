package com.inotia4.export

import android.content.Context

object StaticData {

    @Volatile
    private var appContext: Context? = null

    private val cache = HashMap<String, String>()

    fun attach(context: Context) {
        appContext = context
    }

    fun read(path: String): String? {
        cache[path]?.let { return it }
        val ctx = appContext ?: return null
        val content = try {
            ctx.assets.open("static-data/$path").bufferedReader().use { it.readText() }
        } catch (e: Exception) {
            return null
        }
        if (cache.size < 64) cache[path] = content
        return content
    }
}
