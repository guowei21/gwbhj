package com.hw.security.service.data

import com.google.gson.annotations.SerializedName

data class ClashInfo(
    val code: String,
    @SerializedName("current_device_hash") val currentDeviceHash: String,
    @SerializedName("is_kicked") val isKicked: Boolean,
    @SerializedName("kick_details") val kickDetails: List<KickDetail>,
    @SerializedName("lock_status") val lockStatus: String,
    @SerializedName("lock_remaining_seconds") val lockRemainingSeconds: Long,
    @SerializedName("next_lock_level") val nextLockLevel: String,
    val signature: String
)

data class KickDetail(
    val time: String,
    @SerializedName("device_brand") val deviceBrand: String?,
    @SerializedName("device_model") val deviceModel: String?,
    @SerializedName("device_serial") val deviceSerial: String?,
    @SerializedName("device_hash_prefix") val deviceHashPrefix: String?,
    val action: String
)
