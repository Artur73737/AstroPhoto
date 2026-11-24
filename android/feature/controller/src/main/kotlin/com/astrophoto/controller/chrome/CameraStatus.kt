package com.astrophoto.controller.chrome

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.astrophoto.controller.model.CameraMode
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.coroutines.delay
import java.util.Locale

@Composable internal fun CameraStatus(state: RemoteState, mode: CameraMode) {
    val recording = state.snapshot.obj("frame_capture")
    val stack = state.snapshot.obj("live_stack")
    val sequence = state.snapshot.obj("sequence")
    val focus = state.snapshot.obj("autofocus")
    val text = when {
        recording.flag("finishing") -> "Finishing disk writes…"
        recording.flag("active") -> "Saving ${recording.text("written", "0")} frames · ${recording.text("elapsed_seconds", "0").toDoubleOrNull()?.toInt() ?: 0}s"
        stack.flag("active") -> "${stack.text("phase")} · ${stack.text("accepted", "0")} stacked · ${stack.text("rejected", "0")} rejected"
        state.captureActive -> "${sequence.text("state")} · ${sequence.text("completed", "0")} / ${sequence.text("total", "0")}"
        mode == CameraMode.Focus -> "${focus.text("phase", "idle")} · HFD ${focus.text("current", "—")} px"
        state.busy -> "Applying camera command…"
        !state.authenticated -> "Pair with the PC to start"
        !state.snapshot.flag("connected") -> "Camera not connected"
        state.isZwo -> "Sensor preview · not saving"
        else -> "${mode.title} · photos stay on the PC"
    }
    var now by remember { mutableLongStateOf(System.nanoTime()) }
    val longExposure = state.streamExposureProgress() != null
    LaunchedEffect(longExposure) { if (longExposure) while (true) { now = System.nanoTime(); delay(33) } }
    Column(Modifier.fillMaxWidth().height(48.dp).padding(horizontal = 16.dp, vertical = 3.dp), verticalArrangement = Arrangement.Center) {
        Text(text, style = MaterialTheme.typography.labelSmall, color = Color.LightGray, maxLines = 1, overflow = TextOverflow.Ellipsis)
        val exposure = state.streamExposureProgress(now)
        if (exposure != null) {
            Text(String.format(Locale.US, "Exposure %.2f / %.2f s · estimated", exposure.elapsed.coerceAtMost(exposure.seconds), exposure.seconds), style = MaterialTheme.typography.labelSmall, color = Color.LightGray, maxLines = 1)
            LinearProgressIndicator(progress = { exposure.fraction }, modifier = Modifier.fillMaxWidth().height(3.dp))
        }
    }
}
