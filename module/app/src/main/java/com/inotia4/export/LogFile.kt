package com.inotia4.export

import android.content.Context
import android.os.Environment
import android.util.Log
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object LogFile {

    private const val TAG = "Inotia4Export"
    private const val FILE_NAME = "inotia4-export.log"
    private const val GAME_PKG =
        "com.com2us.inotia4.normal.freefull.google.global.android.common"

    @Volatile
    private var file: File? = null

    fun initEarly() {
        if (file != null) return
        try {
            val base = Environment.getExternalStorageDirectory()
            val dir = File(base, "Android/data/$GAME_PKG/files")
            if (dir.exists() || dir.mkdirs()) {
                val f = File(dir, FILE_NAME)
                f.appendText("=== Inotia4Export log start ${timestamp()} ===\n")
                file = f
                Log.i(TAG, "log file: ${f.absolutePath}")
            } else {
                Log.e(TAG, "log dir mkdir failed: $dir")
            }
        } catch (t: Throwable) {
            Log.e(TAG, "LogFile initEarly failed", t)
        }
    }

    fun init(context: Context) {
        if (file != null) return
        try {
            val dir = context.getExternalFilesDir(null)
            if (dir != null) {
                val f = File(dir, FILE_NAME)
                f.appendText("=== Inotia4Export log start ${timestamp()} ===\n")
                file = f
                Log.i(TAG, "log file: ${f.absolutePath}")
            }
        } catch (t: Throwable) {
            Log.e(TAG, "LogFile init failed", t)
        }
    }

    fun log(msg: String) {
        Log.i(TAG, msg)
        write("[I] $msg")
    }

    fun logError(msg: String, t: Throwable? = null) {
        Log.e(TAG, msg, t)
        val sw = StringWriter()
        t?.printStackTrace(PrintWriter(sw))
        write("[E] $msg${if (t != null) "\n$sw" else ""}")
    }

    fun path(): String? = file?.absolutePath

    private fun write(line: String) {
        try {
            file?.appendText("${timestamp()} $line\n")
        } catch (t: Throwable) {
            Log.e(TAG, "log write failed", t)
        }
    }

    private fun timestamp(): String =
        SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(Date())
}
