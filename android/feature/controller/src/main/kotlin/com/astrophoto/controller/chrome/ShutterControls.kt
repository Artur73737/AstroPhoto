package com.astrophoto.controller.chrome

import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.astrophoto.controller.model.*
import com.astrophoto.network.*
import com.astrophoto.protocol.*

@Composable internal fun ShutterControls(state: RemoteState, mode: CameraMode, action: ShutterAction, gallery: () -> Unit, secondary: @Composable () -> Unit, perform: () -> Unit) {
    Row(Modifier.fillMaxWidth().height(88.dp).padding(horizontal = 16.dp), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
        IconButton(onClick = gallery, enabled = state.authenticated, modifier = Modifier.size(48.dp).testTag("open-gallery")) {
            val image = state.thumbnail
            if (image == null) Icon(Icons.Outlined.PhotoLibrary, "Photos on PC", tint = Color.White)
            else Image(image.asImageBitmap(), "Photos on PC", Modifier.size(40.dp).clip(RoundedCornerShape(10.dp)))
        }
        val fill = when { !action.enabled -> Color.DarkGray; action.stop -> Color(0xFFE65353); else -> Color.White }
        Box(Modifier.size(78.dp).border(2.dp, if (action.enabled) Color.White else Color.Gray, CircleShape).padding(6.dp), contentAlignment = Alignment.Center) {
            FilledIconButton(onClick = perform, enabled = action.enabled, modifier = Modifier.fillMaxSize().testTag("primary-camera-action"),
                colors = IconButtonDefaults.filledIconButtonColors(containerColor = fill, contentColor = if (action.stop) Color.White else Color.Black, disabledContainerColor = fill)) {
                if (action.stop) Icon(Icons.Outlined.Stop, action.label, Modifier.size(30.dp))
                else Icon(when (mode) { CameraMode.Sequence, CameraMode.Capture -> Icons.Outlined.BurstMode; CameraMode.Stack -> Icons.Outlined.Layers; CameraMode.Focus -> Icons.Outlined.CenterFocusStrong; else -> Icons.Outlined.CameraAlt }, action.label, Modifier.size(27.dp))
            }
        }
        Box(Modifier.size(48.dp), contentAlignment = Alignment.Center) { secondary() }
    }
}

@Composable internal fun CameraFooter(mode: CameraMode, general: () -> Unit, modeSettings: () -> Unit, connection: () -> Unit, authenticated: Boolean) {
    Row(Modifier.fillMaxWidth().height(56.dp).padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
        IconButton(onClick = general, modifier = Modifier.size(48.dp).testTag("general-settings")) { Icon(Icons.Outlined.Settings, "General settings", tint = Color.White) }
        TextButton(onClick = connection) { Text(if (authenticated) "Connection" else "Pair with PC", style = MaterialTheme.typography.labelSmall, color = Color.LightGray) }
        IconButton(onClick = modeSettings, modifier = Modifier.size(48.dp).testTag("mode-settings")) { Icon(Icons.Outlined.Tune, "${mode.title} settings", tint = Color.White) }
    }
}
