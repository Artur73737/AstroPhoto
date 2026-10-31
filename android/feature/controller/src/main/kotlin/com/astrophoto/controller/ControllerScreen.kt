package com.astrophoto.controller

import android.os.Build
import android.provider.Settings
import androidx.compose.foundation.*
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import android.content.res.Configuration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.astrophoto.designsystem.*
import com.astrophoto.liveview.LivePreview
import com.astrophoto.gallery.GallerySheet
import com.astrophoto.connection.ConnectionSheet
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import com.astrophoto.settings.*
import kotlinx.coroutines.launch
import kotlinx.serialization.json.*
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable fun ControllerScreen(repository: RemoteRepository) {
    val state by repository.state.collectAsStateWithLifecycle()
    var sheet by remember { mutableStateOf("") }
    var mode by rememberSaveable { mutableStateOf("Photo") }
    var showCapture by rememberSaveable { mutableStateOf(false) }
    var processed by remember { mutableStateOf(false) }
    var showHistogram by remember { mutableStateOf(true) }
    var lensStep by remember { mutableIntStateOf(1) }
    LaunchedEffect(state.settings["focus_peaking"]) { processed = state.settings.flag("focus_peaking"); repository.processedPreview(processed) }
    val connected = state.snapshot.flag("connected")
    val live = state.snapshot.flag("live_view")
    val captureActive = state.snapshot.obj("sequence").text("state") in listOf("Running", "Paused", "Stopping", "In esecuzione", "In pausa", "Arresto")
    val landscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
    val header: @Composable () -> Unit = {
            Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Outlined.Camera, null, tint = AstroCyan, modifier = Modifier.size(22.dp))
                TextButton(onClick = { sheet = "connection" }, modifier = Modifier.weight(1f)) {
                    Column(Modifier.fillMaxWidth()) {
                        Text(if (state.authenticated) state.serverName else "AstroPhoto", style = MaterialTheme.typography.titleMedium)
                        Text(if (connected) state.snapshot.text("model") else state.status, style = MaterialTheme.typography.labelSmall)
                    }
                }
                IconButton(onClick = { sheet = "logs" }, enabled = state.authenticated) { Icon(Icons.Outlined.Notes, "Session log") }
                IconButton(onClick = { sheet = "settings" }, enabled = state.authenticated) { Icon(Icons.Outlined.Tune, "All settings") }
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                state.snapshot.array("properties").map { it.jsonObject }.filter { it.text("id") in listOf("Av", "Tv", "ISO", "ImageQuality") }.forEach { property ->
                    AssistChip(onClick = { sheet = "settings" }, label = { Text("${property.text("id")} ${property.text("label")}", style = MaterialTheme.typography.labelSmall) })
                }
            }
    }
    val modeSelector: @Composable () -> Unit = {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.Center) {
                listOf("Photo", "Sequence", "Focus").forEach { item -> FilterChip(mode == item, { mode = item }, label = { Text(item) }, modifier = Modifier.padding(horizontal = 4.dp)) }
            }
    }
    val controls: @Composable () -> Unit = {
            if (mode == "Focus") Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                val lensReady = state.snapshot.obj("lens").text("state") == "ready" && live && !state.snapshot.obj("autofocus").flag("active")
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilledTonalButton(onClick = { repository.command("lens_move", buildJsonObject { put("step", -lensStep) }) }, enabled = lensReady) { Text("−") }
                    Box(Modifier.weight(1f)) { Choice("Lens step", "$lensStep", state.snapshot.obj("camera_module").array("focus_steps").map { it.jsonPrimitive.content }) { lensStep = it.toInt() } }
                    FilledTonalButton(onClick = { repository.command("lens_move", buildJsonObject { put("step", lensStep) }) }, enabled = lensReady) { Text("+") }
                }
                Text(if (lensReady) "Tap a point to measure focus · Lens AF ready" else state.snapshot.obj("lens").text("detail", "Live View and lens AF required"), style = MaterialTheme.typography.labelSmall)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), modifier = Modifier.horizontalScroll(rememberScrollState())) {
                    listOf("Off", "Normal", "Stars").forEach { peaking ->
                        val selected = if (!processed) peaking == "Off" else if (state.settings.flag("star_peaking")) peaking == "Stars" else peaking == "Normal"
                        FilterChip(selected, {
                            repository.command("update_settings", buildJsonObject { put("patch", buildJsonObject { put("focus_peaking", peaking != "Off"); put("star_peaking", peaking == "Stars") }) })
                        }, label = { Text("Peaking $peaking") }, enabled = state.authenticated)
                    }
                    FilterChip(showHistogram, { showHistogram = !showHistogram }, label = { Text("Histogram") })
                    AssistChip(onClick = { sheet = "settings" }, label = { Text("AF parameters") })
                }
                val af = state.snapshot.obj("autofocus")
                Text("Star AF: ${af.text("phase", "idle")} · HFD ${af.text("current", "—")}", style = MaterialTheme.typography.labelMedium)
                if (af.text("detail").isNotEmpty()) Text(af.text("detail"), style = MaterialTheme.typography.bodySmall)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { repository.command("autofocus_control", buildJsonObject { put("action", "start") }) }, enabled = lensReady) { Text("Start star AF") }
                    OutlinedButton(onClick = { repository.command("autofocus_control", buildJsonObject { put("action", "stop") }) }, enabled = af.flag("active")) { Text("Stop") }
                }
            }
            if (mode == "Sequence") Column(Modifier.padding(horizontal = 16.dp)) {
                val sequence = state.snapshot.obj("sequence")
                Text("${sequence.text("state", "Idle")} · ${sequence.text("completed", "0")} / ${sequence.text("total", "0")}", style = MaterialTheme.typography.labelLarge)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = { sheet = "settings" }, enabled = state.authenticated) { Text("Edit exposures") }
                    TextButton(onClick = { repository.command("sequence_control", buildJsonObject { put("action", "pause") }) }, enabled = connected) { Text("Pause") }
                    TextButton(onClick = { repository.command("sequence_control", buildJsonObject { put("action", "resume") }) }, enabled = connected) { Text("Resume") }
                    TextButton(onClick = { repository.command("sequence_control", buildJsonObject { put("action", "stop") }) }, enabled = connected) { Text("Stop") }
                }
            }
            state.error?.let { error -> Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp).background(MaterialTheme.colorScheme.errorContainer, RoundedCornerShape(12.dp)).padding(10.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(error, Modifier.weight(1f), style = MaterialTheme.typography.bodySmall, maxLines = 3)
                IconButton(onClick = { repository.clearError() }) { Icon(Icons.Outlined.Close, "Dismiss error") }
            } }
            Row(Modifier.fillMaxWidth().padding(horizontal = 32.dp, vertical = 8.dp), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                FilledTonalIconButton(onClick = { sheet = "gallery" }, enabled = state.authenticated, modifier = Modifier.size(54.dp)) {
                    state.thumbnail?.let { Image(it.asImageBitmap(), "Photos on PC", Modifier.fillMaxSize(), contentScale = ContentScale.Crop) } ?: Icon(Icons.Outlined.PhotoLibrary, "Photos on PC")
                }
                Box(Modifier.size(82.dp).border(3.dp, if (connected) Color.White else Color.DarkGray, CircleShape).padding(6.dp)) {
                    FilledIconButton(onClick = {
                        if (mode == "Sequence") repository.command("sequence_control", buildJsonObject { put("action", "start") })
                        else { repository.command("capture_single"); showCapture = false }
                    }, enabled = connected && !captureActive && !state.snapshot.obj("autofocus").flag("active"), modifier = Modifier.fillMaxSize(), colors = IconButtonDefaults.filledIconButtonColors(containerColor = Color.White, contentColor = Color.Black)) {
                        Icon(if (mode == "Sequence") Icons.Outlined.BurstMode else Icons.Outlined.CameraAlt, if (mode == "Sequence") "Start sequence" else "Take photo")
                    }
                }
                FilledTonalIconButton(onClick = { showCapture = !showCapture }, enabled = state.authenticated, modifier = Modifier.size(54.dp)) { Icon(Icons.Outlined.Cameraswitch, "Switch live view / last capture") }
            }
            if (!landscape) modeSelector()

            Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 4.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                TextButton(onClick = { sheet = "connection" }) { Icon(Icons.Outlined.Wifi, null, Modifier.size(18.dp)); Spacer(Modifier.width(6.dp)); Text("Connection") }
                FilledTonalButton(onClick = { showCapture = false; repository.command("live_view", buildJsonObject { put("enabled", true) }) }, enabled = connected) { Text(if (live) "Show Live" else "Start Live") }
            }
    }
    Surface(color = Color.Black, modifier = Modifier.fillMaxSize()) {
        BoxWithConstraints(Modifier.fillMaxSize().safeDrawingPadding()) {
            val controlHeight = maxHeight * .52f
            if (maxWidth > maxHeight) {
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Column(Modifier.weight(1f).fillMaxHeight()) {
                        header()
                        LivePreview(state, repository, mode, showCapture, showHistogram, { sheet = "connection" }, Modifier.weight(1f))
                    }
                    Column(Modifier.width(300.dp).fillMaxHeight().padding(vertical = 8.dp)) {
                        modeSelector()
                        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) { controls() }
                    }
                }
            } else {
                Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    header()
                    LivePreview(state, repository, mode, showCapture, showHistogram, { sheet = "connection" }, Modifier.weight(1f))
                    Column(Modifier.heightIn(max = controlHeight).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(4.dp)) { controls() }
                }
            }
        }
    }
    when (sheet) {
        "connection" -> ConnectionSheet(state, repository) { sheet = "" }
        "settings" -> SettingsSheet(state, repository) { sheet = "" }
        "gallery" -> GallerySheet(repository) { sheet = "" }
        "logs" -> {
            var text by remember { mutableStateOf("Loading…") }
            LaunchedEffect(Unit) { try { val logs = repository.request("get_logs", buildJsonObject { put("limit", 100) }).jsonObject.array("logs"); text = logs.joinToString("\n\n") { val entry = it.jsonObject; "${entry.text("time")} ${entry.text("level")}\n${entry.text("message")}" } } catch (e: Exception) { text = e.message ?: "Unable to read log" } }
            ModalBottomSheet(onDismissRequest = { sheet = "" }) { Column(Modifier.padding(20.dp).verticalScroll(rememberScrollState())) { Text("PC session log", style = MaterialTheme.typography.titleLarge); Text(text, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(vertical = 20.dp)) } }
        }
    }
}
