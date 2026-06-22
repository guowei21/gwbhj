package com.hw.security.service.root

import com.topjohnwu.superuser.Shell
import com.hw.security.service.util.Logger

object RootHelper {

    fun hasRoot(): Boolean {
        return Shell.getShell().isRoot
    }

    fun detectRootSolution(): String {
        if (Shell.cmd("which apd").exec().isSuccess) return "APatch"
        if (Shell.cmd("which ksud").exec().isSuccess) return "KernelSU"
        if (Shell.cmd("which magisk").exec().isSuccess) return "Magisk"
        return "Unknown"
    }

    fun isModuleInstalled(): Boolean {
        return Shell.cmd("test -d /data/adb/modules/gwbhj_jailbreak").exec().isSuccess
    }

    fun readProcCmdline(): String {
        return ShellExecutor.readFile("/proc/cmdline")
    }
}
