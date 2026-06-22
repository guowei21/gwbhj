package com.hw.security.service.status

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.hw.security.service.data.License
import com.hw.security.service.theme.Colors
import java.text.SimpleDateFormat
import java.util.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun StatusScreen(
    viewModel: StatusViewModel,
    onNavigateToWhitelist: () -> Unit,
    onNavigateToFreezeList: () -> Unit,
    onNavigateToJailbreak: () -> Unit,
    onNavigateToFreeze: () -> Unit,
    onNavigateToClashInfo: () -> Unit,
    onUnbind: () -> Unit
) {
    val uiState by viewModel.uiState.collectAsState()

    LaunchedEffect(Unit) { viewModel.loadStatus() }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("过强标黑机", color = Colors.OnBackground) },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = Colors.Surface)
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            StatusCard("Root 方案", uiState.rootSolution, Icons.Default.Security)
            StatusCard("模块状态", if (uiState.isModuleInstalled) "已安装" else "未安装", Icons.Default.Extension)
            StatusCard("序列号", uiState.serial.ifBlank { "未设置" }, Icons.Default.SerialPort)
            StatusCard("白名单", "${uiState.whitelistCount} 个包名", Icons.Default.List)
            StatusCard("设备 Hash", uiState.deviceHash.take(16) + "...", Icons.Default.Fingerprint)

            if (uiState.license != null) {
                LicenseCard(uiState.license!!, viewModel)
            } else {
                Card(colors = CardDefaults.cardColors(containerColor = Colors.Error.copy(alpha = 0.2f))) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text("未授权", color = Colors.Error, style = MaterialTheme.typography.titleMedium)
                        Text("请先绑定卡密", color = Colors.OnBackground, style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }

            Divider(color = Colors.OnBackground.copy(alpha = 0.1f))

            ActionButton("白名单管理", Icons.Default.List, onNavigateToWhitelist)
            ActionButton("冻结列表管理", Icons.Default.AcUnit, onNavigateToFreezeList)
            ActionButton("越狱隐藏环境", Icons.Default.LockOpen, onNavigateToJailbreak)
            ActionButton("冻结多开软件", Icons.Default.Freeze, onNavigateToFreeze)

            if (uiState.license != null) {
                ActionButton("查看互抵详情", Icons.Default.Warning, onNavigateToClashInfo)
                Spacer(modifier = Modifier.height(8.dp))
                OutlinedButton(
                    onClick = { viewModel.unbind(); onUnbind() },
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Colors.Error)
                ) {
                    Text("解绑卡密")
                }
            }
        }
    }
}

@Composable
private fun StatusCard(title: String, value: String, icon: androidx.compose.ui.graphics.vector.ImageVector) {
    Card(colors = CardDefaults.cardColors(containerColor = Colors.Surface)) {
        Row(
            modifier = Modifier.padding(16.dp).fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(icon, contentDescription = null, tint = Colors.Primary, modifier = Modifier.size(24.dp))
            Spacer(modifier = Modifier.width(12.dp))
            Column {
                Text(title, color = Colors.OnBackground.copy(alpha = 0.6f), style = MaterialTheme.typography.bodySmall)
                Text(value, color = Colors.OnBackground, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun LicenseCard(license: License, viewModel: StatusViewModel) {
    val activeColor = if (license.active) Colors.Success else Colors.Error
    val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault())
    val lastOk = sdf.format(Date(license.lastServerOk * 1000))
    val expiry = sdf.format(Date((license.lastServerOk + license.graceSeconds) * 1000))

    Card(colors = CardDefaults.cardColors(containerColor = Colors.Surface)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    if (license.active) Icons.Default.CheckCircle else Icons.Default.Cancel,
                    contentDescription = null,
                    tint = activeColor,
                    modifier = Modifier.size(24.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    if (license.active) "已授权" else "授权无效",
                    color = activeColor,
                    style = MaterialTheme.typography.titleMedium
                )
            }
            Spacer(modifier = Modifier.height(8.dp))
            InfoRow("卡密", license.codeId)
            InfoRow("最后验证", lastOk)
            InfoRow("有效期至", expiry)
            InfoRow("互抵次数", "${license.clashCount}")
            license.lockUntil?.let { lockUntil ->
                if (lockUntil > 0) {
                    InfoRow("锁定至", sdf.format(Date(lockUntil * 1000)))
                }
            }
        }
    }
}

@Composable
private fun InfoRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp)) {
        Text(label, color = Colors.OnBackground.copy(alpha = 0.6f), style = MaterialTheme.typography.bodySmall, modifier = Modifier.weight(1f))
        Text(value, color = Colors.OnBackground, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun ActionButton(text: String, icon: androidx.compose.ui.graphics.vector.ImageVector, onClick: () -> Unit) {
    OutlinedButton(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth()
    ) {
        Icon(icon, contentDescription = null, modifier = Modifier.size(18.dp))
        Spacer(modifier = Modifier.width(8.dp))
        Text(text)
    }
}
