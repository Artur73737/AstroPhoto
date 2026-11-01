package com.astrophoto.remote

import android.content.pm.ActivityInfo
import android.content.res.Configuration
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Rule
import org.junit.Test

/** No camera actions: check navigation and the persistent stream toggle on the real device. */
class LiveStackLayoutTest {
    @get:Rule val compose = createAndroidComposeRule<MainActivity>()
    @Test fun navigationAndStreamToggleRemainVisibleInBothOrientations() {
        try {
            compose.onNodeWithText("Stack").performClick()
            for (orientation in listOf(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)) {
                compose.runOnUiThread { compose.activity.requestedOrientation = orientation }
                val expected = if (orientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) Configuration.ORIENTATION_PORTRAIT else Configuration.ORIENTATION_LANDSCAPE
                compose.waitUntil(10000) { InstrumentationRegistry.getInstrumentation().targetContext.resources.configuration.orientation == expected }
                compose.waitForIdle()
                listOf("Photo", "Sequence", "Focus", "Stack", "Connection", "Start Live").forEach { compose.onNodeWithText(it).assertIsDisplayed() }
                compose.onNodeWithText("Start stack").assertIsDisplayed().assertIsNotEnabled()
            }
        } finally {
            compose.runOnUiThread { compose.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED }
        }
    }
}
