package com.astrophoto.remote

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun UpdateDialog(
    notice: UpdateViewModel.Notice,
    onOpenRelease: (String) -> Unit,
    onLater: () -> Unit,
    onSkip: (String) -> Unit,
) {
    val releaseNotes = notice.info.notes.trim()
    AlertDialog(
        onDismissRequest = onLater,
        title = { Text("Update available") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(
                    "AstroPhoto Remote ${notice.info.version} is available. " +
                        "You are using ${notice.currentVersion}.",
                )
                if (releaseNotes.isNotEmpty()) {
                    Text(
                        "Release notes",
                        style = MaterialTheme.typography.labelLarge,
                    )
                    Column(
                        Modifier
                            .heightIn(max = 180.dp)
                            .verticalScroll(rememberScrollState()),
                    ) {
                        Text(releaseNotes.take(1500), style = MaterialTheme.typography.bodySmall)
                    }
                }
                Text(
                    "Updating replaces the app. Photos stay on the PC and are not affected.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onOpenRelease(notice.info.releaseUrl) }) {
                Text("Open release page")
            }
        },
        dismissButton = {
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                TextButton(onClick = { onSkip(notice.info.version) }) {
                    Text("Skip this version")
                }
                TextButton(onClick = onLater) {
                    Text("Later")
                }
            }
        },
    )
}
