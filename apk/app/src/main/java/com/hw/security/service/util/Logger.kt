package com.hw.security.service.util

import android.util.Log

object Logger {
    private const val TAG = "GWBHJ"

    fun d(msg: String) {
        if (BuildConfig.DEBUG) Log.d(TAG, msg)
    }

    fun e(msg: String) {
        Log.e(TAG, msg)
    }

    fun e(msg: String, t: Throwable) {
        Log.e(TAG, msg, t)
    }
}
