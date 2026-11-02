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
        Text(String.format(Locale.US, "Frame HFD %.2f / FWHM %.2f px", stack.text("hfd", "0").toDoubleOrNull() ?: 0.0, stack.text("fwhm", "0").toDoubleOrNull() ?: 0.0), style = MaterialTheme.typography.labelSmall)
        Text(stack.text("detail", "Uses current PC camera settings. RAW, M and fixed ISO required."), style = MaterialTheme.typography.bodySmall)
        val exposure = stack.text("exposure_seconds").toFloatOrNull() ?: 0f
        val elapsed = stack.text("exposure_elapsed").toFloatOrNull() ?: 0f
        if (active && exposure > .25f) {
            LinearProgressIndicator(progress = { if (stack.text("phase") == "exposing") (elapsed / exposure).coerceIn(0f, 1f) else 0f }, modifier = Modifier.fillMaxWidth())
            Text(String.format(Locale.US, "%.1f / %.1f s · %s", elapsed, exposure, stack.text("phase")), style = MaterialTheme.typography.labelSmall)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            FilledTonalButton(onClick = { repository.command("stack_control", buildJsonObject { put("action", "start") }) }, enabled = state.canChangeCamera && (state.snapshot.flag("connected") || options.text("source_directory").isNotBlank())) { Text("Start stack") }
            OutlinedButton(onClick = { repository.command("stack_control", buildJsonObject { put("action", "stop") }) }, enabled = active && !stack.flag("stopping")) { Text("Stop") }
            OutlinedButton(onClick = { repository.command("stack_control", buildJsonObject { put("action", if (stack.flag("pause_requested")) "resume" else "pause") }) }, enabled = active && !stack.flag("stopping") && !state.busy) { Text(if (stack.flag("pause_requested")) "Resume" else "Pause") }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Switch(options.text("frames") == "0", { patch("frames", JsonPrimitive(if (it) 0 else 50)) }, enabled = state.canChangeCamera)
            Text("Until stopped", Modifier.padding(start = 8.dp))
            Box { TextButton(onClick = { presets = true }, enabled = state.canChangeCamera) { Text("${options.text("frames", "50")} ▾") }
                DropdownMenu(presets, { presets = false }) { listOf(50, 75, 100, 200, 300, 500, 700, 1000).forEach { n -> DropdownMenuItem(text = { Text("$n frames") }, onClick = { patch("frames", JsonPrimitive(n)); presets = false }) } }
            }
        }
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) { listOf("Light", "Dark", "Flat", "Bias", "Dark-flat").forEach { type -> FilterChip(selected = options.text("frame_type", "Light") == type, onClick = {
            repository.command("update_settings", buildJsonObject { put("patch", buildJsonObject { put("live_stack", buildJsonObject { put("frame_type", type); if (type != "Light") { put("alignment", false); put("drizzle_scale", 1); put("background_normalization", false); put("gradient_removal", false); listOf("bias_directory", "dark_directory", "flat_directory", "dark_flat_directory").forEach { put(it, "") } } }) }) })
        }, label = { Text(type) }, enabled = state.canChangeCamera) } }
        TextButton(onClick = { expanded = !expanded }) { Text(if (expanded) "Hide stack settings" else "Stack settings") }
        if (expanded) {
            Toggle("Use homogeneous sequence plan", options.flag("from_sequence"), state.canChangeCamera) { patch("from_sequence", JsonPrimitive(it)) }
            Toggle("Rebuild interrupted run on PC startup", options.flag("auto_rebuild"), state.canChangeCamera) { patch("auto_rebuild", JsonPrimitive(it)) }
            Toggle("Star alignment", options.flag("alignment"), state.canChangeCamera) { patch("alignment", JsonPrimitive(it)) }
            Toggle("Affine refinement", options.flag("affine_alignment"), state.canChangeCamera) { patch("affine_alignment", JsonPrimitive(it)) }
            Row { listOf(1.0, 1.5, 2.0, 3.0).forEach { scale -> FilterChip(selected = options.text("drizzle_scale").toDoubleOrNull() == scale, onClick = { patch("drizzle_scale", JsonPrimitive(scale)) }, label = { Text("${scale}x") }, enabled = state.canChangeCamera) } }
            NumberSetting("drizzle_pixfrac", "Drizzle drop fraction", options, .1, 1.0, false, state.canChangeCamera, ::patch)
            Toggle("Float32 TIFF export", options.flag("export_tiff"), state.canChangeCamera) { patch("export_tiff", JsonPrimitive(it)) }
            Toggle("Float32 XISF export", options.flag("export_xisf"), state.canChangeCamera) { patch("export_xisf", JsonPrimitive(it)) }
            Toggle("Online Winsor limits", options.flag("winsorized_rejection"), state.canChangeCamera) { patch("winsorized_rejection", JsonPrimitive(it)) }

            Toggle("Normalize background", options.flag("background_normalization"), state.canChangeCamera) { patch("background_normalization", JsonPrimitive(it)) }
            Toggle("Remove linear gradient", options.flag("gradient_removal"), state.canChangeCamera) { patch("gradient_removal", JsonPrimitive(it)) }
            NumberSetting("solver_fov", "Solver field height degrees (0 = auto)", options, 0.0, 180.0, false, state.canChangeCamera, ::patch)
            NumberSetting("solver_timeout_seconds", "Solver timeout seconds", options, 5.0, 300.0, true, state.canChangeCamera, ::patch)
            NumberSetting("checkpoint_frames", "FITS checkpoint every N frames (0 = off)", options, 0.0, 1000.0, true, state.canChangeCamera, ::patch)
            Text("Calibration: matching CR2 folders on the PC. Flat requires bias or dark-flat. Dark exposure must match.", style = MaterialTheme.typography.bodySmall)
            listOf("solver_executable" to "ASTAP executable on PC (optional)", "solver_database" to "ASTAP star database on PC", "source_directory" to "Rebuild CR2 folder (empty = camera)", "bias_directory" to "Bias folder", "dark_directory" to "Dark folder", "flat_directory" to "Flat folder", "dark_flat_directory" to "Dark-flat folder").forEach { (key, label) ->
                var folder by remember(options[key]) { mutableStateOf(options.text(key)) }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(folder, { folder = it }, Modifier.weight(1f), label = { Text(label) }, enabled = state.canChangeCamera, singleLine = true)
                    TextButton(onClick = { patch(key, JsonPrimitive(folder)) }, enabled = state.canChangeCamera && folder != options.text(key)) { Text("Set") }
                }
            }

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
            NumberSetting("memory_limit_mb", "PC memory limit (MiB)", options, 512.0, 16384.0, true, state.canChangeCamera, ::patch)
            Text("Stop completes the current exposure, downloads the RAW, then saves the linear FITS and report on the PC. Changing camera settings is blocked during acquisition.", style = MaterialTheme.typography.bodySmall)
        }
        var low by remember { mutableFloatStateOf(0f) }; var mid by remember { mutableFloatStateOf(.25f) }; var high by remember { mutableFloatStateOf(1f) }
        var dragging by remember { mutableStateOf(false) }; var asinh by remember { mutableStateOf(false) }
        LaunchedEffect(stack["revision"]) { if (!dragging) { asinh = stack.flag("asinh"); low = stack.text("low", "0").toFloatOrNull() ?: 0f; mid = stack.text("mid", ".25").toFloatOrNull() ?: .25f; high = stack.text("high", "1").toFloatOrNull() ?: 1f } }
        fun apply() { dragging = false; repository.command("stack_stretch", buildJsonObject { put("low", low); put("mid", mid); put("high", high); put("asinh", asinh) }) }
        Text("Display levels · linear FITS stays unchanged", style = MaterialTheme.typography.labelSmall)
        Toggle("Asinh stretch", asinh, state.authenticated && !state.busy) { asinh = it; apply() }
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
        TextButton(onClick = { low = 0f; mid = .25f; high = 1f; asinh = false; apply() }, enabled = state.authenticated && !state.busy) { Text("Reset display") }
        if (stack.text("astrometry").isNotEmpty()) Text(stack.text("astrometry"), style = MaterialTheme.typography.bodySmall)
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
