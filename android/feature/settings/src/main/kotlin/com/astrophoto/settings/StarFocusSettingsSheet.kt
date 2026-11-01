package com.astrophoto.settings

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.astrophoto.network.*
import com.astrophoto.designsystem.displayName
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable fun StarFocusSettingsSheet(state: RemoteState, repository: RemoteRepository, dismiss: () -> Unit) {
    val original = state.settings.obj("star_autofocus")
    val schemas = state.schema.obj("settings").obj("properties").obj("star_autofocus").obj("properties")
    val fields = remember(original) { mutableStateMapOf<String, String>().apply { original.forEach { (name, value) -> put(name, value.jsonPrimitive.content) } } }
    fun parsed(name: String): JsonPrimitive? {
        val text = fields[name] ?: return null; val schema = schemas.obj(name)
        val value = when (schema.text("type")) {
            "boolean" -> text.toBooleanStrictOrNull()?.let { JsonPrimitive(it) }
            "integer" -> text.toLongOrNull()?.let { JsonPrimitive(it) }
            "number" -> text.toDoubleOrNull()?.takeIf { it.isFinite() }?.let { JsonPrimitive(it) }
            else -> null
        } ?: return null
        val number = value.doubleOrNull
        if (number != null && (number < (schema["minimum"]?.jsonPrimitive?.doubleOrNull ?: -Double.MAX_VALUE) || number > (schema["maximum"]?.jsonPrimitive?.doubleOrNull ?: Double.MAX_VALUE))) return null
        return value
    }
    val groups = linkedMapOf(
        "Sampling and search" to listOf("positions", "frames_per_point", "min_stars", "settle_ms", "search_enabled", "search_span", "max_passes", "max_stride"),
        "Small-step precision" to listOf("small_frames_per_point", "small_settle_timeout_ms", "small_response_sigma", "small_stability_fraction", "backlash", "backlash_probe_limit"),
        "Travel and acquisition" to fields.keys.filter { it.startsWith("homing_") || it.startsWith("acquisition_") },
        "Detection and validation" to fields.keys.filter { it !in listOf("step", "search_step", "positions", "frames_per_point", "min_stars", "settle_ms", "search_enabled", "search_span", "max_passes", "max_stride", "backlash", "backlash_probe_limit") && !it.startsWith("small_") && !it.startsWith("homing_") && !it.startsWith("acquisition_") }
    )
    ModalBottomSheet(onDismissRequest = dismiss, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)) {
        Column(Modifier.fillMaxWidth().fillMaxHeight(.9f).padding(horizontal = 20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Star AF parameters", style = MaterialTheme.typography.titleLarge)
                TextButton(onClick = dismiss) { Text("Done") }
            }
            Text("Motor sequence: Large → Medium → Small. These parameters affect stellar autofocus, not camera point AF.", style = MaterialTheme.typography.bodySmall)
            Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                groups.forEach { (title, names) ->
                    Text(title, style = MaterialTheme.typography.titleMedium)
                    names.distinct().filter { fields.containsKey(it) }.forEach { name -> key(name) {
                        val schema = schemas.obj(name)
                        if (schema.text("type") == "boolean") Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text(name.displayName(), Modifier.weight(1f).padding(top = 12.dp))
                            Switch(fields[name] == "true", { fields[name] = it.toString() }, enabled = state.canChangeCamera)
                        } else OutlinedTextField(fields[name] ?: "", { fields[name] = it }, label = { Text(name.displayName()) }, singleLine = true,
                            enabled = state.canChangeCamera, isError = parsed(name) == null, modifier = Modifier.fillMaxWidth(),
                            keyboardOptions = KeyboardOptions(keyboardType = if (schema.text("type") == "integer") KeyboardType.Number else KeyboardType.Decimal),
                            supportingText = { Text("${schema.text("minimum", "—")} … ${schema.text("maximum", "—")}") })
                    } }
                }
            }
            Row(Modifier.fillMaxWidth().padding(bottom = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    repository.command("update_settings", buildJsonObject { put("patch", buildJsonObject { put("star_autofocus", JsonObject(fields.keys.associateWith { parsed(it)!! })) }) })
                }, enabled = state.canChangeCamera && fields.isNotEmpty() && fields.keys.all { parsed(it) != null }, modifier = Modifier.weight(1f)) { Text("Apply parameters") }
                OutlinedButton(onClick = { repository.command("save_settings") }, enabled = state.canChangeCamera) { Text("Save on PC") }
            }
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
        }
    }
}
