package com.astrophoto.connection

import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.QrCodeScanner
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.astrophoto.designsystem.AstroCyan
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import kotlinx.serialization.json.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable fun ConnectionSheet(state: RemoteState, repository: RemoteRepository, dismiss: () -> Unit) {
    val context = LocalContext.current
    var device by remember { mutableStateOf(Settings.Global.getString(context.contentResolver, Settings.Global.DEVICE_NAME)?.takeIf { it.isNotBlank() } ?: Build.MODEL) }
    var error by remember { mutableStateOf<String?>(null) }
    val scanner = rememberLauncherForActivityResult(ScanContract()) { result ->
        result.contents?.let { raw ->
            try { val code = PairingCode.parse(raw); error = null; repository.connect(code.address, code.key, device) }
            catch (e: Exception) { error = e.message }
        }
    }
    ModalBottomSheet(onDismissRequest = dismiss, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)) {
        Column(Modifier.fillMaxWidth().imePadding().verticalScroll(rememberScrollState()).padding(24.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Text("Pair with your PC", style = MaterialTheme.typography.headlineSmall)
            Text("Open AstroPhoto → Settings → Android / WebSocket → Start WS server. Scan the QR displayed on the PC.")
            OutlinedTextField(device, { device = it }, label = { Text("This device's name") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
            Button(onClick = { scanner.launch(ScanOptions().setDesiredBarcodeFormats(ScanOptions.QR_CODE).setPrompt("Scan the AstroPhoto QR on your PC").setBeepEnabled(false).setOrientationLocked(false).setBarcodeImageEnabled(false)) }, enabled = !state.authenticated && device.isNotBlank(), modifier = Modifier.fillMaxWidth().height(56.dp)) {
                Icon(Icons.Outlined.QrCodeScanner, null); Spacer(Modifier.width(12.dp)); Text("Scan PC pairing QR")
            }
            Text("The QR expires after two minutes and can be used once. Regenerate it on the PC to pair again. No photo or pairing key is saved on this phone.", style = MaterialTheme.typography.bodySmall)
            Text("Use a trusted private Wi-Fi network. Traffic is authenticated and signed; image content is not encrypted.", style = MaterialTheme.typography.bodySmall)
            Text(state.status + if (state.serverName.isNotEmpty()) " · ${state.serverName}" else "", color = AstroCyan)
            (error ?: state.error)?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            if (state.authenticated) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { repository.command("camera_connection", buildJsonObject { put("action", if (state.snapshot.flag("connected")) "disconnect" else "connect") }) }, enabled = !state.busy) { Text(if (state.snapshot.flag("connected")) "Disconnect Canon" else "Connect Canon") }
                    TextButton(onClick = dismiss) { Text("Done") }
                }
                OutlinedButton(onClick = { repository.disconnect() }) { Text("Disconnect phone") }
                Text("Disconnecting the phone does not stop a running PC sequence.", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
