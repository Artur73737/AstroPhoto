package com.astrophoto.remote

import android.os.ParcelFileDescriptor
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.semantics.SemanticsActions
import androidx.lifecycle.ViewModelProvider
import androidx.test.platform.app.InstrumentationRegistry
import com.astrophoto.protocol.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import kotlinx.serialization.json.jsonObject
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test

/** Real installed Windows server + phone; deliberately performs no camera action. */
class HostWithoutCameraTest {
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()

    @Test fun authenticatedHostSettingsAndUpdateApiWithoutCamera() = runBlocking {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        assumeTrue(InstrumentationRegistry.getArguments().getString("host_without_camera") == "true")
        val descriptor = instrumentation.uiAutomation.executeShellCommand("cat /data/local/tmp/astrophoto-pairing.txt")
        val payload = ParcelFileDescriptor.AutoCloseInputStream(descriptor).bufferedReader().use { it.readText().trim() }
        val code = PairingCode.parse(payload)
        lateinit var repository: com.astrophoto.network.RemoteRepository
        lateinit var updates: UpdateViewModel
        compose.runOnUiThread {
            repository = ViewModelProvider(compose.activity)[ControllerModel::class.java].repository
            updates = ViewModelProvider(compose.activity)[UpdateViewModel::class.java]
        }
        repository.connect(code.address, code.key, android.os.Build.MODEL)
        withTimeout(20000) { repository.state.first { it.authenticated && it.settings.isNotEmpty() && it.schema.isNotEmpty() } }
        assertTrue(repository.state.value.authenticated)
        assertFalse("This test requires the host without a connected camera", repository.state.value.snapshot.flag("connected"))
        assertTrue(repository.request("get_control_schema").jsonObject.isNotEmpty())
        assertTrue(repository.request("get_settings").jsonObject.obj("config").isNotEmpty())
        compose.onNodeWithTag("general-settings").performClick()
        compose.onAllNodes(SemanticsMatcher.keyIsDefined(SemanticsActions.Expand)).onFirst().performSemanticsAction(SemanticsActions.Expand) { it() }
        compose.waitUntil(30000) { !updates.status.value.checking }
        val started = System.currentTimeMillis()
        compose.onNodeWithText("Check for updates").performScrollTo().performClick()
        val checked = withTimeout(30000) { updates.status.first { !it.checking && (it.checkedAt >= started || it.error != null) } }
        assertNull("Actual GitHub API check failed", checked.error)
        assertNotNull(checked.result)
        assertFalse(checked.result!!.latest.version.isBlank())
        compose.onNodeWithText("App updates").performScrollTo().assertIsDisplayed()
        compose.onNodeWithText("Installed: ${checked.installed}").assertExists()
        compose.onNodeWithText("Latest published APK: ${checked.result!!.latest.version}").assertExists()
        android.util.Log.i("AstroRemoteValidation", "PASS: real Windows pairing without camera, schemas, settings and GitHub API")
        Unit
    }
}
