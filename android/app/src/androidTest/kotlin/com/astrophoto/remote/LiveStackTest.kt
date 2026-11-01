package com.astrophoto.remote

import android.os.Build
import android.provider.Settings
import androidx.compose.ui.test.*
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.lifecycle.ViewModelProvider
import androidx.test.platform.app.InstrumentationRegistry
import com.astrophoto.network.RemoteRepository
import com.astrophoto.protocol.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import kotlinx.serialization.json.*
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import java.io.File

/** Opt-in: captures three RAW photos with current camera settings, then tests graceful Stop. */
class LiveStackTest {
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()
    private fun SemanticsNodeInteraction.activate() = performSemanticsAction(SemanticsActions.OnClick) { check(it()) }
    @Test fun realRawStackAndGracefulStop() = runBlocking<Unit> {
        assumeTrue(InstrumentationRegistry.getArguments().getString("live_stack") == "true")
        val context=InstrumentationRegistry.getInstrumentation().targetContext
        val fixture=File(context.cacheDir,"stack-pairing.txt")
        assertTrue("One-time pairing fixture is required",fixture.exists())
        val code=PairingCode.parse(fixture.readText().trim());fixture.delete()
        lateinit var repository: RemoteRepository
        compose.runOnUiThread { repository=ViewModelProvider(compose.activity)[ControllerModel::class.java].repository }
        repository.connect(code.address,code.key,Settings.Global.getString(context.contentResolver,Settings.Global.DEVICE_NAME) ?: Build.MODEL)
        withTimeout(15000){repository.state.first{it.authenticated&&it.snapshot.flag("connected")&&it.image!=null}}
        val before=repository.state.value.settings.obj("live_stack")
        val properties=repository.state.value.snapshot.array("properties")
        suspend fun configure(frames: Int) {
            repository.command("update_settings",buildJsonObject { put("patch",buildJsonObject { put("live_stack",buildJsonObject { put("frames",frames);put("alignment",false);put("delay_seconds",2) }) }) })
            withTimeout(15000){repository.state.first{!it.busy&&it.settings.obj("live_stack").text("frames")==frames.toString()&&!it.settings.obj("live_stack").flag("alignment")}}
        }
        fun capture(name: String) { InstrumentationRegistry.getInstrumentation().uiAutomation.takeScreenshot().let { bitmap -> File(context.getExternalFilesDir(null),name).outputStream().use{bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it)};bitmap.recycle() } }
        try {
            compose.onNodeWithText("Stop Live").performScrollTo().activate()
            withTimeout(15000){repository.state.first{!it.busy&&!it.snapshot.flag("live_view")}}
            delay(1800)
            assertFalse("Polling must not re-enable manually stopped Live View",repository.state.value.snapshot.flag("live_view"))
            compose.onNodeWithText("Start Live").performScrollTo().activate()
            withTimeout(15000){repository.state.first{!it.busy&&it.snapshot.flag("live_view")}}
            configure(3)
            compose.onNodeWithText("Stack").activate()
            compose.onNodeWithText("Start stack").performScrollTo().activate()
            withTimeout(15000){repository.state.first{it.snapshot.obj("live_stack").flag("active")||it.error!=null}}
            assertTrue(repository.state.value.error,repository.state.value.snapshot.obj("live_stack").flag("active"))
            assertFalse(repository.state.value.snapshot.flag("live_view"))
            compose.onNodeWithText("Start stack").assertIsNotEnabled()
            // Bypass client-side disabling to validate the PC's authoritative interlock.
            val iso=properties.map{it.jsonObject}.first{it.text("id")=="ISO"}["value"]!!
            val forbidden=repository.request("camera_set_property",buildJsonObject{put("property","ISO");put("value",iso)}).jsonObject
            val id=forbidden["command_id"] ?: forbidden["id"] ?: error("Missing command acknowledgement")
            var rejected=false
            repeat(30){if(!rejected){val ack=repository.request("command_status",buildJsonObject{put("id",id)}).jsonObject;rejected=ack.text("status")=="failed";if(!rejected)delay(100)}}
            assertTrue("PC must reject parameter changes during stacking",rejected)
            withTimeout(300000){repository.state.first{it.snapshot.obj("live_stack").text("accepted","0").toInt()>0&&it.stackImage!=null}}
            assertNotNull(repository.state.value.stackImage)
            repository.command("stack_stretch",buildJsonObject{put("low",0);put("mid",.15);put("high",.9)})
            withTimeout(15000){repository.state.first{!it.busy&&it.snapshot.obj("live_stack").text("mid").toFloatOrNull()==.15f}}
            withTimeout(300000){repository.state.first{!it.snapshot.obj("live_stack").flag("active")}}
            val result=repository.state.value.snapshot.obj("live_stack")
            assertEquals(result.text("detail"),"complete",result.text("phase"))
            assertEquals("3",result.text("accepted"));assertEquals("3",result.text("captured"))
            assertTrue(result.text("final_file").endsWith("stack-linear.fits"))
            withTimeout(10000){repository.state.first{it.snapshot.flag("live_view")}}
            assertEquals(properties,repository.state.value.snapshot.array("properties"))
            File(context.getExternalFilesDir(null),"stack-result.json").writeText(result.toString())
            compose.waitForIdle();capture("stack-portrait.png")
            compose.runOnUiThread{compose.activity.requestedOrientation=android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE}
            withTimeout(10000){while(context.resources.configuration.orientation!=android.content.res.Configuration.ORIENTATION_LANDSCAPE)delay(100)}
            delay(1200);compose.waitForIdle();capture("stack-landscape.png")
            // Infinite mode: stop while an exposure is in progress, not a force abort.
            configure(0)
            compose.onNodeWithText("Start stack").performScrollTo().activate()
            withTimeout(15000){repository.state.first{it.snapshot.obj("live_stack").flag("active")}}
            compose.onNodeWithText("Stop").performScrollTo().activate()
            withTimeout(180000){repository.state.first{!it.snapshot.obj("live_stack").flag("active")}}
            assertEquals("complete",repository.state.value.snapshot.obj("live_stack").text("phase"))
            assertTrue(repository.state.value.snapshot.obj("live_stack").text("captured").toInt() in 1..2)
            println("LIVE STACK PASS: Live View off/on, three real RAWs, authoritative interlock, binary stacked preview, stretch, fixed limit, landscape, infinite Stop; camera properties unchanged")
        } finally {
            if(repository.state.value.snapshot.obj("live_stack").flag("active")){repository.command("stack_control",buildJsonObject{put("action","stop")});withTimeoutOrNull(180000){repository.state.first{!it.snapshot.obj("live_stack").flag("active")}}}
            if(repository.state.value.authenticated)repository.command("update_settings",buildJsonObject{put("patch",buildJsonObject{put("live_stack",before)})})
            compose.runOnUiThread{compose.activity.requestedOrientation=android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED}
        }
    }
}
