package com.astrophoto.settings

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

@Composable
internal fun NativeCameraControls(state: RemoteState, repository: RemoteRepository) {
    state.snapshot.array("device_controls").map { it.jsonObject }
        .groupBy { it.text("category").ifBlank { "Camera controls" } }.forEach { (category, controls) ->
            Text(category, style = MaterialTheme.typography.titleSmall)
            controls.forEach { control -> key(state.snapshot.text("model"), control.text("id")) {
                NativeCameraControl(control, state, repository)
            } }
        }
}

@Composable
private fun NativeCameraControl(control: JsonObject, state: RemoteState, repository: RemoteRepository) {
    val actual = control.text("value").toLongOrNull() ?: return
    val minimum = control.text("min").toLongOrNull() ?: return
    val maximum = control.text("max").toLongOrNull() ?: return
    val duration = control.text("unit") == "us"
    var unit by remember { mutableIntStateOf(if (duration) 1 else 0) }
    val factors = listOf(1L, 1000L, 1000000L)
    val factor = if (duration) factors[unit] else 1L
    var value by remember { mutableStateOf(nativeDisplay(actual, factor)) }
    var dirty by remember { mutableStateOf(false) }
    var logarithmic by remember { mutableStateOf(control.flag("logarithmic")) }
    var choicesOpen by remember { mutableStateOf(false) }
    var unitsOpen by remember { mutableStateOf(false) }
    LaunchedEffect(actual, state.busy, state.error) {
        if (!dirty && !state.busy) value = nativeDisplay(actual, factor)
    }
    val number = nativeRaw(value, factor)
    val writable = state.canChangeCamera && control.flag("writable")
    val valid = number != null && number in minimum..maximum
    fun apply(raw: Long, automatic: Boolean) {
        if (!writable || raw !in minimum..maximum) return
        dirty = false
        repository.command("camera_set_native_control", buildJsonObject {
            put("id", control.text("id").toInt()); put("value", raw); put("automatic", automatic)
        })
    }
    val choices = control.array("choices").map { it.jsonObject }
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(control.text("name"), style = MaterialTheme.typography.labelLarge)
        if (!control.flag("writable")) {
            val scale = control.text("display_scale").toDoubleOrNull() ?: 1.0
            Text("${actual * scale} ${control.text("unit")}", style = MaterialTheme.typography.bodyLarge)
        } else if (choices.isNotEmpty()) {
            Box {
                OutlinedButton(onClick = { choicesOpen = true }, enabled = writable, modifier = Modifier.fillMaxWidth()) {
                    Text(choices.firstOrNull { it.text("value").toLongOrNull() == actual }?.text("label") ?: actual.toString())
                }
                DropdownMenu(expanded = choicesOpen, onDismissRequest = { choicesOpen = false }) {
                    choices.forEach { choice ->
                        val raw = choice.text("value").toLongOrNull()
                        if (raw != null && raw in minimum..maximum) DropdownMenuItem(text = { Text(choice.text("label")) },
                            onClick = { choicesOpen = false; apply(raw, false) })
                    }
                }
            }
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(value, { value = it; dirty = true }, label = { Text("Value") },
                    supportingText = { Text("$minimum .. $maximum ${control.text("unit")}") },
                    isError = !valid, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    enabled = writable, singleLine = true, modifier = Modifier.weight(1f))
                if (duration) Box {
                    OutlinedButton(onClick = { unitsOpen = true }) { Text(listOf("us", "ms", "s")[unit]) }
                    DropdownMenu(expanded = unitsOpen, onDismissRequest = { unitsOpen = false }) {
                        listOf("us", "ms", "s").forEachIndexed { index, label -> DropdownMenuItem(text = { Text(label) },
                            onClick = { val raw = nativeRaw(value, factor) ?: actual; unit = index; value = nativeDisplay(raw, factors[index]); unitsOpen = false }) }
                    }
                }
            }
            if (maximum > minimum) Slider(value = nativeFraction(number ?: actual, minimum, maximum, logarithmic),
                onValueChange = { value = nativeDisplay(nativeSliderValue(it, minimum, maximum, logarithmic), factor); dirty = true },
                onValueChangeFinished = { nativeRaw(value, factor)?.let { apply(it, false) } }, enabled = writable,
                modifier = Modifier.fillMaxWidth())
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = { number?.let { apply(it, false) } }, enabled = writable && valid && dirty) { Text("Apply") }
                FilterChip(selected = logarithmic, onClick = { logarithmic = !logarithmic },
                    label = { Text(if (logarithmic) "Log scale" else "Linear scale") }, enabled = minimum >= 0)
            }
        }
        if (control.flag("auto_supported")) Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Auto", modifier = Modifier.padding(top = 12.dp))
            Switch(checked = control.flag("automatic"), onCheckedChange = { apply(actual, it) }, enabled = writable)
        }
        Text("SDK: $actual ${if ((control.text("display_scale").toDoubleOrNull() ?: 1.0) == 1.0) control.text("unit") else "raw"}", style = MaterialTheme.typography.bodySmall)
        if (control.text("description").isNotBlank()) Text(control.text("description"), style = MaterialTheme.typography.bodySmall)
        HorizontalDivider(Modifier.padding(vertical = 4.dp))
    }
}