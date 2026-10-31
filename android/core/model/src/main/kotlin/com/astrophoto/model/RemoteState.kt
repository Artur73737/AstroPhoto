package com.astrophoto.model
import android.graphics.Bitmap
import kotlinx.serialization.json.*
data class RemoteState(
    val status: String = "Not connected", val authenticated: Boolean = false,
    val serverName: String = "", val snapshot: JsonObject = JsonObject(emptyMap()),
    val settings: JsonObject = JsonObject(emptyMap()), val schema: JsonObject = JsonObject(emptyMap()),
    val tools: JsonArray = JsonArray(emptyList()), val ui: JsonObject = JsonObject(emptyMap()),
    val image: Bitmap? = null, val thumbnail: Bitmap? = null, val fps: Float = 0f,
    val logs: JsonElement? = null, val analysis: JsonObject = JsonObject(emptyMap()),
    val captureAnalysis: JsonObject = JsonObject(emptyMap()),
    val error: String? = null, val busy: Boolean = false
)
