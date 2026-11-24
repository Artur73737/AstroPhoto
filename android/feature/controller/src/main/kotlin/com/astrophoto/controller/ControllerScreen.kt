package com.astrophoto.controller

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Notes
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.astrophoto.connection.ConnectionSheet
import com.astrophoto.controller.chrome.*
import com.astrophoto.controller.model.*
import com.astrophoto.controller.sheets.ModeSettingsSheet
import com.astrophoto.gallery.GallerySheet
import com.astrophoto.liveview.LivePreview
import com.astrophoto.livestack.*
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import com.astrophoto.settings.*
import com.astrophoto.zwocapture.ZwoCaptureControls
import kotlinx.serialization.json.*

private enum class CameraSheet { None, General, Mode, Camera, FocusParameters, Connection, Gallery, Logs }

@Composable fun ControllerScreen(repository: RemoteRepository, generalExtras: (@Composable () -> Unit)? = null) {
    val state by repository.state.collectAsStateWithLifecycle()
    var sheet by remember { mutableStateOf(CameraSheet.None) }
    var requested by rememberSaveable { mutableStateOf(CameraMode.Photo.name) }
    var showCapture by rememberSaveable { mutableStateOf(false) }
    var showHistogram by rememberSaveable { mutableStateOf(true) }
    var showGrid by rememberSaveable { mutableStateOf(true) }
    val mode = resolveMode(state, CameraMode.entries.firstOrNull { it.name == requested } ?: CameraMode.Photo)
    val action = shutterAction(state, mode)
    val connected = state.snapshot.flag("connected")
    LaunchedEffect(state.settings["focus_peaking"]) { repository.processedPreview(state.settings.flag("focus_peaking")) }
    fun command(tool: String, action: String) = repository.command(tool, buildJsonObject { put("action", action) })
    fun perform() {
        when (action.action) {
            PrimaryAction.Photo -> { showCapture = false; repository.command("capture_single") }
            PrimaryAction.Sequence -> command("sequence_control", "start")
            PrimaryAction.Capture -> repository.startFrameCapture(state.settings.obj("zwo_recording"))
            PrimaryAction.Stack -> command("stack_control", "start")
            PrimaryAction.StarFocus -> command("autofocus_control", "start")
            PrimaryAction.Settings -> sheet = CameraSheet.Mode
            PrimaryAction.StopSequence -> command("sequence_control", "stop")
            PrimaryAction.StopCapture -> command("frame_capture_control", "stop")
            PrimaryAction.StopStack -> command("stack_control", "stop")
            PrimaryAction.StopStarFocus -> command("autofocus_control", "stop")
            PrimaryAction.StopPointFocus -> command("point_focus_control", "stop")
        }
    }
    Surface(color = Color.Black, modifier = Modifier.fillMaxSize()) {
        CameraWorkspace(
            header = {
                Row(Modifier.fillMaxWidth().height(52.dp).padding(horizontal = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    TextButton(onClick = { sheet = CameraSheet.Connection }, contentPadding = PaddingValues(4.dp), modifier = Modifier.weight(1f)) {
                        Icon(Icons.Outlined.Camera, null, tint = Color.LightGray, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Column(Modifier.weight(1f)) {
                            Text(if (connected) state.snapshot.text("model") else if (state.authenticated) state.serverName else "AstroPhoto", color = Color.White, style = MaterialTheme.typography.labelLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
                            Text(cameraReadout(state), color = Color.LightGray, style = MaterialTheme.typography.labelSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        }
                    }
                    IconButton(onClick = { sheet = CameraSheet.Logs }, enabled = state.authenticated) { Icon(Icons.AutoMirrored.Outlined.Notes, "Session log", tint = Color.LightGray) }
                }
            },
            preview = { modifier ->
                Box(modifier) {
                    if (mode == CameraMode.Stack) StackPreview(state, Modifier.fillMaxSize())
                    else LivePreview(state, repository, mode.title, showCapture && !state.isZwo && mode == CameraMode.Photo, showHistogram, { sheet = CameraSheet.Connection }, Modifier.fillMaxSize(), showGrid)
                    state.error?.let { error ->
                        Row(Modifier.align(Alignment.TopCenter).fillMaxWidth().padding(8.dp).background(MaterialTheme.colorScheme.errorContainer, RoundedCornerShape(12.dp)).padding(start = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                            Text(error, Modifier.weight(1f), maxLines = 2, overflow = TextOverflow.Ellipsis, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onErrorContainer)
                            IconButton(onClick = repository::clearError) { Icon(Icons.Outlined.Close, "Dismiss error", tint = MaterialTheme.colorScheme.onErrorContainer) }
                        }
                    }
                }
            },
            status = { CameraStatus(state, mode) },
            shutter = {
                ShutterControls(state, mode, action, { sheet = CameraSheet.Gallery }, secondary = {
                    when {
                        state.captureActive -> {
                            val paused = state.snapshot.obj("sequence").text("state") in listOf("Paused", "In pausa")
                            IconButton(onClick = { command("sequence_control", if (paused) "resume" else "pause") }, enabled = !state.busy) { Icon(if (paused) Icons.Outlined.PlayArrow else Icons.Outlined.Pause, if (paused) "Resume sequence" else "Pause sequence", tint = Color.White) }
                        }
                        state.snapshot.obj("live_stack").flag("active") -> {
                            val paused = state.snapshot.obj("live_stack").flag("pause_requested")
                            IconButton(onClick = { command("stack_control", if (paused) "resume" else "pause") }, enabled = !state.busy && !state.snapshot.obj("live_stack").flag("stopping")) { Icon(if (paused) Icons.Outlined.PlayArrow else Icons.Outlined.Pause, if (paused) "Resume stack" else "Pause stack", tint = Color.White) }
                        }
                    }
                }, perform = ::perform)
            },
            modes = { ModeCarousel(CameraMode.available(state.isZwo), mode, !state.acquisitionActive && !state.busy) { requested = it.name; showCapture = false } },
            footer = { CameraFooter(mode, { sheet = CameraSheet.General }, { sheet = CameraSheet.Mode }, { sheet = CameraSheet.Connection }, state.authenticated) },
        )
    }
    val dismiss = { sheet = CameraSheet.None }
    when (sheet) {
        CameraSheet.None -> Unit
        CameraSheet.General -> SettingsSheet(state, repository, dismiss, connection = { sheet = CameraSheet.Connection }, logs = { sheet = CameraSheet.Logs }, generalContent = generalExtras, viewerOptions = {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) { Text("Histogram", Modifier.padding(top = 12.dp)); Switch(showHistogram, { showHistogram = it }) }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) { Text("Composition grid", Modifier.padding(top = 12.dp)); Switch(showGrid, { showGrid = it }) }
        })
        CameraSheet.Connection -> ConnectionSheet(state, repository, dismiss)
        CameraSheet.Gallery -> GallerySheet(repository, dismiss)
        CameraSheet.Camera -> SettingsSheet(state, repository, { sheet = CameraSheet.Mode }, if (state.isZwo) SettingsScope.CaptureArea else SettingsScope.Camera)
        CameraSheet.FocusParameters -> StarFocusSettingsSheet(state, repository) { sheet = CameraSheet.Mode }
        CameraSheet.Mode -> when (mode) {
            CameraMode.Photo -> SettingsSheet(state, repository, dismiss, SettingsScope.Photo, viewerOptions = {
                Choice("Preview source", if (showCapture) "Last capture" else "Live View", listOf("Live View", "Last capture")) { showCapture = it == "Last capture" }
            })
            CameraMode.Sequence -> SettingsSheet(state, repository, dismiss, SettingsScope.Sequence)
            CameraMode.Capture -> ModeSettingsSheet("Capture settings", dismiss) {
                TextButton(onClick = { sheet = CameraSheet.Camera }, modifier = Modifier.padding(horizontal = 16.dp)) { Text("Camera controls, format and ROI") }
                ZwoCaptureControls(state, repository, showActions = false)
            }
            CameraMode.Focus -> ModeSettingsSheet("Focus settings", dismiss) {
                TextButton(onClick = { sheet = CameraSheet.Camera }, modifier = Modifier.padding(horizontal = 16.dp)) { Text("Camera exposure controls") }
                FocusControls(state, repository, showHistogram, { showHistogram = !showHistogram }) { sheet = CameraSheet.FocusParameters }
            }
            CameraMode.Stack -> ModeSettingsSheet("Stack settings", dismiss) {
                TextButton(onClick = { sheet = CameraSheet.Camera }, modifier = Modifier.padding(horizontal = 16.dp)) { Text("Camera exposure controls") }
                StackControls(state, repository)
            }
        }
        CameraSheet.Logs -> SessionLogSheet(repository, dismiss)
    }
}

private fun cameraReadout(state: RemoteState): String {
    if (!state.authenticated) return "Pair with your Windows PC"
    if (!state.snapshot.flag("connected")) return state.status
    return if (state.isZwo) state.snapshot.array("device_controls").map { it.jsonObject }.filter { it.text("id") in listOf("0", "1") }.joinToString(" · ") { control ->
        val value = control.text("value").toDoubleOrNull() ?: 0.0
        if (control.text("unit") == "us") "${"%.3g".format(java.util.Locale.US, value / 1000)} ms" else "${control.text("name")} ${control.text("value")}${if (control.flag("automatic")) " A" else ""}"
    } else state.snapshot.array("properties").map { it.jsonObject }.filter { it.text("id") in listOf("ISO", "Av", "Tv") }.joinToString(" · ") { "${it.text("id")} ${it.text("label")}" }
}
