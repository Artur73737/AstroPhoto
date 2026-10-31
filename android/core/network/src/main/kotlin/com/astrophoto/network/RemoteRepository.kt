package com.astrophoto.network

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Base64
import com.astrophoto.protocol.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import kotlinx.serialization.json.*
import okhttp3.*
import okio.ByteString
import java.net.InetAddress
import java.net.URI
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong

typealias RemoteState = com.astrophoto.model.RemoteState

/** One bounded preview request at a time. Commands are never automatically replayed. */
class RemoteRepository(private val scope: CoroutineScope, private val sessionActive: (Boolean) -> Unit = {}) {
    private val client = OkHttpClient.Builder().pingInterval(15, TimeUnit.SECONDS).readTimeout(0, TimeUnit.MILLISECONDS).build()
    private val mutable = MutableStateFlow(RemoteState())
    val state = mutable.asStateFlow()
    private val pending = ConcurrentHashMap<Long, CompletableDeferred<JsonElement>>()
    private val pendingImages = ConcurrentHashMap<Long, CompletableDeferred<Pair<JsonObject, ByteArray>>>()
    private val ids = AtomicLong()
    private var socket: WebSocket? = null
    private var session: WireSession? = null
    private var polling: Job? = null
    @Volatile private var foreground = true
    private val sendLock = Any()
    @Volatile private var generation = 0
    private var lastCapture = ""
    @Volatile private var previewSource = "raw_live"
    private var pollingError: String? = null

    fun connect(address: String, key: String, device: String) {
        disconnect()
        val epoch = ++generation
        mutable.value = RemoteState(status = "Connecting…"); sessionActive(true)
        scope.launch(Dispatchers.IO) {
            try {
                val uri = URI(address.trim())
                require(uri.scheme == "ws" && uri.host != null && uri.userInfo == null && uri.query == null && uri.fragment == null) { "Use ws://PC-address:port/astrophoto" }
                require(uri.path == "/astrophoto") { "The WebSocket path must be /astrophoto" }
                val resolved = InetAddress.getAllByName(uri.host)
                require(resolved.isNotEmpty() && resolved.all { it.isSiteLocalAddress || it.isLoopbackAddress || it.isLinkLocalAddress }) { "Only local-network servers are supported" }
                val wire = WireSession(key.trim())
                if (epoch != generation) return@launch
                session = wire
                socket = client.newWebSocket(Request.Builder().url(address.trim()).build(), object : WebSocketListener() {
                    override fun onMessage(webSocket: WebSocket, text: String) {
                        if (epoch != generation) return
                        try {
                            require(text.length < 64 * 1024 * 1024) { "Image response is too large" }
                            val envelope = Json.parseToJsonElement(text).jsonObject
                            if (envelope.text("type") == "challenge") {
                                webSocket.send(wire.authenticate(envelope, device.trim()).toString())
                            } else {
                                val payload = wire.receive(envelope)
                                if (payload.text("type") == "authenticated") {
                                    mutable.update { it.copy(status = "Connected", authenticated = true, serverName = payload.text("server_name")) }
                                    startPolling()
                                } else {
                                    val id = payload["id"]?.jsonPrimitive?.longOrNull
                                    val request = pending.remove(id)
                                    if (payload.containsKey("error")) { val error = IllegalStateException(payload.text("error")); request?.completeExceptionally(error); pendingImages.remove(id)?.completeExceptionally(error) }
                                    else request?.complete(payload["result"] ?: JsonNull)
                                }
                            }
                        } catch (e: Exception) { fail(epoch, e.message ?: "Protocol error"); webSocket.close(1008, "Invalid response") }
                    }
                    override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                        if (epoch != generation) return
                        try { val image = wire.receiveImage(bytes.toByteArray()); val id = image.first["id"]!!.jsonPrimitive.long; pendingImages.remove(id)?.complete(image) }
                        catch (e: Exception) { fail(epoch, e.message ?: "Invalid image response"); webSocket.close(1008, "Invalid image response") }
                    }
                    override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) { fail(epoch, t.message ?: "Connection lost") }
                    override fun onClosing(webSocket: WebSocket, code: Int, reason: String) { webSocket.close(code, null); fail(epoch, "Server closed the connection ($code): $reason. Generate a new pairing QR if needed.") }
                    override fun onClosed(webSocket: WebSocket, code: Int, reason: String) { fail(epoch, "Disconnected ($code). Reconnect manually; commands are not replayed.") }
                })
                delay(8000)
                if (epoch == generation && !state.value.authenticated) { socket?.cancel(); fail(epoch, "Pairing timed out. Check the PC server, key and private-network firewall.") }
            } catch (e: Exception) { fail(epoch, e.message ?: "Connection failed") }
        }
    }
    private fun fail(epoch: Int, message: String) {
        if (epoch != generation) return
        sessionActive(false); polling?.cancel(); pending.values.forEach { it.completeExceptionally(IllegalStateException(message)) }; pending.clear()
        pendingImages.values.forEach { it.completeExceptionally(IllegalStateException(message)) }; pendingImages.clear()
        mutable.update { it.copy(authenticated = false, status = "Disconnected", error = message, image = null, fps = 0f, busy = false) }
    }
    fun disconnect() {
        sessionActive(false); generation++; polling?.cancel(); socket?.close(1000, "User disconnected"); socket = null; session = null
        pending.values.forEach { it.cancel() }; pending.clear(); lastCapture = ""; mutable.value = RemoteState()
        pendingImages.values.forEach { it.cancel() }; pendingImages.clear()
    }
    fun close() { disconnect(); client.dispatcher.executorService.shutdown(); client.connectionPool.evictAll() }
    fun foreground(active: Boolean) { foreground = active }
    fun clearError() { mutable.update { it.copy(error = null) } }
    fun processedPreview(enabled: Boolean) { previewSource = if (enabled) "live" else "raw_live" }
    suspend fun request(tool: String, arguments: JsonObject = JsonObject(emptyMap())): JsonElement {
        check(state.value.authenticated) { "Pair with the PC first" }
        val id = ids.incrementAndGet(); val reply = CompletableDeferred<JsonElement>(); pending[id] = reply
        try {
            synchronized(sendLock) {
                val wire = session ?: error("Disconnected")
                check(socket?.send(wire.send(buildJsonObject { put("id", id); put("tool", tool); put("arguments", arguments) })) == true) { "Connection is not writable" }
            }
            return withTimeout(15000) { reply.await() }
        } finally { pending.remove(id) }
    }
    private val imageMutex = kotlinx.coroutines.sync.Mutex()
    private var lastImageRequest = 0L
    suspend fun imageRequest(tool: String, arguments: JsonObject = JsonObject(emptyMap())): Pair<JsonObject, Bitmap?> {
        imageMutex.lock()
        try {
            check(state.value.authenticated) { "Pair with the PC first" }
            delay((130 - (System.nanoTime() - lastImageRequest) / 1_000_000).coerceAtLeast(0)); lastImageRequest = System.nanoTime()
            val id = ids.incrementAndGet(); val reply = CompletableDeferred<Pair<JsonObject, ByteArray>>(); pendingImages[id] = reply
            try {
                synchronized(sendLock) { check(socket?.send(session!!.send(buildJsonObject { put("id", id); put("tool", tool); put("arguments", arguments) })) == true) { "Disconnected" } }
                val (metadata, bytes) = withTimeout(15000) { reply.await() }
                return metadata to decodePreviewBytes(bytes)
            } finally { pendingImages.remove(id) }
        } finally { imageMutex.unlock() }
    }
    fun command(tool: String, arguments: JsonObject = JsonObject(emptyMap())) {
        if (state.value.busy) return
        mutable.update { it.copy(busy = true, error = null) }
        scope.launch {
            try {
                val result = request(tool, arguments)
                val commandId = (result as? JsonObject)?.get("command_id") ?: (result as? JsonObject)?.get("id")
                if (commandId is JsonPrimitive && commandId.longOrNull != null) {
                    var acknowledged = false
                    for (attempt in 0 until 40) {
                        val ack = request("command_status", json("id" to commandId)).jsonObject
                        if (ack.text("status") == "failed" || ack.flag("failed")) error(ack.toString())
                        if (ack.text("status") == "completed") { acknowledged = true; break }
                        delay(100)
                    }
                    if (!acknowledged) error("Command is still queued. Check PC status before retrying; it has not been sent again.")
                }
                refresh()
            } catch (e: CancellationException) { if (e is TimeoutCancellationException) mutable.update { it.copy(error = "Response timed out; command outcome is unknown. Check PC status before retrying.") } else throw e }
            catch (e: Exception) { mutable.update { it.copy(error = e.message) } }
            finally { mutable.update { it.copy(busy = false) } }
        }
    }
    suspend fun refresh() {
        val snapshot = request("get_state").jsonObject
        val settings = request("get_settings").jsonObject
        mutable.update { it.copy(snapshot = snapshot.obj("state"), ui = snapshot.obj("ui"), settings = settings.obj("config")) }
    }
    private fun startPolling() {
        polling?.cancel()
        polling = scope.launch(Dispatchers.IO) {
            try {
                refresh()
                val schema = request("get_control_schema").jsonObject
                val tools = request("tools").jsonArray
                mutable.update { it.copy(schema = schema, tools = tools) }
                var cameraWasConnected = false
                var tick = 0; var frames = 0; var frameWindow = System.nanoTime(); var lastRevision = ""
                while (isActive && state.value.authenticated) {
                    if (!foreground) { val update = request("get_state").jsonObject; mutable.update { it.copy(snapshot = update.obj("state"), ui = update.obj("ui")) }; delay(2000); continue }
                    val started = System.nanoTime()
                    if (tick++ % 5 == 0) {
                        val update = request("get_state").jsonObject
                        mutable.update { it.copy(snapshot = update.obj("state"), ui = update.obj("ui")) }
                        val connectedNow = state.value.snapshot.flag("connected")
                        if (connectedNow && !cameraWasConnected && !state.value.snapshot.flag("live_view") && state.value.snapshot.obj("sequence").text("state") == "Idle" && !state.value.snapshot.obj("autofocus").flag("active")) {
                            request("live_view", buildJsonObject { put("enabled", true) })
                        }
                        cameraWasConnected = connectedNow
                        val capture = state.value.snapshot.obj("last_capture").text("revision")
                        if (capture != "0" && capture.isNotEmpty() && capture != lastCapture) {
                            runCatching { imageRequest("preview_capture").second }.getOrNull()?.let { image ->
                                mutable.update { it.copy(thumbnail = image) }; lastCapture = capture
                                runCatching { request("get_analysis", buildJsonObject { put("source", "capture") }).jsonObject }.getOrNull()?.let { analysis -> mutable.update { it.copy(captureAnalysis = analysis) } }
                            }
                        }
                    }
                    if (tick % 20 == 0) {
                        val settings = request("get_settings").jsonObject.obj("config")
                        mutable.update { it.copy(settings = settings) }
                    }
                    if (state.value.snapshot.flag("live_view")) {
                        val preview = try { imageRequest("preview_live", buildJsonObject { put("processed", previewSource == "live") }) }
                        catch (e: IllegalStateException) {
                            // EVF/processed texture may briefly be unavailable after a property
                            // change or capture. Keep the last frame and retry without UI churn.
                            if (e.message in listOf("No preview available", "No active Live View frame")) { delay(150); continue }
                            throw e
                        }
                        val (metadata, bitmap) = preview
                        val revision = metadata.text("revision")
                        if (revision.isEmpty() || revision != lastRevision) {
                            if (bitmap != null) { frames++; lastRevision = revision; mutable.update { it.copy(image = bitmap, error = if (it.error == pollingError) null else it.error) }; pollingError = null }
                        }
                        if (tick % 10 == 0) runCatching { request("get_analysis", buildJsonObject { put("source", "live") }).jsonObject }.getOrNull()?.let { analysis -> mutable.update { it.copy(analysis = analysis) } }
                    } else mutable.update { it.copy(fps = 0f) }
                    val elapsed = (System.nanoTime() - frameWindow) / 1e9
                    if (elapsed >= 1) { val fps = (frames / elapsed).toFloat(); mutable.update { it.copy(fps = fps) }; frames = 0; frameWindow = System.nanoTime() }
                    delay((125 - (System.nanoTime() - started) / 1_000_000).coerceAtLeast(1))
                }
            } catch (e: CancellationException) { throw e }
            catch (e: Exception) { pollingError = e.message; mutable.update { it.copy(error = e.message) }; if (state.value.authenticated) { delay(1000); startPolling() } }
        }
    }
    suspend fun gallery(): JsonArray = request("gallery_list").jsonObject.array("items")
    suspend fun galleryImage(id: String): Bitmap? = imageRequest("gallery_image", buildJsonObject { put("id", id) }).second
    companion object {
        fun decodeImage(result: JsonElement): Bitmap? {
            val root = result as? JsonObject ?: return null
            val encoded = root.text("data").ifEmpty { root.array("content").mapNotNull { it as? JsonObject }.firstOrNull { it.text("type") == "image" }?.text("data") ?: "" }
            if (encoded.isEmpty()) return null
            val bytes = Base64.decode(encoded, Base64.DEFAULT)
            return decodePreviewBytes(bytes)
        }
        private fun decodePreviewBytes(bytes: ByteArray): Bitmap? {
            val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
            require(bounds.outWidth in 1..16384 && bounds.outHeight in 1..16384) { "Invalid preview dimensions" }
            val options = BitmapFactory.Options().apply { inSampleSize = 1; while (bounds.outWidth / inSampleSize > 2048 || bounds.outHeight / inSampleSize > 2048) inSampleSize *= 2 }
            return BitmapFactory.decodeByteArray(bytes, 0, bytes.size, options)
        }
    }
}
