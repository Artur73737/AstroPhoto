package com.astrophoto.liveview

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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.astrophoto.designsystem.*


import com.astrophoto.network.*
import com.astrophoto.protocol.*

import kotlinx.coroutines.launch
import kotlinx.serialization.json.*
import java.util.Locale

@Composable fun LivePreview(state: RemoteState, repository: RemoteRepository, mode: String, showCapture: Boolean, showHistogram: Boolean, onPair: () -> Unit, modifier: Modifier = Modifier) {
    val connected = state.snapshot.flag("connected")
    val live = state.snapshot.flag("live_view")
            Box(modifier.fillMaxWidth().padding(horizontal = 8.dp).clip(RoundedCornerShape(12.dp)).background(Color(0xFF0B1012)), contentAlignment = Alignment.Center) {
                val image = if (showCapture) state.thumbnail else state.image
                if (image != null) {
                    BoxWithConstraints(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        val ratio = image.width.toFloat() / image.height
                        val width = minOf(maxWidth, maxHeight * ratio); val height = width / ratio
                        Box(Modifier.size(width, height).pointerInput(mode, state.authenticated) {
                            detectTapGestures { point -> if (mode == "Focus" && state.authenticated && !showCapture) repository.command("update_ui", buildJsonObject {
                                put("patch", buildJsonObject { put("select_focus", true); put("focus_target", buildJsonObject { put("enabled", true); put("u", (point.x / size.width).coerceIn(0f, 1f)); put("v", (point.y / size.height).coerceIn(0f, 1f)) }) })
                            }) }
                        }) {
                            Image(image.asImageBitmap(), if (showCapture) "Last photo saved on PC" else "Canon Live View", Modifier.fillMaxSize(), contentScale = ContentScale.Fit)
                            Canvas(Modifier.fillMaxSize()) {
                                for (fraction in listOf(1f / 3, 2f / 3)) {
                                    drawLine(Color.White.copy(alpha = .3f), Offset(size.width * fraction, 0f), Offset(size.width * fraction, size.height), 1f)
                                    drawLine(Color.White.copy(alpha = .3f), Offset(0f, size.height * fraction), Offset(size.width, size.height * fraction), 1f)
                                }
                                if (mode == "Focus") {
                                    val target = state.ui.obj("focus_target")
                                    val u = target.text("u", ".5").toFloatOrNull() ?: .5f; val v = target.text("v", ".5").toFloatOrNull() ?: .5f
                                    drawCircle(AstroCyan, 28.dp.toPx(), Offset(u * size.width, v * size.height), style = Stroke(1.dp.toPx()))
                                }
                            }
                            if (showHistogram) HistogramOverlay(if (showCapture) state.captureAnalysis else state.analysis, Modifier.align(Alignment.TopEnd).padding(8.dp).size(112.dp, 64.dp))
                            if (mode == "Focus" && !showCapture) {
                                val reading = state.analysis.obj("focus")
                                val text = if (reading.flag("valid")) "Point ${reading.text("score")} · Best ${reading.text("best")}\n${if (reading.flag("near_best")) "Near measured best" else "Turn the focus ring to compare"}" else "Tap a point to measure focus"
                                Text(text, style = MaterialTheme.typography.labelSmall, color = AstroCyan, modifier = Modifier.align(Alignment.BottomStart).padding(8.dp).background(Color.Black.copy(alpha = .7f), RoundedCornerShape(8.dp)).padding(8.dp))
                            }
                        }
                    }
                } else Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.padding(24.dp)) {
                    Icon(Icons.Outlined.NightsStay, null, tint = AstroCyan, modifier = Modifier.size(42.dp))
                    Text(if (!state.authenticated) "Your camera, from indoors." else if (!connected) "Connect the Canon on your PC" else if (showCapture) "Your next photo will appear here" else "Live View is stopped", style = MaterialTheme.typography.titleMedium)
                    Text(if (!state.authenticated) "Pair with AstroPhoto on the same local network." else "Photos are stored on your PC.", style = MaterialTheme.typography.bodySmall)
                    FilledTonalButton(onClick = { if (!state.authenticated) onPair() else if (!connected) repository.command("camera_connection", buildJsonObject { put("action", "connect") }) else repository.command("live_view", buildJsonObject { put("enabled", true) }) }) { Text(if (!state.authenticated) "Pair with PC" else if (!connected) "Connect camera" else "Start Live View") }
                }
                Text(if (showCapture) "CAPTURE · PC" else String.format(Locale.US, if (live) "LIVE · %.1f FPS" else "CAPTURING · %.1f FPS", state.fps), color = AstroCyan, style = MaterialTheme.typography.labelSmall, modifier = Modifier.align(Alignment.TopStart).padding(12.dp).background(Color.Black.copy(alpha = .7f), RoundedCornerShape(8.dp)).padding(6.dp))
                Row(Modifier.align(Alignment.BottomCenter).padding(12.dp).background(Color.Black.copy(alpha = .75f), CircleShape).padding(horizontal = 8.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    state.snapshot.obj("camera_module").array("zoom_ratios").forEach { ratio ->
                        val selected = state.snapshot.obj("frame").text("zoom") == ratio.jsonPrimitive.content
                        FilterChip(selected = selected, onClick = { repository.command("live_zoom", json("ratio" to ratio)) }, enabled = live, label = { Text("${ratio.jsonPrimitive.content}×") })
                    }
                }
            }
}
@Composable private fun HistogramOverlay(analysis: JsonObject, modifier: Modifier) {
    val bins = analysis.obj("histogram").array("bins")
    if (bins.isEmpty()) return
    Canvas(modifier.background(Color.Black.copy(alpha = .7f), RoundedCornerShape(8.dp)).padding(6.dp)) {
        bins.take(4).forEachIndexed { channel, item ->
            val values = (item as? JsonArray)?.map { it.jsonPrimitive.doubleOrNull ?: 0.0 } ?: return@forEachIndexed
            val maximum = values.maxOrNull()?.coerceAtLeast(1.0) ?: 1.0
            val path = Path()
            values.forEachIndexed { index, count -> val x = index.toFloat() / (values.size - 1).coerceAtLeast(1) * size.width; val y = size.height * (1 - kotlin.math.ln(1 + count) / kotlin.math.ln(1 + maximum)).toFloat(); if (index == 0) path.moveTo(x, y) else path.lineTo(x, y) }
            drawPath(path, listOf(Color(0xFFFF8D93), Color(0xFFACEEBC), Color(0xFF9CCBFF), Color.White)[channel], style = Stroke(1.dp.toPx()))
        }
    }
}
