package com.astrophoto.zwocapture

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.coroutines.delay
import kotlinx.serialization.json.*
import java.util.Locale

@Composable fun ZwoCaptureControls(state: RemoteState, repository: RemoteRepository) {
    val current = state.settings.obj("zwo_recording")
    var limit by remember { mutableStateOf("Frames") }
    var count by remember { mutableStateOf("1000") }
    var seconds by remember { mutableStateOf("30") }
    var format by remember { mutableStateOf("ser") }
    var type by remember { mutableStateOf("Light") }
    LaunchedEffect(Unit) {
        val frames = current.text("frames", "1000").toLongOrNull() ?: 1000
        val duration = current.text("seconds", "0").toDoubleOrNull() ?: 0.0
        limit = if (frames > 0) "Frames" else if (duration > 0) "Duration" else "Until stopped"
        count = frames.takeIf { it > 0 }?.toString() ?: "1000"
        seconds = duration.takeIf { it > 0 }?.toString() ?: "30"
        format = current.text("format", "ser"); type = current.text("type", "Light")
    }
    val recording = state.snapshot.obj("frame_capture")
    fun options(frames: Long, time: Double) = buildJsonObject {
        put("frames", frames); put("seconds", time); put("format", format); put("type", type)
        put("memory_mib", current["memory_mib"] ?: JsonPrimitive(256))
    }
    val frames = if (limit == "Frames") count.toLongOrNull() else 0L
    val duration = if (limit == "Duration") seconds.toDoubleOrNull() else 0.0
    val valid = frames != null && frames in 0..4294967295L && (limit != "Frames" || frames > 0) &&
        duration != null && duration.isFinite() && duration in 0.0..86400.0 && (limit != "Duration" || duration > 0)
    Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("ZWO Capture", style = MaterialTheme.typography.titleMedium)
        Text("The sensor stream stays on. Start capture saves its native frames on the PC using the Camera settings.", style = MaterialTheme.typography.bodySmall)
        if (!state.frameRecordingActive) {
        CaptureChoice("Limit", limit, listOf("Frames", "Duration", "Until stopped"), state.canChangeCamera) { limit = it }
        if (limit == "Frames") OutlinedTextField(count, { count = it }, label = { Text("Frames") }, enabled = state.canChangeCamera, singleLine = true, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = Modifier.fillMaxWidth())
        if (limit == "Duration") OutlinedTextField(seconds, { seconds = it }, label = { Text("Duration (seconds)") }, enabled = state.canChangeCamera, singleLine = true, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal), modifier = Modifier.fillMaxWidth())
        CaptureChoice("Frame type", type, listOf("Light", "Dark", "Flat", "Bias"), state.canChangeCamera) { type = it }
        CaptureChoice("Output file", format, listOf("ser", "fits", "png", "tiff"), state.canChangeCamera) { format = it }
        var quick by remember { mutableStateOf(false) }
        Box {
            OutlinedButton(onClick = { quick = true }, enabled = state.canChangeCamera) { Text("Quick capture") }
            DropdownMenu(quick, { quick = false }) {
                DropdownMenuItem(text = { Text("Until stopped") }, onClick = { quick = false; repository.startFrameCapture(options(0, 0.0)) })
                listOf(100L, 200L, 500L, 1000L, 2000L, 3000L, 5000L).forEach { n -> DropdownMenuItem(text = { Text("$n frames") }, onClick = { quick = false; repository.startFrameCapture(options(n, 0.0)) }) }
                listOf(10.0, 20.0, 30.0, 60.0, 90.0, 120.0, 180.0).forEach { time -> DropdownMenuItem(text = { Text("${time.toInt()} seconds") }, onClick = { quick = false; repository.startFrameCapture(options(0, time)) }) }
            }
        }
        }
        Button(onClick = { repository.startFrameCapture(options(frames ?: 0, duration ?: 0.0)) }, enabled = state.canChangeCamera && state.snapshot.flag("connected") && valid, modifier = Modifier.fillMaxWidth()) { Text("Start capture") }
        FilledTonalButton(onClick = { repository.command("frame_capture_control", buildJsonObject { put("action", "stop") }) }, enabled = state.frameRecordingActive && !recording.flag("finishing"), modifier = Modifier.fillMaxWidth()) { Text(if (recording.flag("finishing")) "Finishing disk writes…" else "Stop saving") }
        var clock by remember { mutableLongStateOf(System.nanoTime()) }
        val showExposure = state.streamExposureProgress() != null
        LaunchedEffect(showExposure) { if(showExposure) while (true) { clock = System.nanoTime(); delay(33) } }
        state.streamExposureProgress(clock)?.let { exposure ->
            Text(String.format(Locale.US, "Exposure %.2f / %.2f s", exposure.elapsed.coerceAtMost(exposure.seconds), exposure.seconds), style = MaterialTheme.typography.labelMedium)
            LinearProgressIndicator(progress = { exposure.fraction }, modifier = Modifier.fillMaxWidth())
            Text(if (exposure.fraction >= 1) "Waiting for frame / readout · estimated" else "Estimated from native-frame timing", style = MaterialTheme.typography.labelSmall)
        }
        Text(when { recording.flag("finishing") -> "Finalizing files"; recording.flag("active") -> "Saving native frames"; recording.flag("complete") -> "Capture saved"; else -> "Preview only · not saving" })
        Text("${recording.text("written", "0")} saved · ${recording.text("received", "0")} received", style = MaterialTheme.typography.bodyMedium)
        Text("SDK lost: ${recording.text("sdk_dropped", "0")} · Queue lost: ${recording.text("queue_dropped", "0")}", style = MaterialTheme.typography.labelSmall)
        if (recording.text("error").isNotBlank()) Text(recording.text("error"), color = MaterialTheme.colorScheme.error)
        Text("SER: one multi-frame file. FITS/PNG/TIFF: numbered native frames. Stop saving leaves preview active. Cover the optical path yourself for Dark/Bias.", style = MaterialTheme.typography.bodySmall)
    }
}

@Composable private fun CaptureChoice(label: String, value: String, values: List<String>, enabled: Boolean, onChange: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box(Modifier.fillMaxWidth()) {
        OutlinedButton(onClick = { open = true }, enabled = enabled, modifier = Modifier.fillMaxWidth()) { Text("$label: $value") }
        DropdownMenu(open, { open = false }) { values.forEach { item -> DropdownMenuItem(text = { Text(item) }, onClick = { onChange(item); open = false }) } }
    }
}
