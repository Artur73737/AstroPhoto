package com.astrophoto.remote

import android.graphics.BitmapFactory
import android.graphics.Bitmap

import android.os.Build
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.provider.Settings
import androidx.compose.ui.test.*
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.lifecycle.ViewModelProvider
import androidx.test.platform.app.InstrumentationRegistry
import com.astrophoto.network.RemoteRepository
import com.astrophoto.protocol.*
import com.google.zxing.*
import com.google.zxing.common.HybridBinarizer
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import kotlinx.serialization.json.*
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

/** Opt-in: reads a real PC QR, uses the real LAN and camera, leaves Live View on.
 * No SDK mocks and no autofocus. Output is kept in a distinct PC test session.
 */
class RemoteHardwareTest {
    private fun SemanticsNodeInteraction.activate() = performSemanticsAction(SemanticsActions.OnClick) { check(it()) }
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()
    @Test fun realQrCameraSettingsCaptureSequenceAndGallery() = runBlocking {
        val args = InstrumentationRegistry.getArguments()
        assumeTrue("Pass hardware=true and copy the PC QR to target cache/pairing.png", args.getString("hardware") == "true")
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val fixture = File(context.cacheDir, "pairing.png")
        assertTrue("Missing PC QR fixture", fixture.exists())
        val bitmap = BitmapFactory.decodeFile(fixture.path); fixture.delete()
        val pixels = IntArray(bitmap.width * bitmap.height); bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
        val qr = MultiFormatReader().decode(BinaryBitmap(HybridBinarizer(RGBLuminanceSource(bitmap.width, bitmap.height, pixels))), mapOf(DecodeHintType.TRY_HARDER to true)).text
        val code = PairingCode.parse(qr)
        lateinit var repository: RemoteRepository
        compose.runOnUiThread { repository = ViewModelProvider(compose.activity)[ControllerModel::class.java].repository }
        val device = Settings.Global.getString(context.contentResolver, Settings.Global.DEVICE_NAME) ?: Build.MODEL
        val wrongKey = (if (code.key[0] == 'a') "b" else "a") + code.key.substring(1)
        repository.connect(code.address, wrongKey, device)
        withTimeout(12000) { repository.state.first { it.error != null } }
        assertFalse("An incorrect pairing key must never authenticate", repository.state.value.authenticated)
        repository.connect(code.address, code.key, device)
        withTimeout(12000) { repository.state.first { it.authenticated || it.error != null } }
        assertTrue(repository.state.value.error, repository.state.value.authenticated)
        withTimeout(15000) { repository.state.first { it.settings.isNotEmpty() && it.schema.isNotEmpty() } }
        val original = repository.state.value.settings
        val originalUi = repository.state.value.ui
        val properties = mutableMapOf<String, JsonElement>()
        suspend fun call(tool: String, args: JsonObject = JsonObject(emptyMap())): JsonObject {
            val result = repository.request(tool, args).jsonObject
            val id = result["id"]?.jsonPrimitive?.longOrNull
            if (id != null) withTimeout(15000) {
                while (true) { val ack = repository.request("command_status", buildJsonObject { put("id", id) }).jsonObject; if (ack.text("status") == "failed") error(ack.text("error")); if (ack.text("status") == "completed") break; delay(100) }
            }
            return result
        }
        suspend fun refresh() { repository.refresh(); delay(200) }
        suspend fun stage(name: String) { android.util.Log.i("AstroRemoteValidation", name) }
        try {
            if (!repository.state.value.snapshot.flag("connected")) call("camera_connection", buildJsonObject { put("action", "connect") })
            call("live_view", buildJsonObject { put("enabled", true) }); refresh()
            try { withTimeout(20000) { repository.state.first { it.image != null } } }
            catch (e: TimeoutCancellationException) { error("Live preview not available: ${repository.state.value.status}; ${repository.state.value.error}; live=${repository.state.value.snapshot.flag("live_view")}") }
            assertTrue(repository.state.value.snapshot.text("model").contains("1100D"))
            stage("PASS: QR challenge-response and real Canon Live View")
            val ui = InstrumentationRegistry.getInstrumentation().uiAutomation
            ui.executeShellCommand("input keyevent KEYCODE_HOME").close()
            delay(35000)
            assertTrue("Pairing survives background across WebSocket ping intervals", repository.state.value.authenticated)
            assertTrue(repository.request("get_state").jsonObject.obj("state").flag("connected"))
            context.startActivity(Intent(context, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT))
            delay(1500)
            stage("PASS: foreground service retains the authenticated background connection")
            compose.waitUntil(10000) { compose.onAllNodesWithText("Canon EOS 1100D").fetchSemanticsNodes().isNotEmpty() }
            delay(2500)
            assertTrue("Actual displayed preview FPS", repository.state.value.fps > 0f && repository.state.value.fps <= 11f)
            android.util.Log.i("AstroRemoteValidation", "Measured phone preview: ${repository.state.value.fps} FPS")
            suspend fun screenshot(name: String) { compose.waitForIdle(); delay(750); val screen = InstrumentationRegistry.getInstrumentation().uiAutomation.takeScreenshot(); assertNotNull("Device screenshot", screen); File(context.cacheDir, name).outputStream().use { screen.compress(Bitmap.CompressFormat.PNG, 100, it) } }
            screenshot("validation-live.png")
            compose.runOnUiThread { compose.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE }
            compose.waitUntil(10000) { compose.activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE }
            screenshot("validation-landscape.png")
            compose.onNodeWithContentDescription("Canon Live View").assertIsDisplayed()
            compose.runOnUiThread { compose.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT }
            compose.waitUntil(10000) { compose.activity.resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT }
            delay(800)
            compose.onNodeWithText("Focus").activate()
            if (repository.state.value.snapshot.obj("lens").text("state") != "ready") {
                compose.onNodeWithText("−").assertIsNotEnabled(); compose.onNodeWithText("+").assertIsNotEnabled()
            }
            for ((mode, stars) in listOf("Peaking Normal" to false, "Peaking Stars" to true)) {
                compose.onNodeWithText(mode).performScrollTo().activate()
                try { withTimeout(10000) { repository.state.first { it.settings.flag("focus_peaking") && it.settings.flag("star_peaking") == stars } } } catch (e: TimeoutCancellationException) { error("Peaking update $mode: ${repository.state.value.error}; busy=${repository.state.value.busy}; focus=${repository.state.value.settings["focus_peaking"]}; stars=${repository.state.value.settings["star_peaking"]}") }
                stage("PASS: UI  applied to PC")
                assertNotNull("Preview retained through setting update", repository.state.value.image)
            }
            screenshot("validation-focus.png")
            compose.onNodeWithText("Photo").performScrollTo().activate()
            for (name in listOf("ISO", "Av", "Tv")) {
                val property = repository.state.value.snapshot.array("properties").map { it.jsonObject }.first { it.text("id") == name }
                properties[name] = property["value"]!!
                val choices = property.array("choices").map { it.jsonObject }
                val choice = if (name == "Tv") choices.first { it.text("label") == "0.5 s" } else choices.first { it["value"] != property["value"] && it.text("label") != "Auto" }
                call("camera_set_property", buildJsonObject { put("property", name); put("value", choice["value"]!!) }); refresh()
                val actual = repository.state.value.snapshot.array("properties").map { it.jsonObject }.first { it.text("id") == name }
                assertEquals("Camera readback for $name", choice["value"], actual["value"])
                stage("PASS: $name write, command acknowledgement and camera readback")
            }
            val zoomWidths = mutableMapOf<Int, Int>()
            for (ratio in repository.state.value.snapshot.obj("camera_module").array("zoom_ratios")) {
                call("live_zoom", json("ratio" to ratio)); delay(500); refresh()
                assertEquals(ratio.jsonPrimitive.content, repository.state.value.snapshot.obj("frame").text("zoom"))
                val zoomImage = repository.imageRequest("preview_live").second
                assertNotNull(zoomImage); zoomWidths[ratio.jsonPrimitive.int] = zoomImage!!.width
            }
            assertTrue("1100D 10x preview must apply the module crop, rather than duplicate 5x", zoomWidths.getValue(10) < zoomWidths.getValue(5))
            call("live_zoom", buildJsonObject { put("ratio", 1) })
            stage("PASS: 1x / 5x / 10x camera zoom and binary JPEG previews")
            call("update_ui", buildJsonObject { put("patch", buildJsonObject { put("focus_target", buildJsonObject { put("enabled", true); put("u", .5); put("v", .5) }); put("histogram", true) }) })
            call("update_settings", buildJsonObject { put("patch", buildJsonObject { put("focus_peaking", true); put("star_peaking", true) }) })
            assertNotNull(repository.imageRequest("preview_live", buildJsonObject { put("processed", true) }).second)
            assertTrue(repository.request("get_analysis", buildJsonObject { put("source", "live") }).jsonObject.flag("available"))
            stage("PASS: peaking, histogram and point-focus controls; star AF deliberately excluded")
            val single = JsonObject(original.obj("single_capture") + mapOf("bulb" to JsonPrimitive(false), "delay_seconds" to JsonPrimitive(0), "interval_seconds" to JsonPrimitive(0), "count" to JsonPrimitive(1)))
            call("update_settings", buildJsonObject { put("patch", buildJsonObject { put("session_name", "android-validation-${System.currentTimeMillis()}"); put("save_destination", "pc_only"); put("single_capture", single) }) })
            val revision = repository.state.value.snapshot.obj("last_capture").text("revision")
            compose.onNodeWithContentDescription("Take photo").performScrollTo().activate()
            withTimeout(45000) { repository.state.first { it.snapshot.obj("last_capture").text("revision") != revision && it.snapshot.obj("last_capture").array("files").isNotEmpty() } }
            assertNotNull(repository.imageRequest("preview_capture").second)
            withTimeout(10000) { repository.state.first { it.snapshot.flag("live_view") && it.image != null } }
            stage("PASS: single exposure safely stops EVF, downloads and restores Live View")
            call("live_view", buildJsonObject { put("enabled", true) })
            val row = JsonObject(single + mapOf("count" to JsonPrimitive(2), "delay_seconds" to JsonPrimitive(0), "interval_seconds" to JsonPrimitive(2), "settings" to JsonObject(emptyMap())))
            call("update_settings", buildJsonObject { put("patch", buildJsonObject { put("sequence", JsonArray(listOf(row))) }) })
            compose.onNodeWithText("Sequence").performScrollTo().activate(); compose.waitForIdle(); delay(500)
            compose.onNodeWithContentDescription("Start sequence").performScrollTo().activate()
            withTimeout(60000) { repository.state.first { it.snapshot.obj("sequence").text("completed") == "2" } }
            withTimeout(10000) { repository.state.first { it.snapshot.flag("live_view") } }
            stage("PASS: two-frame sequence with inter-frame delay and restored Live View")
            call("live_view", buildJsonObject { put("enabled", true) })
            val files = repository.gallery(); assertTrue(files.isNotEmpty())
            assertNotNull(repository.galleryImage(files.first().jsonObject.text("id")))
            stage("PASS: PC gallery listing and binary image preview")
            compose.onNodeWithText("Photo").performScrollTo().activate(); compose.waitForIdle(); delay(500)
            compose.onNodeWithContentDescription("All settings").activate()
            compose.onNodeWithText("Camera settings").assertIsDisplayed()
            compose.onNodeWithText("Pro").activate()
            screenshot("validation-settings.png")
            compose.onNodeWithText("All controls").activate()
            stage("PASS: Compose settings categories rendered on physical Pixel")
            delay(2000)
        } finally {
            runCatching { compose.runOnUiThread { compose.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED } }
            runCatching { call("sequence_control", buildJsonObject { put("action", "stop") }) }
            runCatching { call("update_settings", json("patch" to JsonObject(original.filterKeys { it != "schema_version" }))) }
            for ((name, value) in properties) runCatching { call("camera_set_property", buildJsonObject { put("property", name); put("value", value) }) }
            runCatching { call("live_zoom", buildJsonObject { put("ratio", 1) }) }
            runCatching { call("update_ui", buildJsonObject { put("patch", buildJsonObject { put("select_focus", originalUi.flag("select_focus")); put("view", "live") }) }) }
            runCatching { call("live_view", buildJsonObject { put("enabled", true) }) }
        }
    }
}
