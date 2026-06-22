package com.hw.security.service.auth

import com.hw.security.service.crypto.Ed25519Verifier
import com.hw.security.service.data.License
import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.util.Logger
import java.security.MessageDigest

object LicenseManager {

    private var lastWrittenHash: String = ""

    fun validateLicense(license: License, localDeviceHash: String): Boolean {
        if (!Ed25519Verifier.verifyLicense(license)) {
            Logger.e("License signature verification failed")
            return false
        }

        if (license.deviceHash != localDeviceHash) {
            Logger.e("License device_hash mismatch")
            return false
        }

        if (license.revoked) {
            Logger.e("License revoked")
            return false
        }

        license.lockUntil?.let { lockUntil ->
            if (lockUntil > 0 && System.currentTimeMillis() / 1000 < lockUntil) {
                Logger.e("License locked until $lockUntil")
                return false
            }
        }

        if (!license.active) {
            Logger.e("License not active")
            return false
        }

        val now = System.currentTimeMillis() / 1000
        if (license.lastServerOk + license.graceSeconds < now) {
            Logger.e("Grace period expired")
            return false
        }

        return true
    }

    fun recordHash(json: String) {
        val digest = MessageDigest.getInstance("SHA-256")
        val bytes = digest.digest(json.toByteArray(Charsets.UTF_8))
        lastWrittenHash = bytes.joinToString("") { "%02x".format(it) }
    }

    fun detectTamper(): Boolean {
        val currentContent = ModuleFileIO.readLicenseRaw()
        if (currentContent.isBlank()) return false
        val digest = MessageDigest.getInstance("SHA-256")
        val bytes = digest.digest(currentContent.toByteArray(Charsets.UTF_8))
        val currentHash = bytes.joinToString("") { "%02x".format(it) }
        return currentHash != lastWrittenHash && lastWrittenHash.isNotBlank()
    }
}
