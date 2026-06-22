package com.hw.security.service.worker

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.work.*
import com.hw.security.service.auth.DeviceIdHelper
import com.hw.security.service.auth.LicenseManager
import com.hw.security.service.crypto.Ed25519Verifier
import com.hw.security.service.network.ApiClient
import com.hw.security.service.network.VerifyRequest
import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.util.Constants
import com.hw.security.service.util.Logger
import java.util.UUID
import java.util.concurrent.TimeUnit

object WorkerScheduler {

    fun schedulePeriodicVerify(context: Context) {
        val request = PeriodicWorkRequestBuilder<VerifyWorker>(
            15, TimeUnit.MINUTES
        )
            .setConstraints(
                Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build()
            )
            .build()

        WorkManager.getInstance(context)
            .enqueueUniquePeriodicWork(
                Constants.WORK_NAME_VERIFY,
                ExistingPeriodicWorkPolicy.KEEP,
                request
            )
    }

    fun cancelPeriodicVerify(context: Context) {
        WorkManager.getInstance(context)
            .cancelUniqueWork(Constants.WORK_NAME_VERIFY)
    }
}
