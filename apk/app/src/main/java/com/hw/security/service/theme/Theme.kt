package com.hw.security.service.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable

private val DarkColorScheme = darkColorScheme(
    primary = Colors.Primary,
    onPrimary = Colors.OnPrimary,
    secondary = Colors.Secondary,
    background = Colors.Background,
    surface = Colors.Surface,
    onBackground = Colors.OnBackground,
    onSurface = Colors.OnSurface,
    error = Colors.Error
)

private val LightColorScheme = lightColorScheme(
    primary = Colors.Primary,
    onPrimary = Colors.OnPrimary,
    secondary = Colors.Secondary,
    background = Colors.Background,
    surface = Colors.Surface,
    onBackground = Colors.OnBackground,
    onSurface = Colors.OnSurface,
    error = Colors.Error
)

@Composable
fun GWBHJTheme(content: @Composable () -> Unit) {
    val colorScheme = if (isSystemInDarkTheme()) DarkColorScheme else DarkColorScheme
    MaterialTheme(
        colorScheme = colorScheme,
        content = content
    )
}
