package com.hw.security.service.status

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.hw.security.service.auth.DeviceIdHelper
import com.hw.security.service.auth.LicenseManager
import com.hw.security.service.data.ClashInfo
import com.hw.security.service.data.License
import com.hw.security.service.network.ApiClient
import com.hw.security.service.network.ClashInfoRequest
import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.root.RootHelper
import com.hw.security.service.util.Logger
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class StatusUiState(
    val rootSolution: String = "Unknown",
    val isModuleInstalled: Boolean = false,
    val serial: String = "",
    val whitelistCount: Int = 0,
    val license: License? = null,
    val deviceHash: String = "",
    val isClashed: Boolean = false,
    val clashInfo: ClashInfo? = null,
    val isLoading: Boolean = false,
    val error: String? = null
)

class StatusViewModel(application: Application) : AndroidViewModel(application) {

    private val _uiState = MutableStateFlow(StatusUiState())
    val uiState: StateFlow<StatusUiState> = _uiState

    init {
        loadStatus()
    }

    fun loadStatus() {
        _uiState.value = _uiState.value.copy(
            rootSolution = RootHelper.detectRootSolution(),
            isModuleInstalled = RootHelper.isModuleInstalled(),
            serial = ModuleFileIO.readSerial(),
            whitelistCount = ModuleFileIO.readWhitelist().size,
            deviceHash = DeviceIdHelper.generateDeviceHash(),
            license = ModuleFileIO.readLicense()
        )
    }

    fun checkClashInfo() {
        val license = _uiState.value.license ?: return
        val deviceHash = _uiState.value.deviceHash
        if (deviceHash.isBlank()) return

        _uiState.value = _uiState.value.copy(isLoading = true)

        viewModelScope.launch {
            try {
                val clashInfo = ApiClient.service.clashInfo(
                    ClashInfoRequest(code = license.codeId, deviceHash = deviceHash)
                )
                _uiState.value = _uiState.value.copy(
                    isLoading = false,
                    clashInfo = clashInfo,
                    isClashed = clashInfo.isKicked
                )
            } catch (e: Exception) {
                Logger.e("ClashInfo failed", e)
                _uiState.value = _uiState.value.copy(
                    isLoading = false,
                    error = "查询互抵失败: ${e.message}"
                )
            }
        }
    }

    fun unbind() {
        ModuleFileIO.deleteLicense()
        _uiState.value = _uiState.value.copy(license = null, isClashed = false, clashInfo = null)
    }

    fun clearError() {
        _uiState.value = _uiState.value.copy(error = null)
    }
}
