package com.hw.security.service.crypto

import android.util.Base64
import com.hw.security.service.data.License
import org.bouncycastle.crypto.Signer
import org.bouncycastle.crypto.signers.Ed25519Signer
import org.bouncycastle.crypto.params.Ed25519PublicKeyParameters
import java.security.MessageDigest

object Ed25519Verifier {

    private const val SERVER_PUBLIC_KEY_B64 = "REPLACE_WITH_SERVER_PUBLIC_KEY_BASE64"

    fun verifyLicense(license: License): Boolean {
        val signatureBytes = try {
            Base64.decode(license.signature, Base64.NO_WRAP)
        } catch (e: Exception) {
            return false
        }

        if (signatureBytes.size != 64) return false

        val pubKeyBytes = try {
            Base64.decode(SERVER_PUBLIC_KEY_B64, Base64.NO_WRAP)
        } catch (e: Exception) {
            return false
        }

        if (pubKeyBytes.size != 32) return false

        val message = buildSignMessage(license)

        return try {
            val publicKey = Ed25519PublicKeyParameters(pubKeyBytes, 0)
            val signer: Signer = Ed25519Signer()
            signer.init(false, publicKey)
            signer.update(message, 0, message.size)
            signer.verifySignature(signatureBytes)
        } catch (e: Exception) {
            false
        }
    }

    private fun buildSignMessage(license: License): ByteArray {
        val fields = listOf(
            license.codeId,
            license.deviceHash,
            license.active.toString(),
            license.revoked.toString(),
            license.lastServerOk.toString(),
            license.checkInterval.toString(),
            license.graceSeconds.toString(),
            (license.lockUntil ?: 0).toString(),
            license.clashCount.toString()
        )
        return fields.joinToString("|").toByteArray(Charsets.UTF_8)
    }

    fun verifyClashInfoSignature(
        code: String,
        isKicked: Boolean,
        lockStatus: String,
        lockRemainingSeconds: Long,
        signatureBase64: String
    ): Boolean {
        val signatureBytes = try {
            Base64.decode(signatureBase64, Base64.NO_WRAP)
        } catch (e: Exception) { return false }
        if (signatureBytes.size != 64) return false

        val pubKeyBytes = try {
            Base64.decode(SERVER_PUBLIC_KEY_B64, Base64.NO_WRAP)
        } catch (e: Exception) { return false }
        if (pubKeyBytes.size != 32) return false

        val message = """{"code":"$code","is_kicked":$isKicked,"lock_status":"$lockStatus","lock_remaining_seconds":$lockRemainingSeconds}"""
            .toByteArray(Charsets.UTF_8)

        return try {
            val publicKey = Ed25519PublicKeyParameters(pubKeyBytes, 0)
            val signer: Signer = Ed25519Signer()
            signer.init(false, publicKey)
            signer.update(message, 0, message.size)
            signer.verifySignature(signatureBytes)
        } catch (e: Exception) { false }
    }
}
