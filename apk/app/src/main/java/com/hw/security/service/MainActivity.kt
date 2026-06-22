package com.hw.security.service

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.*
import androidx.lifecycle.viewmodel.compose.viewModel
import com.hw.security.service.auth.AuthScreen
import com.hw.security.service.auth.AuthViewModel
import com.hw.security.service.management.FreezeListScreen
import com.hw.security.service.management.FreezeListViewModel
import com.hw.security.service.management.WhitelistScreen
import com.hw.security.service.management.WhitelistViewModel
import com.hw.security.service.actions.JailbreakScreen
import com.hw.security.service.actions.FreezeScreen
import com.hw.security.service.root.ModuleFileIO
import com.hw.security.service.root.RootHelper
import com.hw.security.service.status.*
import com.hw.security.service.theme.GWBHJTheme
import com.hw.security.service.worker.WorkerScheduler
import com.topjohnwu.superuser.Shell

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Shell.getShell()
        enableEdgeToEdge()

        if (ModuleFileIO.hasLicense()) {
            WorkerScheduler.schedulePeriodicVerify(this)
        }

        setContent {
            GWBHJTheme {
                AppNavigator()
            }
        }
    }
}

sealed class Screen {
    data object Auth : Screen()
    data object Status : Screen()
    data object Whitelist : Screen()
    data object FreezeList : Screen()
    data object Jailbreak : Screen()
    data object Freeze : Screen()
    data object ClashInfo : Screen()
}

@Composable
fun AppNavigator() {
    var currentScreen by remember { mutableStateOf(determineStartScreen()) }

    when (currentScreen) {
        Screen.Auth -> {
            val viewModel: AuthViewModel = viewModel()
            AuthScreen(
                viewModel = viewModel,
                onAuthSuccess = { currentScreen = Screen.Status }
            )
        }

        Screen.Status -> {
            val viewModel: StatusViewModel = viewModel()
            StatusScreen(
                viewModel = viewModel,
                onNavigateToWhitelist = { currentScreen = Screen.Whitelist },
                onNavigateToFreezeList = { currentScreen = Screen.FreezeList },
                onNavigateToJailbreak = { currentScreen = Screen.Jailbreak },
                onNavigateToFreeze = { currentScreen = Screen.Freeze },
                onNavigateToClashInfo = { currentScreen = Screen.ClashInfo },
                onUnbind = { currentScreen = Screen.Auth }
            )
        }

        Screen.Whitelist -> {
            val viewModel: WhitelistViewModel = viewModel()
            WhitelistScreen(viewModel = viewModel, onBack = { currentScreen = Screen.Status })
        }

        Screen.FreezeList -> {
            val viewModel: FreezeListViewModel = viewModel()
            FreezeListScreen(viewModel = viewModel, onBack = { currentScreen = Screen.Status })
        }

        Screen.Jailbreak -> {
            JailbreakScreen(onBack = { currentScreen = Screen.Status })
        }

        Screen.Freeze -> {
            FreezeScreen(onBack = { currentScreen = Screen.Status })
        }

        Screen.ClashInfo -> {
            val viewModel: StatusViewModel = viewModel()
            viewModel.checkClashInfo()
            ClashInfoScreen(
                clashInfo = viewModel.uiState.collectAsState().value.clashInfo,
                onBack = { currentScreen = Screen.Status }
            )
        }
    }
}

private fun determineStartScreen(): Screen {
    if (!RootHelper.hasRoot()) return Screen.Auth
    if (!RootHelper.isModuleInstalled()) return Screen.Auth
    val license = ModuleFileIO.readLicense()
    return if (license != null && license.active) Screen.Status else Screen.Auth
}
