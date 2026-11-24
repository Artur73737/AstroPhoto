package com.astrophoto.controller.chrome

import androidx.compose.foundation.layout.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp

/** Only camera chrome lives here: large parameter editors belong to sheets. */
@Composable internal fun CameraWorkspace(
    header: @Composable () -> Unit,
    preview: @Composable (Modifier) -> Unit,
    status: @Composable () -> Unit,
    shutter: @Composable () -> Unit,
    modes: @Composable () -> Unit,
    footer: @Composable () -> Unit,
) {
    BoxWithConstraints(Modifier.fillMaxSize().safeDrawingPadding()) {
        if (maxWidth > maxHeight) {
            Row(Modifier.fillMaxSize()) {
                Column(Modifier.weight(1f).fillMaxHeight()) {
                    header()
                    preview(Modifier.weight(1f).fillMaxWidth().testTag("camera-preview"))
                }
                Column(Modifier.width(232.dp).fillMaxHeight().padding(horizontal = 8.dp), verticalArrangement = Arrangement.SpaceEvenly) {
                    status(); shutter(); modes(); footer()
                }
            }
        } else {
            Column(Modifier.fillMaxSize()) {
                header()
                preview(Modifier.weight(1f).fillMaxWidth().testTag("camera-preview"))
                status(); shutter(); modes(); footer()
            }
        }
    }
}
