package com.astrophoto.controller.model

import com.astrophoto.network.*
import com.astrophoto.protocol.*

enum class CameraMode(val title: String) {
    Photo("Photo"), Sequence("Sequence"), Capture("Capture"), Focus("Focus"), Stack("Stack");
    companion object {
        fun available(zwo: Boolean) = if (zwo) listOf(Capture, Focus, Stack) else listOf(Photo, Sequence, Focus, Stack)
    }
}

/** Physical activity owns the selected mode until it ends, keeping Stop reachable. */
fun resolveMode(state: RemoteState, requested: CameraMode): CameraMode = when {
    state.frameRecordingActive -> CameraMode.Capture
    state.snapshot.obj("live_stack").flag("active") -> CameraMode.Stack
    state.captureActive -> if (requested == CameraMode.Photo && !state.isZwo) CameraMode.Photo else CameraMode.Sequence
    state.snapshot.obj("autofocus").flag("active") || state.snapshot.obj("point_autofocus").flag("active") -> CameraMode.Focus
    requested in CameraMode.available(state.isZwo) -> requested
    state.isZwo -> CameraMode.Capture
    else -> CameraMode.Photo
}

enum class PrimaryAction { Photo, Sequence, Capture, Stack, StarFocus, Settings, StopSequence, StopCapture, StopStack, StopStarFocus, StopPointFocus }
data class ShutterAction(val action: PrimaryAction, val label: String, val enabled: Boolean, val stop: Boolean = false)

fun shutterAction(state: RemoteState, mode: CameraMode): ShutterAction {
    if (state.frameRecordingActive) return ShutterAction(PrimaryAction.StopCapture, "Stop saving", !state.snapshot.obj("frame_capture").flag("finishing"), true)
    if (state.snapshot.obj("live_stack").flag("active")) return ShutterAction(PrimaryAction.StopStack, "Stop stack", true, true)
    if (state.captureActive) return ShutterAction(PrimaryAction.StopSequence, "Stop capture", true, true)
    if (state.snapshot.obj("point_autofocus").flag("active")) return ShutterAction(PrimaryAction.StopPointFocus, "Stop point autofocus", true, true)
    if (state.snapshot.obj("autofocus").flag("active")) return ShutterAction(PrimaryAction.StopStarFocus, "Stop stellar autofocus", true, true)
    val ready = state.snapshot.flag("connected") && state.canChangeCamera
    return when (mode) {
        CameraMode.Photo -> ShutterAction(PrimaryAction.Photo, "Take photo", ready && !state.isZwo)
        CameraMode.Sequence -> ShutterAction(PrimaryAction.Sequence, "Start sequence", ready && !state.isZwo)
        CameraMode.Capture -> ShutterAction(PrimaryAction.Capture, "Start saving native frames", ready && state.isZwo)
        CameraMode.Stack -> ShutterAction(PrimaryAction.Stack, "Start live stack", state.canChangeCamera && (state.snapshot.flag("connected") || state.settings.obj("live_stack").text("source_directory").isNotBlank()))
        CameraMode.Focus -> if (state.canDriveLens && state.snapshot.obj("lens").flag("manual_exposure"))
            ShutterAction(PrimaryAction.StarFocus, "Start stellar autofocus", true)
        else ShutterAction(PrimaryAction.Settings, "Focus controls", state.authenticated)
    }
}
