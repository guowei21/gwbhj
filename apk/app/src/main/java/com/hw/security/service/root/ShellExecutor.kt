package com.hw.security.service.root

import com.topjohnwu.superuser.Shell
import com.hw.security.service.util.Logger

object ShellExecutor {

    fun execute(command: String): Shell.Result {
        return Shell.cmd(command).exec()
    }

    fun execute(vararg commands: String): Shell.Result {
        return Shell.cmd(*commands).exec()
    }

    fun executeAsync(command: String, callback: (Shell.Result) -> Unit) {
        Shell.cmd(command).submit(callback)
    }

    fun readFile(path: String): String {
        val result = Shell.cmd("cat '$path'").exec()
        return if (result.isSuccess) result.out.joinToString("\n") else ""
    }

    fun writeFile(path: String, content: String): Boolean {
        val escaped = content.replace("'", "'\\''")
        val result = Shell.cmd("echo '$escaped' > '$path'").exec()
        return result.isSuccess
    }

    fun writeFileDirect(path: String, content: String): Boolean {
        val tmpPath = "/data/local/tmp/.gwbhj_tmp"
        try {
            java.io.File(tmpPath).writeText(content)
            val result = Shell.cmd("cp '$tmpPath' '$path'", "rm -f '$tmpPath'").exec()
            return result.isSuccess
        } catch (e: Exception) {
            Logger.e("writeFileDirect failed", e)
            return false
        }
    }

    fun deleteFile(path: String): Boolean {
        return Shell.cmd("rm -f '$path'").exec().isSuccess
    }

    fun fileExists(path: String): Boolean {
        return Shell.cmd("test -f '$path'").exec().isSuccess
    }
}
