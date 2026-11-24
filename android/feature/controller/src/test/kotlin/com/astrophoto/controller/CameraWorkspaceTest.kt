package com.astrophoto.controller

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.unit.dp
import com.astrophoto.controller.chrome.*
import com.astrophoto.controller.model.CameraMode
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/** Actual Compose layout and input tests; no camera or network replacement. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w412dp-h892dp")
class CameraWorkspaceTest {
    @get:Rule val compose = createComposeRule()

    private fun workspace() {
        compose.setContent {
            var mode by remember { mutableStateOf(CameraMode.Photo) }
            var opened by remember { mutableStateOf("") }
            MaterialTheme {
                CameraWorkspace(
                    header = { Box(Modifier.fillMaxWidth().height(52.dp)) { Text(opened) } },
                    preview = { Box(it) },
                    status = { Spacer(Modifier.height(48.dp)) },
                    shutter = { Spacer(Modifier.height(88.dp)) },
                    modes = { ModeCarousel(CameraMode.available(false), mode, true) { mode = it } },
                    footer = { CameraFooter(mode, { opened = "General opened" }, { opened = "${mode.title} opened" }, {}, true) },
                )
            }
        }
    }

    @Test fun portraitKeepsPreviewLargeAndSettingsAtOppositeBottomCorners() {
        workspace()
        val preview = compose.onNodeWithTag("camera-preview").fetchSemanticsNode().boundsInRoot
        val left = compose.onNodeWithTag("general-settings").fetchSemanticsNode().boundsInRoot
        val right = compose.onNodeWithTag("mode-settings").fetchSemanticsNode().boundsInRoot
        assertTrue(preview.height > preview.width)
        assertTrue(left.top >= preview.bottom)
        assertEquals(left.top, right.top, 1f)
        assertTrue(left.right < right.left)
        compose.onNodeWithTag("general-settings").performClick()
        compose.onNodeWithText("General opened").assertExists()
        compose.onNodeWithTag("mode-settings").performClick()
        compose.onNodeWithText("Photo opened").assertExists()
    }

    @Test fun modeTapSelectsAndCentersTheRequestedMode() {
        workspace()
        compose.onNodeWithText("Sequence").performClick()
        compose.waitForIdle()
        compose.onNodeWithText("Sequence").assertIsSelected()
        val item = compose.onNodeWithText("Sequence").fetchSemanticsNode().boundsInRoot
        val strip = compose.onNodeWithTag("mode-carousel").fetchSemanticsNode().boundsInRoot
        assertEquals(strip.center.x, item.center.x, 2f)
        compose.onNodeWithTag("mode-settings").performClick()
        compose.onNodeWithText("Sequence opened").assertExists()
    }

    @Test @Config(qualifiers = "w892dp-h412dp-land")
    fun landscapeMovesControlsBesidePreviewWithoutOverlap() {
        workspace()
        val preview = compose.onNodeWithTag("camera-preview").fetchSemanticsNode().boundsInRoot
        val settings = compose.onNodeWithTag("general-settings").fetchSemanticsNode().boundsInRoot
        assertTrue(preview.width > preview.height)
        assertTrue(settings.left >= preview.right)
        compose.onNodeWithTag("mode-settings").assertIsDisplayed().performClick()
        compose.onNodeWithText("Photo opened").assertExists()
    }

    @Test fun swipeChangesModeAndItsSettingsRouteTogether() {
        workspace()
        compose.onNodeWithTag("mode-carousel").performTouchInput { swipeLeft() }
        compose.waitForIdle()
        compose.onNodeWithText("Photo").assertDoesNotExist()
        compose.onNodeWithTag("mode-settings").performClick()
        compose.onNodeWithText("Photo opened").assertDoesNotExist()
    }
}
