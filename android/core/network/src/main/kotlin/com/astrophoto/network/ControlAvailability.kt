package com.astrophoto.network

import com.astrophoto.protocol.*

/** Shared by all screens; the PC worker also enforces these exclusions. */
val RemoteState.captureActive: Boolean get() = snapshot.obj("sequence").text("state") in
    listOf("Running", "Paused", "Stopping", "In esecuzione", "In pausa", "Arresto")
val RemoteState.isZwo: Boolean get() = snapshot.obj("camera_module").text("id") == "zwo_asi"
val RemoteState.frameRecordingActive: Boolean get() = snapshot.obj("frame_capture").flag("active")
val RemoteState.acquisitionActive: Boolean get() = frameRecordingActive || snapshot.obj("live_stack").flag("active") || captureActive || snapshot.obj("autofocus").flag("active") || snapshot.obj("point_autofocus").flag("active")
val RemoteState.canChangeCamera: Boolean get() = authenticated && !busy && !acquisitionActive
val RemoteState.canDriveLens: Boolean get() = canChangeCamera && snapshot.flag("connected") && snapshot.flag("live_view") && snapshot.obj("lens").text("state") == "ready"
