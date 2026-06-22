package com.hw.security.service.root

import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.io.SuFile
import com.hw.security.service.util.Logger
import java.io.BufferedReader
import java.io.InputStreamReader

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
        val f = SuFile(path)
        if (!f.exists()) return ""
        return try {
            val sb = StringBuilder()
            BufferedReader(InputStreamReader(f.openInputStream())).use { reader ->
                var line: String?
                while (reader.readLine().also { line = it } != null) {
                    if (sb.isNotEmpty()) sb.append("\n")
                    sb.append(line)
                }
            }
            sb.toString()
        } catch (e: Exception) {
            Logger.e("readFile failed: $path", e)
            ""
        }
    }

    fun writeFile(path: String, content: String): Boolean {
        return try {
            val f = SuFile(path)
            f.parentFile?.mkdirs()
            f.openOutputStream().use { os ->
                os.write(content.toByteArray(Charsets.UTF_8))
            }
            true
        } catch (e: Exception) {
            Logger.e("writeFile failed: $path", e)
            false
        }
    }

    fun deleteFile(path: String): Boolean {
        return SuFile(path).delete()
    }

    fun fileExists(path: String): Boolean {
        return SuFile(path).exists()
    }
}
