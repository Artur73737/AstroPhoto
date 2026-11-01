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

/** Opt-in physical AF lens test. No shutter or stellar autofocus commands. */
class PointAutofocusTest {
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()
    private fun SemanticsNodeInteraction.activate() = performSemanticsAction(SemanticsActions.OnClick) { check(it()) }
    @Test fun tapPointWaitForCameraConfirmationAndCancel() = runBlocking<Unit> {
        assumeTrue(InstrumentationRegistry.getArguments().getString("point_af") == "true")
        val context=InstrumentationRegistry.getInstrumentation().targetContext
        val fixture=File(context.cacheDir,"point-pairing.txt")
        assertTrue("Copy the explicit PC one-time pairing fixture to app cache",fixture.exists())
        val code=PairingCode.parse(fixture.readText().trim());fixture.delete()
        lateinit var repository: RemoteRepository
        compose.runOnUiThread { repository=ViewModelProvider(compose.activity)[ControllerModel::class.java].repository }
        repository.connect(code.address,code.key,Settings.Global.getString(context.contentResolver,Settings.Global.DEVICE_NAME) ?: Build.MODEL)
        withTimeout(12000){repository.state.first{it.authenticated||it.error!=null}}
        assertTrue(repository.state.value.error,repository.state.value.authenticated)
        withTimeout(15000){repository.state.first{it.image!=null&&it.snapshot.obj("lens").text("state")=="ready"}}
        compose.onNodeWithText("Focus").activate()
        compose.onNodeWithText("Auto").performScrollTo().activate()
        withTimeout(10000){repository.state.first{it.ui.flag("auto_focus_point")&&it.snapshot.obj("frame").text("zoom")=="1"&&!it.busy}}
        compose.onNodeWithContentDescription("Canon Live View").performTouchInput { click(center) }
        withTimeout(10000){repository.state.first{it.snapshot.obj("point_autofocus").flag("active")}}
        compose.onNodeWithText("− Near").assertIsNotEnabled()
        compose.onNodeWithText("+ Far").assertIsNotEnabled()
        compose.onNodeWithText("Measure").assertIsNotEnabled()
        withTimeout(35000){repository.state.first{!it.snapshot.obj("point_autofocus").flag("active")}}
        val result=repository.state.value.snapshot.obj("point_autofocus")
        assertEquals(result.text("detail"),"complete",result.text("phase"))
        assertTrue(result.text("detail").contains("confirmed by camera"))
        assertTrue(repository.state.value.snapshot.flag("live_view"))
        compose.waitForIdle()
        InstrumentationRegistry.getInstrumentation().uiAutomation.takeScreenshot().let { bitmap ->
            File(context.getExternalFilesDir(null),"point-af-confirmed.png").outputStream().use { bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it) }; bitmap.recycle()
        }
        android.util.Log.i("AstroPointAfValidation","PASS: touch on actual Live View -> camera-confirmed point AF")
        // A second tap starts a fresh operation; stop must not retain the old OK.
        compose.onNodeWithContentDescription("Canon Live View").performTouchInput { click(androidx.compose.ui.geometry.Offset(center.x*.7f,center.y*.9f)) }
        withTimeout(10000){repository.state.first{it.snapshot.obj("point_autofocus").flag("active")}}
        compose.onNodeWithText("Stop point AF").performScrollTo().activate()
        withTimeout(10000){repository.state.first{it.snapshot.obj("point_autofocus").text("phase")=="cancelled"}}
        withTimeout(10000){repository.state.first{!it.busy}}
        assertTrue(repository.state.value.snapshot.flag("live_view"))
        compose.onNodeWithText("Measure").performScrollTo().assertIsEnabled().activate()
        withTimeout(10000){repository.state.first{!it.ui.flag("auto_focus_point")&&it.ui.flag("select_focus")&&!it.busy}}
        android.util.Log.i("AstroPointAfValidation","PASS: stop invalidates OK; manual point measurement remains available")
        compose.onAllNodesWithText("Off")[0].performScrollTo().activate()
        withTimeout(10000){repository.state.first{!it.ui.flag("auto_focus_point")&&!it.ui.flag("select_focus")&&!it.busy}}
        // Parameter sheet is dedicated to AF; it uses typed fields and returns
        // the actual server value after apply. Restore the user's value.
        compose.onNodeWithText("Star AF parameters").performScrollTo().activate()
        val previous = repository.state.value.settings.obj("star_autofocus").text("settle_ms")
        val next = if(previous == "700") "800" else "700"
        compose.onNodeWithText("Settle ms").performScrollTo().performTextReplacement(next)
        compose.onNodeWithText("Apply parameters").activate()
        withTimeout(10000){repository.state.first{!it.busy && it.settings.obj("star_autofocus").text("settle_ms") == next}}
        compose.onNodeWithText("Settle ms").performScrollTo().performTextReplacement(previous)
        compose.onNodeWithText("Apply parameters").activate()
        withTimeout(10000){repository.state.first{!it.busy && it.settings.obj("star_autofocus").text("settle_ms") == previous}}
        androidx.test.espresso.Espresso.closeSoftKeyboard()
        compose.onNodeWithText("Done").activate()
        compose.runOnUiThread { compose.activity.requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE }
        compose.waitForIdle()
        compose.onNodeWithText("Auto").performScrollTo().activate()
        withTimeout(10000){repository.state.first{it.ui.flag("auto_focus_point")&&!it.busy}}
        compose.onNodeWithContentDescription("Canon Live View").performTouchInput { click(center) }
        withTimeout(10000){repository.state.first{it.snapshot.obj("point_autofocus").flag("active")}}
        withTimeout(35000){repository.state.first{it.snapshot.obj("point_autofocus").text("phase")=="complete"}}
        compose.waitForIdle()
        InstrumentationRegistry.getInstrumentation().uiAutomation.takeScreenshot().let { bitmap ->
            File(context.getExternalFilesDir(null),"point-af-landscape.png").outputStream().use { bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it) }; bitmap.recycle()
        }
        compose.runOnUiThread { compose.activity.requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED }
        android.util.Log.i("AstroPointAfValidation","PASS: Off, typed AF parameters round-trip, landscape camera confirmation")
        // Centre native zoom even after an off-centre point selection.
        repository.command("point_focus_control", buildJsonObject { put("action", "start"); put("u", .25); put("v", .3) })
        withTimeout(10000){repository.state.first{!it.busy && kotlin.math.abs((it.snapshot.obj("point_autofocus").text("u").toDoubleOrNull() ?: 1.0)-.25)<.02}}
        repository.command("point_focus_control", buildJsonObject { put("action", "stop") })
        withTimeout(10000){repository.state.first{!it.snapshot.obj("point_autofocus").flag("active")&&!it.busy}}
        repository.pointMode("off")
        withTimeout(10000){repository.state.first{!it.ui.flag("auto_focus_point")&&!it.busy}}
        for(ratio in listOf(5,10,1)) {
            compose.onNodeWithText("${ratio}×").activate()
            withTimeout(10000){repository.state.first{it.snapshot.obj("frame").text("zoom")==ratio.toString()&&!it.busy}}
            delay(400)
            val frame=repository.state.value.snapshot.obj("frame")
            if(ratio>1){
                val rect=frame.array("zoom_rect").map{it.jsonPrimitive.double}
                val x=(rect[0]+rect[2]/2)/frame.text("coordinate_width").toDouble()
                val y=(rect[1]+rect[3]/2)/frame.text("coordinate_height").toDouble()
                assertEquals("Zoom $ratio horizontal centre",.5,x,.01);assertEquals("Zoom $ratio vertical centre",.5,y,.01)
                println("ZOOM $ratio centre=$x,$y rect=$rect")
            }
        }
        val peakingBefore=repository.state.value.settings["focus_peaking"]
        val starBefore=repository.state.value.settings["star_peaking"]
        suspend fun rate(turbo:Boolean):Double {
            repository.command("update_settings",buildJsonObject{put("patch",buildJsonObject{put("turbo_live_stream",turbo)})})
            withTimeout(10000){repository.state.first{!it.busy && it.settings.flag("turbo_live_stream")==turbo}}
            delay(3000)
            val values=mutableListOf<Float>(); val first=repository.state.value
            val started=System.nanoTime()
            repeat(12){delay(1000);values+=repository.state.value.fps}
            val seconds=(System.nanoTime()-started)/1e9
            val last=repository.state.value
            val cameraFps=(last.snapshot.obj("live_performance").text("frames").toDouble()-first.snapshot.obj("live_performance").text("frames").toDouble())/seconds
            val fps=values.average()
            assertEquals(peakingBefore,last.settings["focus_peaking"]);assertEquals(starBefore,last.settings["star_peaking"])
            assertTrue(last.authenticated);assertNull(last.error)
            println("STREAM turbo=$turbo phone_fps=$fps camera_fps=$cameraFps size=${last.image?.width}x${last.image?.height} peaking=$peakingBefore stars=$starBefore")
            return fps
        }
        val normal=rate(false);val turbo=rate(true)
        assertTrue("Turbo must not regress: normal=$normal turbo=$turbo",turbo>=normal*.9)
        compose.waitForIdle()
        InstrumentationRegistry.getInstrumentation().uiAutomation.takeScreenshot().let { bitmap ->
            File(context.getExternalFilesDir(null),"turbo-live.png").outputStream().use { bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it) }; bitmap.recycle()
        }
    }
}
