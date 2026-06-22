package com.hw.security.service.auth

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.hw.security.service.theme.Colors

@Composable
fun AuthScreen(viewModel: AuthViewModel, onAuthSuccess: () -> Unit) {
    val uiState by viewModel.uiState.collectAsState()

    LaunchedEffect(uiState.success) {
        if (uiState.success) onAuthSuccess()
    }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Colors.Background
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(32.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "GWBHJ 授权绑定",
                style = MaterialTheme.typography.headlineMedium,
                color = Colors.OnBackground
            )

            Spacer(modifier = Modifier.height(8.dp))

            Text(
                text = "设备 Hash: ${uiState.deviceHash.take(16)}...",
                style = MaterialTheme.typography.bodySmall,
                color = Colors.OnBackground.copy(alpha = 0.6f)
            )

            Spacer(modifier = Modifier.height(32.dp))

            OutlinedTextField(
                value = uiState.codeInput,
                onValueChange = viewModel::onCodeChange,
                label = { Text("卡密") },
                placeholder = { Text("GWBHJ-XXXX-XXXX-XXXX") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Text),
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = Colors.OnBackground,
                    unfocusedTextColor = Colors.OnBackground,
                    focusedBorderColor = Colors.Primary,
                    unfocusedBorderColor = Colors.OnBackground.copy(alpha = 0.3f)
                )
            )

            Spacer(modifier = Modifier.height(16.dp))

            Button(
                onClick = viewModel::bind,
                modifier = Modifier.fillMaxWidth(),
                enabled = !uiState.isLoading && uiState.codeInput.isNotBlank()
            ) {
                if (uiState.isLoading) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(20.dp),
                        color = Colors.OnPrimary,
                        strokeWidth = 2.dp
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                }
                Text("绑定")
            }

            uiState.error?.let { error ->
                Spacer(modifier = Modifier.height(16.dp))
                Text(
                    text = error,
                    color = Colors.Error,
                    style = MaterialTheme.typography.bodyMedium
                )
            }
        }
    }
}
