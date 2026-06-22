package com.hw.security.service.crypto

import java.security.MessageDigest

object HashGenerator {

    fun sha256(input: String): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val bytes = digest.digest(input.toByteArray(Charsets.UTF_8))
        return bytes.joinToString("") { "%02x".format(it) }
    }

    fun generateDeviceHash(hardwareId: String, salt: String): String {
        return sha256(hardwareId + salt)
    }
}
