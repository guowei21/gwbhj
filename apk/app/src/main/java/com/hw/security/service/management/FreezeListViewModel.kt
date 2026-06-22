package com.hw.security.service.management

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.hw.security.service.root.ModuleFileIO
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

data class FreezeListUiState(
    val packages: List<String> = emptyList(),
    val newPackage: String = "",
    val message: String? = null
)

class FreezeListViewModel(application: Application) : AndroidViewModel(application) {

    private val _uiState = MutableStateFlow(FreezeListUiState())
    val uiState: StateFlow<FreezeListUiState> = _uiState

    init { loadFreezeList() }

    fun loadFreezeList() {
        _uiState.value = _uiState.value.copy(packages = ModuleFileIO.readFreezeList())
    }

    fun onNewPackageChange(pkg: String) {
        _uiState.value = _uiState.value.copy(newPackage = pkg)
    }

    fun addPackage() {
        val pkg = _uiState.value.newPackage.trim()
        if (pkg.isBlank()) return
        if (_uiState.value.packages.contains(pkg)) {
            _uiState.value = _uiState.value.copy(message = "包名已存在", newPackage = "")
            return
        }
        val updated = _uiState.value.packages + pkg
        if (ModuleFileIO.writeFreezeList(updated)) {
            _uiState.value = _uiState.value.copy(packages = updated, newPackage = "", message = "已添加")
        }
    }

    fun removePackage(pkg: String) {
        val updated = _uiState.value.packages.filter { it != pkg }
        if (ModuleFileIO.writeFreezeList(updated)) {
            _uiState.value = _uiState.value.copy(packages = updated, message = "已删除")
        }
    }

    fun clearMessage() {
        _uiState.value = _uiState.value.copy(message = null)
    }
}
