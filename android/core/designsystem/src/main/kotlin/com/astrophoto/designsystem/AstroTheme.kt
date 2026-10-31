package com.astrophoto.designsystem

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

val AstroCyan = Color(0xFFB6E2E4)
@Composable fun AstroTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = darkColorScheme(
        primary = AstroCyan, onPrimary = Color(0xFF12383A), secondary = Color(0xFF91BEC2),
        primaryContainer = Color(0xFFB6E2E4), onPrimaryContainer = Color(0xFF12383A),
        secondaryContainer = Color(0xFF2A4143), onSecondaryContainer = Color(0xFFC2E8EA),
        background = Color.Black, surface = Color(0xFF101415), surfaceContainer = Color(0xFF1D2526),
        onSurface = Color(0xFFEDF3F3), outline = Color(0xFF617475)
    ), content = content)
}
@Composable fun Section(title: String, content: @Composable ColumnScope.() -> Unit) {
    Surface(shape = RoundedCornerShape(24.dp), color = MaterialTheme.colorScheme.surfaceContainer, modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium, color = AstroCyan)
            content()
        }
    }
}
fun String.displayName() = replace('_', ' ').replaceFirstChar { it.uppercase() }
