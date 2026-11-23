package com.astrophoto.settings

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.astrophoto.network.*
import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

@Composable
internal fun NativeCameraControls(state: RemoteState, repository: RemoteRepository) {
    state.snapshot.array("device_controls").forEach { element ->
        val control = element.jsonObject
        key(control.text("id")) {
            var value by remember { mutableStateOf(control.text("value")) }
            LaunchedEffect(control.text("value")) { value = control.text("value") }
            val minimum = control.text("min").toLongOrNull() ?: Long.MIN_VALUE
            val maximum = control.text("max").toLongOrNull() ?: Long.MAX_VALUE
            val number = value.toLongOrNull()
            val writable = state.canChangeCamera && control.flag("writable")
            fun apply(actual: Long, automatic: Boolean) {
                repository.command("camera_set_native_control", buildJsonObject {
                    put("id", control.text("id").toInt()); put("value", actual); put("automatic", automatic)
                })
            }
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                if (control.flag("writable")) {
                    OutlinedTextField(value, { value = it }, label = { Text(control.text("name")) },
                        supportingText = { Text("$minimum .. $maximum · SDK-native units") },
                        isError = number == null || number !in minimum..maximum,
                        enabled = writable, singleLine = true, modifier = Modifier.fillMaxWidth())
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        TextButton(onClick = { number?.let { apply(it, false) } }, enabled = writable && number != null && number in minimum..maximum) { Text("Apply") }
                        if (control.flag("auto_supported")) {
                            Text("Auto", modifier = Modifier.padding(top = 12.dp))
                            Switch(checked = control.flag("automatic"), onCheckedChange = { automatic ->
                                control.text("value").toLongOrNull()?.let { apply(it, automatic) }
                            }, enabled = writable)
                        }
                    }
                } else Text("${control.text("name")}: ${control.text("value")}", style = MaterialTheme.typography.bodyMedium)
                if (control.text("description").isNotBlank()) Text(control.text("description"), style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
