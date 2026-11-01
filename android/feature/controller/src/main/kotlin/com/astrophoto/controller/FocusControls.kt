package com.astrophoto.controller

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable internal fun FocusControls(state: RemoteState, repository: RemoteRepository, histogram: Boolean, toggleHistogram: () -> Unit, parameters: () -> Unit) {
    val point = state.snapshot.obj("point_autofocus")
    val stars = state.snapshot.obj("autofocus")
    val selected = if (state.ui.flag("auto_focus_point")) "auto" else if (state.ui.flag("select_focus")) "measure" else "off"
    val steps = state.snapshot.obj("camera_module").array("focus_steps").mapNotNull { it.jsonPrimitive.intOrNull }.filter { it in 1..3 }
    var step by remember { mutableIntStateOf(1) }
    Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("Focus point", style = MaterialTheme.typography.titleSmall)
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            listOf("off" to "Off", "measure" to "Measure", "auto" to "Auto").forEachIndexed { index, (value, title) ->
                SegmentedButton(selected = selected == value, onClick = { repository.pointMode(value) },
                    enabled = state.canChangeCamera && (value != "auto" || (state.canDriveLens && state.snapshot.obj("camera_module").flag("point_autofocus"))),
                    shape = SegmentedButtonDefaults.itemShape(index, 3)) { Text(title) }
            }
        }
        Text(when (selected) {
            "auto" -> "Auto focus point · Tap the image at 1×. OK appears after camera confirmation."
            "measure" -> "Measure point · Tap the image, then adjust focus manually."
            else -> "Point focus is off. Tapping the image will not move the lens."
        }, style = MaterialTheme.typography.bodySmall)
        if (selected == "auto") Text(point.text("detail").ifEmpty { "Ready to select a point" }, style = MaterialTheme.typography.labelMedium)
        OutlinedButton(onClick = { repository.command("point_focus_control", buildJsonObject { put("action", "stop") }) }, enabled = point.flag("active"), modifier = Modifier.fillMaxWidth()) { Text("Stop point AF") }
        HorizontalDivider()
        Text("Lens motor", style = MaterialTheme.typography.titleSmall)
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            listOf("Small", "Medium", "Large").forEachIndexed { index, label ->
                SegmentedButton(selected = step == index + 1, onClick = { step = index + 1 }, enabled = state.canDriveLens && steps.contains(index + 1), shape = SegmentedButtonDefaults.itemShape(index, 3)) { Text(label) }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilledTonalButton(onClick = { repository.command("lens_move", buildJsonObject { put("step", -step) }) }, enabled = state.canDriveLens, modifier = Modifier.weight(1f)) { Text("− Near") }
            FilledTonalButton(onClick = { repository.command("lens_move", buildJsonObject { put("step", step) }) }, enabled = state.canDriveLens, modifier = Modifier.weight(1f)) { Text("+ Far") }
        }
        Text(if (state.canDriveLens) "Lens AF ready · One motor command per tap" else if (state.acquisitionActive || state.busy) "Camera busy · Other actions are locked" else "Enable Live View and set the lens switch to AF", style = MaterialTheme.typography.labelSmall)
        HorizontalDivider()
        Text("Focus peaking", style = MaterialTheme.typography.titleSmall)
        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
            listOf("Off", "Normal", "Stars").forEachIndexed { index, label ->
                val chosen = if (!state.settings.flag("focus_peaking")) index == 0 else if (state.settings.flag("star_peaking")) index == 2 else index == 1
                SegmentedButton(selected = chosen, onClick = { repository.command("update_settings", buildJsonObject { put("patch", buildJsonObject { put("focus_peaking", index != 0); put("star_peaking", index == 2) }) }) }, enabled = state.canChangeCamera, shape = SegmentedButtonDefaults.itemShape(index, 3)) { Text(label) }
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Histogram", modifier = Modifier.padding(top = 12.dp)); Switch(histogram, { toggleHistogram() })
        }
        HorizontalDivider()
        Text("Stellar autofocus", style = MaterialTheme.typography.titleSmall)
        Text("${stars.text("phase", "idle")} · HFD ${stars.text("current", "—")} px", style = MaterialTheme.typography.labelMedium)
        if (stars.text("detail").isNotEmpty()) Text(stars.text("detail"), style = MaterialTheme.typography.bodySmall)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { repository.command("autofocus_control", buildJsonObject { put("action", "start") }) }, enabled = state.canDriveLens && state.snapshot.obj("lens").flag("manual_exposure"), modifier = Modifier.weight(1f)) { Text("Start star AF") }
            OutlinedButton(onClick = { repository.command("autofocus_control", buildJsonObject { put("action", "stop") }) }, enabled = stars.flag("active")) { Text("Stop star AF") }
        }
        OutlinedButton(onClick = parameters, enabled = state.canChangeCamera, modifier = Modifier.fillMaxWidth()) { Text("Star AF parameters") }
    }
}
