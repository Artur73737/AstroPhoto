package com.astrophoto.settings.sequence

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.astrophoto.protocol.*
import com.astrophoto.settings.ValueEditor
import com.astrophoto.settings.defaultValue
import kotlinx.serialization.json.*

/** Visible Canon plan editor; editing a draft never starts an exposure. */
@Composable fun SequencePlanEditor(rows: JsonArray, schema: JsonObject, template: JsonElement?, cameraProperties: JsonArray, change: (JsonElement) -> Unit) {
    var selected by remember { mutableIntStateOf(0) }
    Text("Capture plan · ${rows.size} ${if(rows.size == 1) "row" else "rows"}", style = MaterialTheme.typography.titleMedium)
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(onClick = {
            val defaults = defaultValue(schema.obj("items")) as? JsonObject ?: JsonObject(emptyMap())
            val row = JsonObject(defaults + ((template as? JsonObject) ?: JsonObject(emptyMap())))
            selected = rows.size; change(JsonArray(rows + row))
        }) { Text("Add row") }
        if (rows.isNotEmpty()) {
            OutlinedButton(onClick = { val index = selected.coerceIn(rows.indices); change(JsonArray(rows.toMutableList().apply { add(index + 1, rows[index]) })); selected = index + 1 }) { Text("Duplicate row") }
            OutlinedButton(onClick = { val index = selected.coerceIn(rows.indices); change(JsonArray(rows.filterIndexed { i, _ -> i != index })); selected = (index - 1).coerceAtLeast(0) }) { Text("Remove row") }
        }
    }
    if (rows.isEmpty()) Text("Add a row to configure frame type, count, exposure and delays.", style = MaterialTheme.typography.bodySmall)
    rows.forEachIndexed { index, item ->
        val row = item.jsonObject
        FilterChip(selected = index == selected, onClick = { selected = index }, label = { Text("${index + 1} · ${row.text("type")} · ${row.text("count")} frames") })
    }
    if (rows.isNotEmpty()) {
        val index = selected.coerceIn(rows.indices)
        val row = rows[index].jsonObject
        Text("Row ${index + 1}", style = MaterialTheme.typography.titleSmall)
        val ordered = listOf("type", "count", "output_file", "bulb", "exposure_seconds", "delay_seconds", "initial_delay_seconds", "interval_seconds", "settings")
        (ordered.filter { it in row } + (row.keys - ordered.toSet())).forEach { name -> key(index, name) {
            val value = row.getValue(name)
            ValueEditor(name, value, schema.obj("items").obj("properties").obj(name), { replacement ->
                change(JsonArray(rows.mapIndexed { i, original -> if(i == index) JsonObject(row + (name to replacement)) else original }))
            }, cameraProperties = cameraProperties)
        } }
    }
}
