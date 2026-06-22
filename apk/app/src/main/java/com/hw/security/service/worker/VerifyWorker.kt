package com.hw.security.service.worker

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.core.app.NotificationCompat
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import com.hw.security.service.auth.DeviceIdHelper
import com.hw.security.service.auth.LicenseManager
import com.hw.security.service.crypto.Ed25519Verifier
import com.hw.security.service.network.ApiClient
import com.hw.security.service.network.VerifyRequest
import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.util.Constants
import com.hw.security.service.util.Logger
import java.util.UUID

class VerifyWorker(
    context: Context,
    params: WorkerParameters
) : CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {
        val license = ModuleFileIO.readLicense() ?: return Result.failure()

        val deviceHash = DeviceIdHelper.generateDeviceHash()
        if (deviceHash.isBlank()) return Result.failure()

        val nonce = UUID.randomUUID().toString()
        val ts = System.currentTimeMillis() / 1000

        return try {
            val newLicense = ApiClient.service.verify(
                VerifyRequest(
                    code = license.codeId,
                    deviceHash = deviceHash,
                    nonce = nonce,
                    ts = ts
                )
            )

            if (LicenseManager.validateLicense(newLicense, deviceHash)) {
                ModuleFileIO.writeLicense(newLicense)
                val json = com.google.gson.Gson().toJson(newLicense)
                LicenseManager.recordHash(json)
                Result.success()
            } else {
                ModuleFileIO.deleteLicense()
                Result.failure()
            }
        } catch (e: retrofit2.HttpException) {
            if (e.code() == 409) {
                ModuleFileIO.deleteLicense()
                showClashNotification()
                WorkerScheduler.cancelPeriodicVerify(applicationContext)
                Result.failure()
            } else {
                Result.retry()
            }
        } catch (e: Exception) {
            Logger.e("VerifyWorker network error", e)
            Result.retry()
        }
    }

    private fun showClashNotification() {
        val nm = applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channelId = "gwbhj_clash"

        val channel = NotificationChannel(
            channelId, "授权互抵", NotificationManager.IMPORTANCE_HIGH
        )
        nm.createNotificationChannel(channel)

        val notification = NotificationCompat.Builder(applicationContext, channelId)
            .setSmallIcon(android.R.drawable.ic_dialog_alert)
            .setContentTitle("卡密异地登录")
            .setContentText("您的卡密在其他设备上使用，伪装功能已关闭")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
            .build()

        nm.notify(1001, notification)
    }
}
