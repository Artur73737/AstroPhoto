package com.astrophoto.controller

import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.astrophoto.controller.sheets.ModeSettingsSheet
import com.astrophoto.network.RemoteRepository
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

@Composable internal fun SessionLogSheet(repository: RemoteRepository, dismiss: () -> Unit) {
    var text by remember { mutableStateOf("Loading…") }
    LaunchedEffect(Unit) {
        text = try {
            repository.request("get_logs", buildJsonObject { put("limit", 100) }).jsonObject.array("logs").joinToString("\n\n") {
                val entry = it.jsonObject; "${entry.text("time")} ${entry.text("level")}\n${entry.text("message")}"
            }
        } catch (e: Exception) { e.message ?: "Unable to read log" }
    }
    ModeSettingsSheet("PC session log", dismiss) { Text(text, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp)) }
}
