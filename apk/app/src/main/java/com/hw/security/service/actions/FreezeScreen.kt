package com.hw.security.service.actions

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.hw.security.service.theme.Colors

@Composable
fun FreezeScreen(onBack: () -> Unit) {
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
            Text("冻结多开系统软件", style = MaterialTheme.typography.headlineSmall, color = Colors.OnBackground)

            Spacer(modifier = Modifier.height(16.dp))

            Text(
                "将冻结冻结列表中多开用户的系统软件。\n" +
                "请确保已配置冻结列表。",
                color = Colors.OnBackground.copy(alpha = 0.7f),
                style = MaterialTheme.typography.bodyMedium
            )

            Spacer(modifier = Modifier.height(24.dp))

            Button(
                onClick = {
                    isExecuting = true
                    Thread {
                        val success = ActionExecutor.freeze()
                        result = if (success) "执行成功" else "执行失败"
                        isExecuting = false
                    }.start()
                },
                enabled = !isExecuting
            ) {
                if (isExecuting) {
                    CircularProgressIndicator(modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
                    Spacer(modifier = Modifier.width(8.dp))
                }
                Text("执行冻结")
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
