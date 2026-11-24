package com.astrophoto.controller

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createComposeRule
import com.astrophoto.designsystem.AstroTheme
import com.astrophoto.settings.sequence.SequencePlanEditor
import kotlinx.serialization.json.*
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w412dp-h892dp")
class SequenceEditorTest {
    @get:Rule val compose = createComposeRule()
    @Test fun planIsVisibleAndRowsCanBeAddedDuplicatedAndRemoved() {
        compose.setContent {
            var rows by remember { mutableStateOf(JsonArray(emptyList())) }
            val template = buildJsonObject { put("type", "Light"); put("count", 3); put("exposure_seconds", 1); put("delay_seconds", 0); put("interval_seconds", 3); put("bulb", false); put("settings", buildJsonObject {}) }
            AstroTheme { Column(Modifier.verticalScroll(rememberScrollState())) { SequencePlanEditor(rows, buildJsonObject {}, template, JsonArray(emptyList())) { rows = it.jsonArray } } }
        }
        compose.onNodeWithText("Capture plan · 0 rows").assertIsDisplayed()
        compose.onNodeWithText("Add row").performClick()
        compose.onNodeWithText("Row 1").assertIsDisplayed()
        compose.onNodeWithText("Interval seconds").assertExists()
        compose.onNodeWithText("Duplicate row").performClick()
        compose.onNodeWithText("Capture plan · 2 rows").assertExists()
        compose.onNodeWithText("Remove row").performClick()
        compose.onNodeWithText("Capture plan · 1 row").assertExists()
    }
}
