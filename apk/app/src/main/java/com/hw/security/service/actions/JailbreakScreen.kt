package com.hw.security.service.actions

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.hw.security.service.theme.Colors

@Composable
fun JailbreakScreen(onBack: () -> Unit) {
    var isExecuting by remember { mutableStateOf(false) }
    var result by remember { mutableStateOf<String?>(null) }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Colors.Background
    ) {
        Column(
            modifier = Modifier.padding(32.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("越狱隐藏环境", style = MaterialTheme.typography.headlineSmall, color = Colors.OnBackground)

            Spacer(modifier = Modifier.height(16.dp))

            Text(
                "将执行以下操作：\n" +
                "1. 放宽内核安全参数\n" +
                "2. restorecon 属性区\n" +
                "3. PID 回收\n" +
                "4. 自动 soft reboot",
                color = Colors.OnBackground.copy(alpha = 0.7f),
                style = MaterialTheme.typography.bodyMedium
            )

            Spacer(modifier = Modifier.height(24.dp))

            Button(
                onClick = {
                    isExecuting = true
                    Thread {
                        val success = ActionExecutor.jailbreak()
                        result = if (success) "执行成功，设备将软重启" else "执行失败"
                        isExecuting = false
                    }.start()
                },
                enabled = !isExecuting,
                colors = ButtonDefaults.buttonColors(containerColor = Colors.Warning)
            ) {
                if (isExecuting) {
                    CircularProgressIndicator(modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
                    Spacer(modifier = Modifier.width(8.dp))
                }
                Text("执行越狱")
            }

            result?.let {
                Spacer(modifier = Modifier.height(16.dp))
                Text(it, color = Colors.Success, style = MaterialTheme.typography.bodyMedium)
            }

            Spacer(modifier = Modifier.height(24.dp))

            TextButton(onClick = onBack) { Text("返回", color = Colors.Primary) }
        }
    }
}
