package com.astrophoto.settings

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.astrophoto.designsystem.*
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable fun SettingsSheet(state: RemoteState, repository: RemoteRepository, dismiss: () -> Unit) {
    var tab by remember { mutableIntStateOf(0) }
    var draft by remember { mutableStateOf(state.settings) }
    var baseline by remember { mutableStateOf(state.settings) }
    var applying by remember { mutableStateOf(false) }
    LaunchedEffect(state.settings) { if (draft == baseline) { draft = state.settings; baseline = state.settings } }
    LaunchedEffect(state.busy, state.settings, state.error) { if (applying) { if (state.error == null) { draft = state.settings; baseline = state.settings }; applying = false } }
    var uiDraft by remember { mutableStateOf(JsonObject(emptyMap())) }
    ModalBottomSheet(onDismissRequest = dismiss, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true), containerColor = MaterialTheme.colorScheme.surface) {
        Column(Modifier.fillMaxWidth().fillMaxHeight(.88f).padding(horizontal = 20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Camera settings", style = MaterialTheme.typography.headlineSmall)
            SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                listOf("General", "Pro", "All controls").forEachIndexed { i, label -> SegmentedButton(selected = tab == i, onClick = { tab = i }, shape = SegmentedButtonDefaults.itemShape(i, 3)) { Text(label) } }
            }
            Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                when (tab) {
                    0 -> {
                        Section("Camera properties") { CameraProperties(state, repository) }
                        Section("Capture and files on PC") {
                            listOf("single_capture", "sequence", "save_destination", "output_directory", "session_name", "camera_module").forEach { key ->
                                draft[key]?.let { value -> key(key) { ValueEditor(key, value, state.schema.obj("settings").obj("properties").obj(key), { draft = JsonObject(draft + (key to it)) }, template = draft["single_capture"], cameraProperties = state.snapshot.array("properties")) } }
                            }
                        }
                    }
                    1 -> {
                        Text("Changes apply to the PC. Save preferences to retain them after restarting AstroPhoto.", style = MaterialTheme.typography.bodySmall)
                        val excluded = setOf("schema_version", "single_capture", "sequence", "save_destination", "output_directory", "session_name", "camera_module")
                        draft.filterKeys { it !in excluded }.forEach { (name, value) -> key(name) { ValueEditor(name, value, state.schema.obj("settings").obj("properties").obj(name), { draft = JsonObject(draft + (name to it)) }) } }
                        Section("Desktop presentation") {
                            val props = state.schema.obj("ui").obj("properties")
                            props.forEach { (name, schema) -> key(name) { ValueEditor(name, uiDraft[name] ?: schemaValue(state.ui[name] ?: defaultValue(schema.jsonObject), schema.jsonObject), schema.jsonObject, { uiDraft = JsonObject(uiDraft + (name to it)) }) } }
                            Button(onClick = { repository.command("update_ui", json("patch" to uiDraft)) }, enabled = state.authenticated) { Text("Apply desktop presentation") }
                        }
                        Text("Restart PC app for: ${state.schema.array("restart_required_settings").joinToString { it.jsonPrimitive.content.displayName() }}", style = MaterialTheme.typography.bodySmall)
                    }
                    else -> ToolBrowser(state, repository)
                }
            }
            if (tab != 2) Row(Modifier.fillMaxWidth().padding(bottom = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { applying = true; repository.command("update_settings", json("patch" to changedSettings(baseline, draft))) }, enabled = state.authenticated, modifier = Modifier.weight(1f)) { Text("Apply") }
                OutlinedButton(onClick = { repository.command("save_settings") }, enabled = state.authenticated) { Text("Save on PC") }
            }
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(bottom = 12.dp)) }
        }
    }
}

@Composable fun CameraProperties(state: RemoteState, repository: RemoteRepository) {
    val properties = state.snapshot.array("properties")
    if (properties.isEmpty()) Text("Connect the Canon to load supported settings.")
    properties.forEach { element ->
        val property = element.jsonObject
        if (property.array("choices").isEmpty() && property.flag("writable")) {
            var value by remember(property["value"]) { mutableStateOf(property.text("value")) }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(value, { value = it }, label = { Text(property.text("id")) }, singleLine = true, modifier = Modifier.weight(1f))
                TextButton(onClick = { value.toLongOrNull()?.let { number -> repository.command("camera_set_property", buildJsonObject { put("property", property.text("id")); put("value", number) }) } }, enabled = state.authenticated && (value.toLongOrNull() ?: -1L) in 0L..4294967295L) { Text("Apply") }
            }
        } else Choice(property.text("id"), property.text("label"), property.array("choices").map { it.jsonObject.text("label") }, property.flag("writable")) { label ->
            repository.command("camera_set_property", buildJsonObject { put("property", property.text("id")); put("value", property.array("choices").map { it.jsonObject }.first { it.text("label") == label }["value"]!!) })
        }
    }
}

@Composable fun Choice(label: String, current: String, choices: List<String>, enabled: Boolean = true, select: (String) -> Unit) {
    var expanded by remember { mutableStateOf(false) }
    Box {
        OutlinedButton(onClick = { expanded = true }, enabled = enabled && choices.isNotEmpty(), modifier = Modifier.fillMaxWidth()) {
            Text(label.displayName(), Modifier.weight(1f)); Text(current)
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }, modifier = Modifier.heightIn(max = 350.dp)) {
            choices.forEach { item -> DropdownMenuItem(text = { Text(item) }, onClick = { select(item); expanded = false }) }
        }
    }
}

fun defaultValue(schema: JsonObject): JsonElement {
    schema.array("enum").firstOrNull()?.let { return it }
    return when (schema.text("type")) {
        "boolean" -> JsonPrimitive(false)
        "integer", "number" -> schema["minimum"] ?: JsonPrimitive(0)
        "array" -> JsonArray(emptyList())
        "object" -> JsonObject(schema.obj("properties").mapValues { defaultValue(it.value.jsonObject) })
        else -> JsonPrimitive("")
    }
}
fun schemaValue(value: JsonElement, schema: JsonObject): JsonElement = if (value is JsonObject && schema.obj("properties").isNotEmpty()) JsonObject(value.filterKeys { schema.obj("properties").containsKey(it) }.mapValues { (key, child) -> schemaValue(child, schema.obj("properties").obj(key)) }) else value
fun changedSettings(before: JsonObject, after: JsonObject): JsonObject = JsonObject(after.filter { (name, value) -> name != "schema_version" && before[name] != value }.mapValues { (name, value) -> if (value is JsonObject && before[name] is JsonObject) changedSettings(before[name]!!.jsonObject, value) else value } + (before.keys - after.keys).associateWith { JsonNull })

/** Schema-driven fields keep every existing and future desktop setting reachable. */
@Composable fun ValueEditor(name: String, value: JsonElement, schema: JsonObject, change: (JsonElement) -> Unit, template: JsonElement? = null, depth: Int = 0, cameraProperties: JsonArray = JsonArray(emptyList())) {
    val label = name.displayName()
    when {
        name == "settings" && value is JsonObject && cameraProperties.isNotEmpty() -> {
            var expanded by remember { mutableStateOf(false) }
            TextButton(onClick = { expanded = !expanded }) { Text("${if (expanded) "−" else "+"} Exposure properties") }
            if (expanded) cameraProperties.map { it.jsonObject }.filter { it.text("id") in listOf("ISO", "Av", "Tv") }.forEach { property ->
                val propertyName = property.text("id"); val choices = property.array("choices").map { it.jsonObject }
                val current = choices.firstOrNull { it["value"] == value[propertyName] }?.text("label") ?: "Use camera setting"
                Choice(propertyName, current, listOf("Use camera setting") + choices.map { it.text("label") }) { label ->
                    change(if (label == "Use camera setting") JsonObject(value - propertyName) else JsonObject(value + (propertyName to choices.first { it.text("label") == label }["value"]!!)))
                }
            }
        }
        schema.array("enum").isNotEmpty() -> Choice(label, (value as? JsonPrimitive)?.content ?: "", schema.array("enum").map { it.jsonPrimitive.content }) { selected -> change(schema.array("enum").first { it.jsonPrimitive.content == selected }) }
        value is JsonObject -> {
            var expanded by remember { mutableStateOf(false) }
            TextButton(onClick = { expanded = !expanded }, modifier = Modifier.fillMaxWidth()) { Text("${if (expanded) "−" else "+"} $label", Modifier.fillMaxWidth()) }
            if (expanded) Column(Modifier.padding(start = if (depth < 3) 12.dp else 0.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                value.forEach { (child, current) -> key(child) {
                    ValueEditor(child, current, schema.obj("properties").obj(child).ifEmpty { schema.obj("additionalProperties") }, { change(JsonObject(value + (child to it))) }, depth = depth + 1, cameraProperties = cameraProperties)
                    if (schema["additionalProperties"] is JsonObject) TextButton(onClick = { change(JsonObject(value - child)) }) { Text("Remove ${child.displayName()}") }
                } }
                if (schema["additionalProperties"] is JsonObject) {
                    var newKey by remember { mutableStateOf("") }
                    OutlinedTextField(newKey, { newKey = it }, label = { Text("New entry name") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                    TextButton(onClick = { change(JsonObject(value + (newKey to defaultValue(schema.obj("additionalProperties"))))); newKey = "" }, enabled = newKey.isNotBlank() && !value.containsKey(newKey)) { Text("Add entry") }
                }
            }
        }
        value is JsonArray -> {
            var expanded by remember { mutableStateOf(false) }
            TextButton(onClick = { expanded = !expanded }, modifier = Modifier.fillMaxWidth()) { Text("${if (expanded) "−" else "+"} $label · ${value.size}", Modifier.fillMaxWidth()) }
            if (expanded) {
                value.forEachIndexed { index, item -> Column(Modifier.padding(start = 8.dp)) {
                    ValueEditor("Exposure ${index + 1}", item, schema.obj("items"), { replacement -> change(JsonArray(value.mapIndexed { i, old -> if (i == index) replacement else old })) }, depth = depth + 1, cameraProperties = cameraProperties)
                    TextButton(onClick = { change(JsonArray(value.filterIndexed { i, _ -> i != index })) }) { Text("Remove exposure ${index + 1}") }
                } }
                OutlinedButton(onClick = { change(JsonArray(value + (template ?: defaultValue(schema.obj("items"))))) }) { Text("Add exposure") }
            }
        }
        value is JsonPrimitive && (schema.text("type") == "boolean" || (!value.isString && value.booleanOrNull != null)) -> Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) { Text(label, Modifier.weight(1f).padding(top = 12.dp)); Switch(value.boolean, { change(JsonPrimitive(it)) }) }
        else -> {
            var draft by remember(value) { mutableStateOf((value as? JsonPrimitive)?.content ?: "") }
            var invalid by remember { mutableStateOf(false) }
            OutlinedTextField(draft, { text ->
                draft = text
                val parsed = when (schema.text("type")) { "integer" -> text.toLongOrNull()?.let { JsonPrimitive(it) }; "number" -> text.toDoubleOrNull()?.takeIf { it.isFinite() }?.let { JsonPrimitive(it) }; else -> JsonPrimitive(text) }
                invalid = parsed == null
                if (parsed != null) change(parsed)
            }, label = { Text(label) }, isError = invalid, singleLine = true, modifier = Modifier.fillMaxWidth(), supportingText = {
                if (schema.containsKey("minimum") || schema.containsKey("maximum")) Text("${schema.text("minimum", "−∞")} … ${schema.text("maximum", "+∞")}")
            })
        }
    }
}

@Composable private fun ToolBrowser(state: RemoteState, repository: RemoteRepository) {
    var selected by remember { mutableStateOf("get_state") }
    val tools = state.tools.map { it.jsonObject }
    val tool = tools.firstOrNull { it.text("name") == selected }
    Choice("Control", selected.displayName(), tools.map { it.text("name") }) { selected = it }
    if (tool != null) key(selected) {
        val schema = tool.obj("inputSchema")
        var args by remember { mutableStateOf(defaultValue(schema).jsonObject) }
        var result by remember { mutableStateOf("") }
        var running by remember { mutableStateOf(false) }
        val scope = rememberCoroutineScope()
        Text(tool.text("description"), style = MaterialTheme.typography.bodySmall)
        // Optional camera_set_property selectors are mutually exclusive: prefer reported label.
        val visible = schema.obj("properties").filterKeys { selected != "camera_set_property" || it != "value" }
        visible.forEach { (name, property) -> ValueEditor(name, args[name] ?: defaultValue(property.jsonObject), property.jsonObject, { args = JsonObject(args + (name to it)) }) }
        Button(onClick = { running = true; scope.launch { try { val submitted = if (selected == "camera_set_property") JsonObject(args - "value") else args; result = repository.request(selected, submitted).toString(); repository.refresh() } catch (e: Exception) { result = e.message ?: "Request failed" } finally { running = false } } }, enabled = !running) { Text(if (running) "Waiting…" else "Run control") }
        if (result.isNotEmpty()) Text(result.take(16000), style = MaterialTheme.typography.bodySmall)
    }
}
