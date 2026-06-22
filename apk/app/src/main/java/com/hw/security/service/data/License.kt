package com.hw.security.service.data

import com.google.gson.annotations.SerializedName

data class License(
    @SerializedName("code_id") val codeId: String,
    @SerializedName("device_hash") val deviceHash: String,
    val active: Boolean,
    val revoked: Boolean,
    @SerializedName("last_server_ok") val lastServerOk: Long,
    @SerializedName("check_interval") val checkInterval: Int = 600,
    @SerializedName("grace_seconds") val graceSeconds: Int = 86400,
    @SerializedName("lock_until") val lockUntil: Long? = null,
    @SerializedName("clash_count") val clashCount: Int = 0,
    val signature: String
)
