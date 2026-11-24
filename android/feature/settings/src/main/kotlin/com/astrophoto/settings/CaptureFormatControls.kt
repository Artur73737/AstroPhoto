package com.astrophoto.settings

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

/** Choices are read from the current sensor, never a model-specific table. */
@Composable internal fun CaptureFormatControls(value: JsonObject, capabilities: JsonObject, schema: JsonObject, change: (JsonElement) -> Unit) {
    fun update(name: String, replacement: JsonElement) = change(JsonObject(value + (name to replacement)))
    val formats = capabilities.array("pixel_formats").map { it.jsonObject }
    if (formats.isNotEmpty()) Choice("Sensor format", formats.firstOrNull { it["value"] == value["pixel_format"] }?.text("label") ?: "Read from camera", formats.map { it.text("label") }) { selected ->
        update("pixel_format", formats.first { it.text("label") == selected }["value"]!!)
    }
    val bins = capabilities.array("bins")
    if (bins.isNotEmpty()) Choice("Binning", value.text("bin"), bins.map { it.jsonPrimitive.content }) { selected -> update("bin", bins.first { it.jsonPrimitive.content == selected }) }
    Text("Sensor ${capabilities.text("sensor_width")} × ${capabilities.text("sensor_height")} px · ROI dimensions and offsets are checked by the PC before applying.", style = MaterialTheme.typography.bodySmall)
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        value.filterKeys { (it != "pixel_format" || formats.isEmpty()) && (it != "bin" || bins.isEmpty()) }.forEach { (name, current) ->
            ValueEditor(name, current, schema.obj("properties").obj(name), { update(name, it) })
        }
    }
}
