package com.hw.security.service.management

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.hw.security.service.theme.Colors

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FreezeListScreen(viewModel: FreezeListViewModel, onBack: () -> Unit) {
    val uiState by viewModel.uiState.collectAsState()

    LaunchedEffect(Unit) { viewModel.loadFreezeList() }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("冻结列表管理", color = Colors.OnBackground) },
                navigationIcon = { TextButton(onClick = onBack) { Text("返回", color = Colors.Primary) } },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = Colors.Surface)
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier.fillMaxSize().padding(padding).padding(16.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                OutlinedTextField(
                    value = uiState.newPackage,
                    onValueChange = viewModel::onNewPackageChange,
                    label = { Text("包名") },
                    placeholder = { Text("com.example.app") },
                    modifier = Modifier.weight(1f),
                    singleLine = true,
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Colors.OnBackground,
                        unfocusedTextColor = Colors.OnBackground
                    )
                )
                Spacer(modifier = Modifier.width(8.dp))
                IconButton(onClick = viewModel::addPackage) {
                    Icon(Icons.Default.Add, "添加", tint = Colors.Primary)
                }
            }

            uiState.message?.let {
                Spacer(modifier = Modifier.height(8.dp))
                Text(it, color = Colors.Success, style = MaterialTheme.typography.bodySmall)
                LaunchedEffect(it) {
                    kotlinx.coroutines.delay(2000)
                    viewModel.clearMessage()
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            LazyColumn {
                items(uiState.packages) { pkg ->
                    Row(
                        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(pkg, color = Colors.OnBackground, modifier = Modifier.weight(1f))
                        IconButton(onClick = { viewModel.removePackage(pkg) }) {
                            Icon(Icons.Default.Delete, "删除", tint = Colors.Error, modifier = Modifier.size(20.dp))
                        }
                    }
                    HorizontalDivider(color = Colors.OnBackground.copy(alpha = 0.1f))
                }
            }

            if (uiState.packages.isEmpty()) {
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text("冻结列表为空", color = Colors.OnBackground.copy(alpha = 0.4f))
                }
            }
        }
    }
}
