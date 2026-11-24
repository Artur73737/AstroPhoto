package com.astrophoto.remote

import android.os.ParcelFileDescriptor
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.lifecycle.ViewModelProvider
import androidx.test.platform.app.InstrumentationRegistry
import com.astrophoto.protocol.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import kotlinx.serialization.json.*
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test

/** Real Canon + installed host + signed phone app. Edits/restores the plan; no exposures. */
class CanonSequenceRemoteTest {
    private fun SemanticsNodeInteraction.activate() = performSemanticsAction(SemanticsActions.OnClick) { check(it()) }
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()
    @Test fun canonSequenceEditorAppliesAndRestoresRealHostPlan() = runBlocking {
        assumeTrue(InstrumentationRegistry.getArguments().getString("canon_sequence") == "true")
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val code = PairingCode.parse(ParcelFileDescriptor.AutoCloseInputStream(instrumentation.uiAutomation.executeShellCommand("cat /data/local/tmp/astrophoto-pairing.txt")).bufferedReader().use { it.readText().trim() })
        lateinit var repository: com.astrophoto.network.RemoteRepository
        compose.runOnUiThread { repository = ViewModelProvider(compose.activity)[ControllerModel::class.java].repository }
        repository.connect(code.address, code.key, android.os.Build.MODEL)
        withTimeout(20000) { repository.state.first { it.authenticated && it.snapshot.flag("connected") && it.settings.isNotEmpty() && it.schema.isNotEmpty() } }
        assertEquals("canon_eos_1100d", repository.state.value.snapshot.obj("camera_module").text("id"))
        withTimeout(20000) { repository.state.first { it.image != null } }
        val original = repository.state.value.settings["sequence"] ?: JsonArray(emptyList())
        val originalCount = original.jsonArray.size
        try {
            compose.onNodeWithTag("mode-carousel").performTouchInput { swipeRight() }
            compose.waitForIdle()
            compose.onNodeWithText("Sequence").performClick()
            compose.onNodeWithTag("mode-settings").performClick()
            compose.onAllNodes(SemanticsMatcher.keyIsDefined(SemanticsActions.Expand)).onFirst().performSemanticsAction(SemanticsActions.Expand) { it() }
            compose.onNodeWithText("Add row").performScrollTo().activate()
            compose.waitUntil(5000) { compose.onAllNodesWithText("Duplicate row").fetchSemanticsNodes().isNotEmpty() }
            compose.onNodeWithText("Duplicate row").performScrollTo().activate()
            compose.onNodeWithText("Remove row").performScrollTo().activate()
            compose.onNodeWithText("Apply").activate()
            withTimeout(20000) { repository.state.first { !it.busy && it.settings.array("sequence").size == originalCount + 1 } }
            assertTrue(repository.state.value.snapshot.flag("connected"))
            android.util.Log.i("AstroRemoteValidation", "PASS: actual Canon preview and phone Sequence editor Add/Duplicate/Remove/Apply")
        } finally {
            val response = repository.request("update_settings", buildJsonObject { put("patch", buildJsonObject { put("sequence", original) }) }).jsonObject
            val id = response["command_id"] ?: response["id"]
            if(id != null) withTimeout(15000) {
                while(true) {
                    val ack = repository.request("command_status", buildJsonObject { put("id", id) }).jsonObject
                    check(ack.text("status") != "failed") { ack.text("error") }
                    if(ack.text("status") == "completed") break
                    delay(100)
                }
            }
            repository.refresh()
            assertEquals(original, repository.state.value.settings["sequence"])
        }
        Unit
    }
}
