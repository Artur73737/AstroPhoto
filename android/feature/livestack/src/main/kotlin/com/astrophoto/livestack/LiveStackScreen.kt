package com.astrophoto.livestack

import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.unit.dp
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*
import java.util.Locale

@Composable fun StackPreview(state: RemoteState, modifier: Modifier = Modifier) {
    val stack = state.snapshot.obj("live_stack")
    Box(modifier.fillMaxWidth().background(Color.Black)) {
        state.stackImage?.let { Image(it.asImageBitmap(), "Accumulated RAW stack", Modifier.fillMaxSize(), contentScale = ContentScale.Fit) }
            ?: Text("The accumulated image appears after the first accepted RAW frame.", Modifier.align(Alignment.Center).padding(24.dp), color = Color.White)
        Column(Modifier.align(Alignment.BottomStart).background(Color.Black.copy(alpha = .8f)).padding(8.dp)) {
            val target = stack.text("target", "0")
            Text(if (target == "0") "${stack.text("accepted", "0")} frames · ${stack.text("captured", "0")} shots" else "${stack.text("accepted", "0")} / $target frames", color = Color.White, style = MaterialTheme.typography.labelMedium)
            Text("${stack.text("rejected", "0")} rejected · ${stack.text("elapsed_seconds", "0").toDoubleOrNull()?.toInt() ?: 0}s elapsed", color = Color.White, style = MaterialTheme.typography.labelSmall)
        }
    }
}

@Composable fun StackControls(state: RemoteState, repository: RemoteRepository) {
    val stack = state.snapshot.obj("live_stack")
    val active = stack.flag("active")
    val options = state.settings.obj("live_stack")
    var expanded by remember { mutableStateOf(false) }
    var presets by remember { mutableStateOf(false) }
    fun patch(key: String, value: JsonElement) = repository.command("update_settings", buildJsonObject { put("patch", buildJsonObject { put("live_stack", buildJsonObject { put(key, value) }) }) })
    Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text("RAW live stacking", style = MaterialTheme.typography.titleMedium)
        Text(stack.text("detail", "Uses current PC camera settings. RAW, M and fixed ISO required."), style = MaterialTheme.typography.bodySmall)
        val exposure = stack.text("exposure_seconds").toFloatOrNull() ?: 0f
        val elapsed = stack.text("exposure_elapsed").toFloatOrNull() ?: 0f
        if (active && exposure > .25f) {
            LinearProgressIndicator(progress = { if (stack.text("phase") == "exposing") (elapsed / exposure).coerceIn(0f, 1f) else 0f }, modifier = Modifier.fillMaxWidth())
            Text(String.format(Locale.US, "%.1f / %.1f s · %s", elapsed, exposure, stack.text("phase")), style = MaterialTheme.typography.labelSmall)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            FilledTonalButton(onClick = { repository.command("stack_control", buildJsonObject { put("action", "start") }) }, enabled = state.canChangeCamera && state.snapshot.flag("connected")) { Text("Start stack") }
            OutlinedButton(onClick = { repository.command("stack_control", buildJsonObject { put("action", "stop") }) }, enabled = active && !stack.flag("stopping")) { Text("Stop") }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Switch(options.text("frames") == "0", { patch("frames", JsonPrimitive(if (it) 0 else 50)) }, enabled = state.canChangeCamera)
            Text("Until stopped", Modifier.padding(start = 8.dp))
            Box { TextButton(onClick = { presets = true }, enabled = state.canChangeCamera) { Text("${options.text("frames", "50")} ▾") }
                DropdownMenu(presets, { presets = false }) { listOf(50, 75, 100, 200, 300, 500, 700, 1000).forEach { n -> DropdownMenuItem(text = { Text("$n frames") }, onClick = { patch("frames", JsonPrimitive(n)); presets = false }) } }
            }
        }
        TextButton(onClick = { expanded = !expanded }) { Text(if (expanded) "Hide stack settings" else "Stack settings") }
        if (expanded) {
            Toggle("Star alignment", options.flag("alignment"), state.canChangeCamera) { patch("alignment", JsonPrimitive(it)) }
            Toggle("Sigma rejection", options.flag("rejection"), state.canChangeCamera) { patch("rejection", JsonPrimitive(it)) }
            Toggle("Correct isolated hot pixels", options.flag("cosmetic_correction"), state.canChangeCamera) { patch("cosmetic_correction", JsonPrimitive(it)) }
            Toggle("Horizontal banding reduction", options.flag("horizontal_banding"), state.canChangeCamera) { patch("horizontal_banding", JsonPrimitive(it)) }
            if (options.flag("horizontal_banding")) {
                NumberSetting("banding_strength", "Banding strength (0–1)", options, 0.0, 1.0, false, state.canChangeCamera, ::patch)
                NumberSetting("banding_radius", "Band radius (rows)", options, 2.0, 256.0, true, state.canChangeCamera, ::patch)
                NumberSetting("banding_max_offset", "Maximum linear correction", options, .0001, .1, false, state.canChangeCamera, ::patch)
            }
            NumberSetting("frames", "Exposure limit (0 = until stopped)", options, 0.0, 100000.0, true, state.canChangeCamera, ::patch)
            NumberSetting("delay_seconds", "Delay after processing (s)", options, 2.0, 3600.0, false, state.canChangeCamera, ::patch)
            NumberSetting("detection_snr", "Detection SNR", options, 3.0, 30.0, false, state.canChangeCamera, ::patch)
            NumberSetting("minimum_stars", "Minimum alignment stars", options, 8.0, 100.0, true, state.canChangeCamera, ::patch)
            NumberSetting("maximum_rms", "Maximum alignment RMS (px)", options, .1, 2.0, false, state.canChangeCamera, ::patch)
            NumberSetting("sigma", "Rejection sigma", options, 3.0, 10.0, false, state.canChangeCamera, ::patch)
            NumberSetting("memory_limit_mb", "PC memory limit (MiB)", options, 512.0, 8192.0, true, state.canChangeCamera, ::patch)
            Text("Stop completes the current exposure, downloads the RAW, then saves the linear FITS and report on the PC. Changing camera settings is blocked during acquisition.", style = MaterialTheme.typography.bodySmall)
        }
        var low by remember { mutableFloatStateOf(0f) }; var mid by remember { mutableFloatStateOf(.25f) }; var high by remember { mutableFloatStateOf(1f) }
        var dragging by remember { mutableStateOf(false) }
        LaunchedEffect(stack["revision"]) { if (!dragging) { low = stack.text("low", "0").toFloatOrNull() ?: 0f; mid = stack.text("mid", ".25").toFloatOrNull() ?: .25f; high = stack.text("high", "1").toFloatOrNull() ?: 1f } }
        fun apply() { dragging = false; repository.command("stack_stretch", buildJsonObject { put("low", low); put("mid", mid); put("high", high) }) }
        Text("Display levels · linear FITS stays unchanged", style = MaterialTheme.typography.labelSmall)
        val bins = stack.array("histogram").map { it.jsonPrimitive.floatOrNull ?: 0f }
        if (bins.size == 256) Canvas(Modifier.fillMaxWidth().height(64.dp).background(MaterialTheme.colorScheme.surfaceContainer)) {
            val maximum = bins.maxOrNull()?.coerceAtLeast(1f) ?: 1f
            for (i in 1..255) drawLine(Color(0xFFB8E3E6), Offset((i - 1) * size.width / 255, size.height * (1 - bins[i - 1] / maximum)), Offset(i * size.width / 255, size.height * (1 - bins[i] / maximum)), 1.5f)
        }
        listOf("Low", "Mid", "High").forEach { name ->
            val value = when(name) { "Low" -> low; "Mid" -> mid; else -> high }
            val range = when(name) { "Low" -> 0f..(high - .0001f).coerceAtLeast(.0001f); "Mid" -> .002f.. .998f; else -> (low + .0001f)..1f }
            Row(verticalAlignment = Alignment.CenterVertically) { Text(name, Modifier.width(40.dp)); Slider(value, { dragging = true; when(name) { "Low" -> low = it; "Mid" -> mid = it; else -> high = it } }, Modifier.weight(1f), enabled = state.authenticated && !state.busy, valueRange = range, onValueChangeFinished = ::apply) }
        }
        TextButton(onClick = { low = 0f; mid = .25f; high = 1f; apply() }, enabled = state.authenticated && !state.busy) { Text("Reset display") }
        if (stack.text("final_file").isNotEmpty()) Text(stack.text("final_file"), style = MaterialTheme.typography.labelSmall)
    }
}
@Composable private fun Toggle(label: String, value: Boolean, enabled: Boolean, update: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) { Text(label, Modifier.weight(1f)); Switch(value, update, enabled = enabled) }
}
@Composable private fun NumberSetting(key: String, label: String, options: JsonObject, min: Double, max: Double, integer: Boolean, enabled: Boolean, update: (String, JsonElement) -> Unit) {
    var text by remember(options[key]) { mutableStateOf(options.text(key)) }
    val number = text.toDoubleOrNull(); val valid = number != null && number.isFinite() && number in min..max && (!integer || number == number.toInt().toDouble())
    Row(verticalAlignment = Alignment.CenterVertically) {
        OutlinedTextField(text, { text = it }, Modifier.weight(1f), label = { Text(label) }, singleLine = true, enabled = enabled, isError = !valid, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal))
        TextButton(onClick = { number?.let { update(key, if (integer) JsonPrimitive(it.toInt()) else JsonPrimitive(it)) } }, enabled = enabled && valid && text != options.text(key)) { Text("Set") }
    }
}
