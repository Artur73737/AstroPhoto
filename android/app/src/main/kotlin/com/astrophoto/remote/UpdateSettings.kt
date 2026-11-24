package com.astrophoto.remote

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.astrophoto.designsystem.Section
import java.text.DateFormat
import java.util.Date

@Composable fun UpdateSettings(model: UpdateViewModel, open: (String) -> Unit) {
    val status by model.status.collectAsStateWithLifecycle()
    Section("App updates") {
        Text("Installed: ${status.installed}", style = MaterialTheme.typography.bodyMedium)
        status.result?.let { result ->
            Text("Latest published APK: ${result.latest.version}", style = MaterialTheme.typography.bodyMedium)
            Text(if (result.updateAvailable) "An update is available." else if (result.latest.version == status.installed) "You are up to date." else "Installed version is newer than the published APK.", style = MaterialTheme.typography.bodySmall)
            if (result.updateAvailable) TextButton(onClick = { open(result.latest.releaseUrl) }) { Text("Open release page") }
        }
        if (status.checkedAt != 0L) Text("Last successful check: ${DateFormat.getDateTimeInstance().format(Date(status.checkedAt))}", style = MaterialTheme.typography.bodySmall)
        status.error?.let { Text(it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall) }
        OutlinedButton(onClick = { model.checkForUpdates() }, enabled = !status.checking, modifier = Modifier.fillMaxWidth()) { Text(if (status.checking) "Checking GitHub…" else "Check for updates") }
        Text("Checks the GitHub release API for Android APKs. Works without a PC connection or camera.", style = MaterialTheme.typography.bodySmall)
    }
}
