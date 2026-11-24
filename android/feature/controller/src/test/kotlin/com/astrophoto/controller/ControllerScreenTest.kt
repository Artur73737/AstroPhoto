package com.astrophoto.controller

import android.graphics.Bitmap
import android.graphics.Canvas
import androidx.activity.ComponentActivity
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import com.astrophoto.network.RemoteRepository
import com.astrophoto.designsystem.AstroTheme
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import org.junit.After
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/** Disconnected application rendering: no SDK, transport or hardware is simulated. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w412dp-h892dp")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class ControllerScreenTest {
    @get:Rule val compose = createAndroidComposeRule<ComponentActivity>()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    @After fun close() = scope.cancel()

    @Test fun portraitRendersAndOpensGeneralSettings() {
        compose.setContent { AstroTheme { ControllerScreen(RemoteRepository(scope)) } }
        compose.onNodeWithTag("camera-preview").assertIsDisplayed()
        compose.onNodeWithContentDescription("Switch live view and last capture").assertDoesNotExist()
        screenshot("portrait")
        compose.onNodeWithTag("general-settings").performClick()
        compose.onNodeWithText("General settings").assertIsDisplayed()
        compose.onNodeWithText("Composition grid").assertExists()
    }

    @Test @Config(qualifiers = "w892dp-h412dp-land")
    fun landscapeRendersAndOpensSelectedModeSettings() {
        compose.setContent { AstroTheme { ControllerScreen(RemoteRepository(scope)) } }
        compose.onNodeWithTag("camera-preview").assertIsDisplayed()
        screenshot("landscape")
        compose.onNodeWithTag("mode-settings").performClick()
        compose.onNodeWithText("Photo settings").assertIsDisplayed()
        compose.onNodeWithText("Advanced").assertIsDisplayed()
        compose.onNodeWithText("Tools").assertIsDisplayed()
        compose.onNodeWithText("Preview source").assertExists()
    }

    private fun screenshot(name: String) {
        val file = File("build/reports/ui/$name.png")
        file.parentFile.mkdirs()
        compose.runOnIdle {
            val view = compose.activity.window.decorView
            val bitmap = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
            view.draw(Canvas(bitmap))
            file.outputStream().use { bitmap.compress(Bitmap.CompressFormat.PNG, 100, it) }
            bitmap.recycle()
        }
    }
}
