package com.hw.security.service.status

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.hw.security.service.data.ClashInfo
import com.hw.security.service.data.KickDetail
import com.hw.security.service.theme.Colors
import java.text.SimpleDateFormat
import java.util.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ClashInfoScreen(clashInfo: ClashInfo?, onBack: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("互抵详情", color = Colors.OnBackground) },
                navigationIcon = {
                    TextButton(onClick = onBack) { Text("返回", color = Colors.Primary) }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = Colors.Surface)
            )
        }
    ) { padding ->
        if (clashInfo == null) {
            Column(
                modifier = Modifier.fillMaxSize().padding(padding).padding(32.dp),
                verticalArrangement = Arrangement.Center
            ) {
                Text("暂无互抵记录", color = Colors.OnBackground, style = MaterialTheme.typography.bodyLarge)
            }
            return@Scaffold
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            if (clashInfo.isKicked) {
                Card(colors = CardDefaults.cardColors(containerColor = Colors.Error.copy(alpha = 0.2f))) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text("!! 卡密异地登录", color = Colors.Error, style = MaterialTheme.typography.titleMedium)
                        Text("您的设备已被其他设备踢出", color = Colors.OnBackground, style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }

            Card(colors = CardDefaults.cardColors(containerColor = Colors.Surface)) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("锁定状态", style = MaterialTheme.typography.titleSmall, color = Colors.Primary)
                    Spacer(modifier = Modifier.height(8.dp))
                    val statusText = when (clashInfo.lockStatus) {
                        "permanent" -> "永久锁定"
                        "locked" -> "锁定中 (${formatDuration(clashInfo.lockRemainingSeconds)})"
                        else -> "正常"
                    }
                    val statusColor = when (clashInfo.lockStatus) {
                        "permanent" -> Colors.Error
                        "locked" -> Colors.Warning
                        else -> Colors.Success
                    }
                    Text(statusText, color = statusColor, style = MaterialTheme.typography.bodyMedium)
                    Spacer(modifier = Modifier.height(4.dp))
                    Text("下次锁定等级: ${clashInfo.nextLockLevel}", color = Colors.OnBackground.copy(alpha = 0.6f), style = MaterialTheme.typography.bodySmall)
                }
            }

            if (clashInfo.kickDetails.isNotEmpty()) {
                Text("互抵记录", style = MaterialTheme.typography.titleSmall, color = Colors.Primary)
                clashInfo.kickDetails.forEach { detail ->
                    KickDetailCard(detail)
                }
            }
        }
    }
}

@Composable
private fun KickDetailCard(detail: KickDetail) {
    Card(colors = CardDefaults.cardColors(containerColor = Colors.Surface)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(detail.time, color = Colors.OnBackground.copy(alpha = 0.6f), style = MaterialTheme.typography.bodySmall)
            Spacer(modifier = Modifier.height(4.dp))
            detail.deviceModel?.let { Text("设备: $it", color = Colors.OnBackground, style = MaterialTheme.typography.bodyMedium) }
            detail.deviceSerial?.let { Text("序列号: $it", color = Colors.OnBackground, style = MaterialTheme.typography.bodySmall) }
            detail.deviceHashPrefix?.let { Text("Hash: ${it}...", color = Colors.OnBackground.copy(alpha = 0.4f), style = MaterialTheme.typography.bodySmall) }
            Spacer(modifier = Modifier.height(4.dp))
            Text(detail.action, color = Colors.Warning, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

private fun formatDuration(seconds: Long): String {
    if (seconds <= 0) return "0秒"
    val hours = seconds / 3600
    val minutes = (seconds % 3600) / 60
    return when {
        hours > 24 -> "${hours / 24}天${hours % 24}小时"
        hours > 0 -> "${hours}小时${minutes}分钟"
        else -> "${minutes}分钟"
    }
}
