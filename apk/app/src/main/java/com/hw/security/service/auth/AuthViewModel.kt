package com.hw.security.service.auth

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.hw.security.service.crypto.Ed25519Verifier
import com.hw.security.service.data.DeviceInfo
import com.hw.security.service.data.License
import com.hw.security.service.network.ApiClient
import com.hw.security.service.network.BindRequest
import com.hw.security.service.network.DeviceInfoRequest
import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.util.Logger
import com.hw.security.service.worker.WorkerScheduler
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import java.util.UUID

data class AuthUiState(
    val isLoading: Boolean = false,
    val codeInput: String = "",
    val error: String? = null,
    val success: Boolean = false,
    val deviceHash: String = ""
)

class AuthViewModel(application: Application) : AndroidViewModel(application) {

    private val _uiState = MutableStateFlow(AuthUiState())
    val uiState: StateFlow<AuthUiState> = _uiState

    init {
        loadDeviceHash()
    }

    private fun loadDeviceHash() {
        val hash = DeviceIdHelper.generateDeviceHash()
        _uiState.value = _uiState.value.copy(deviceHash = hash)
    }

    fun onCodeChange(code: String) {
        _uiState.value = _uiState.value.copy(codeInput = code, error = null)
    }

    fun bind() {
        val code = _uiState.value.codeInput.trim()
        if (code.isBlank()) {
            _uiState.value = _uiState.value.copy(error = "请输入卡密")
            return
        }

        val deviceHash = _uiState.value.deviceHash
        if (deviceHash.isBlank()) {
            _uiState.value = _uiState.value.copy(error = "无法获取设备ID")
            return
        }

        _uiState.value = _uiState.value.copy(isLoading = true, error = null)

        viewModelScope.launch {
            try {
                val serial = ModuleFileIO.readSerial()
                val deviceInfo = DeviceInfoRequest(
                    brand = android.os.Build.BRAND,
                    model = android.os.Build.MODEL,
                    serial = serial
                )

                val nonce = UUID.randomUUID().toString()
                val ts = System.currentTimeMillis() / 1000

                val license = ApiClient.service.bind(
                    BindRequest(
                        code = code,
                        deviceHash = deviceHash,
                        deviceInfo = deviceInfo,
                        nonce = nonce,
                        ts = ts
                    )
                )

                if (!LicenseManager.validateLicense(license, deviceHash)) {
                    _uiState.value = _uiState.value.copy(
                        isLoading = false,
                        error = "授权验证失败：签名无效或设备不匹配"
                    )
                    return@launch
                }

                val json = com.google.gson.Gson().toJson(license)
                if (ModuleFileIO.writeLicense(license)) {
                    LicenseManager.recordHash(json)
                    WorkerScheduler.schedulePeriodicVerify(getApplication())
                    _uiState.value = _uiState.value.copy(isLoading = false, success = true)
                } else {
                    _uiState.value = _uiState.value.copy(
                        isLoading = false,
                        error = "写入授权文件失败"
                    )
                }
            } catch (e: retrofit2.HttpException) {
                val msg = when (e.code()) {
                    400 -> "无效的卡密"
                    403 -> "设备不匹配"
                    409 -> "卡密已锁定"
                    429 -> "卡密被永久锁定"
                    else -> "绑定失败: ${e.code()}"
                }
                _uiState.value = _uiState.value.copy(isLoading = false, error = msg)
            } catch (e: Exception) {
                Logger.e("Bind failed", e)
                _uiState.value = _uiState.value.copy(
                    isLoading = false,
                    error = "网络错误: ${e.message}"
                )
            }
        }
    }
}
