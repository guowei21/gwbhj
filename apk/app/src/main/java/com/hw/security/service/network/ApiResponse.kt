package com.hw.security.service.network

import com.google.gson.annotations.SerializedName

data class BindRequest(
    val code: String,
    @SerializedName("device_hash") val deviceHash: String,
    @SerializedName("device_info") val deviceInfo: DeviceInfoRequest?,
    val nonce: String? = null,
    val ts: Long? = null
)

data class VerifyRequest(
    val code: String,
    @SerializedName("device_hash") val deviceHash: String,
    val nonce: String? = null,
    val ts: Long? = null
)

data class ClashInfoRequest(
    val code: String,
    @SerializedName("device_hash") val deviceHash: String
)

data class DeviceInfoRequest(
    val brand: String,
    val model: String,
    val serial: String
)

data class GenerateRequest(
    val count: Int,
    val prefix: String? = null,
    @SerializedName("admin_key") val adminKey: String
)

data class DeleteRequest(
    val codes: List<String>,
    @SerializedName("admin_key") val adminKey: String
)

data class GenerateResponse(
    val codes: List<String>,
    val count: Int
)

data class DeleteResponse(
    val deleted: List<String>
)

data class ListResponse(
    val codes: List<CodeEntry>,
    val total: Int
)

data class CodeEntry(
    val code: String,
    val status: String,
    @SerializedName("active_device_info") val activeDeviceInfo: DeviceInfoRequest?,
    @SerializedName("active_time") val activeTime: String?,
    @SerializedName("clash_count") val clashCount: Int,
    @SerializedName("lock_until") val lockUntil: Long?,
    @SerializedName("created_at") val createdAt: String?
)

data class ApiError(
    val error: String
)
