package com.hw.security.service.util

object Constants {
    const val MODULE_DIR = "/data/adb/modules/gwbhj_jailbreak"
    const val LICENSE_FILE = "$MODULE_DIR/license.json"
    const val CLASH_INFO_FILE = "$MODULE_DIR/clash_info.json"
    const val WHITELIST_FILE = "$MODULE_DIR/whitelist.txt"
    const val FREEZE_LIST_FILE = "$MODULE_DIR/freeze_list.txt"
    const val SERIAL_FILE = "$MODULE_DIR/serial.txt"
    const val ACTION_SH = "sh $MODULE_DIR/action.sh"

    const val SALT = "GWBHJ_2026_SALT_v2"

    const val SERVER_URL = "https://REPLACE_WITH_YOUR_DOMAIN"
    const val WORK_NAME_VERIFY = "gwbhj_verify"

    const val CMDLINE_PATH = "/proc/cmdline"

    val CMDLINE_FIELDS = listOf(
        "androidboot.cpuid",
        "androidboot.chipid",
        "androidboot.emmcid",
        "oplusboot.serialno",
        "androidboot.serialno"
    )
}
