package com.hw.security.service.auth

import com.hw.security.service.crypto.HashGenerator
import com.hw.security.service.root.RootHelper
import com.hw.security.service.util.Constants
import com.hw.security.service.util.Logger

object DeviceIdHelper {

    fun readHardwareId(): String {
        val cmdline = RootHelper.readProcCmdline()
        if (cmdline.isBlank()) {
            Logger.e("Failed to read /proc/cmdline")
            return ""
        }

        for (field in Constants.CMDLINE_FIELDS) {
            val value = extractField(cmdline, field)
            if (value.isNotBlank()) {
                Logger.d("Hardware ID from $field")
                return value
            }
        }

        Logger.e("No hardware ID found in cmdline")
        return ""
    }

    fun generateDeviceHash(): String {
        val hwId = readHardwareId()
        if (hwId.isBlank()) return ""
        return HashGenerator.generateDeviceHash(hwId, Constants.SALT)
    }

    private fun extractField(cmdline: String, key: String): String {
        val searchKey = "$key="
        val startIndex = cmdline.indexOf(searchKey)
        if (startIndex < 0) return ""

        val valueStart = startIndex + searchKey.length
        val valueEnd = cmdline.indexOfAny(charArrayOf(' ', '\t', '\n', '\r'), valueStart)
        val end = if (valueEnd < 0) cmdline.length else valueEnd

        return cmdline.substring(valueStart, end).trim()
    }
}
