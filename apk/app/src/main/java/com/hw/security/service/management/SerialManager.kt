package com.hw.security.service.management

import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.root.ShellExecutor

object SerialManager {

    fun readSerial(): String {
        return ModuleFileIO.readSerial()
    }

    fun resetSerial(): String {
        ShellExecutor.execute("sh /data/adb/modules/gwbhj_jailbreak/action.sh reset-serial")
        return ModuleFileIO.readSerial()
    }

    fun setSerial(serial: String): Boolean {
        return ModuleFileIO.writeSerial(serial)
    }
}
