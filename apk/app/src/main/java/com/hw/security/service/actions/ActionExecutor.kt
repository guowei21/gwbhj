package com.hw.security.service.actions

import com.hw.security.service.root.ShellExecutor
import com.hw.security.service.util.Constants

object ActionExecutor {

    fun resetSerial(): Boolean {
        return ShellExecutor.execute(Constants.ACTION_SH + " reset-serial").isSuccess
    }

    fun jailbreak(): Boolean {
        return ShellExecutor.execute(Constants.ACTION_SH + " jailbreak").isSuccess
    }

    fun freeze(): Boolean {
        return ShellExecutor.execute(Constants.ACTION_SH + " freeze").isSuccess
    }

    fun status(): String {
        val result = ShellExecutor.execute(Constants.ACTION_SH + " status")
        return if (result.isSuccess) result.out.joinToString("\n") else "获取状态失败"
    }
}
