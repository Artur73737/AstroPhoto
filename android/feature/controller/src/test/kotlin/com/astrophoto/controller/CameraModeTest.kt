package com.astrophoto.controller

import com.astrophoto.controller.model.*
import com.astrophoto.network.RemoteState
import kotlinx.serialization.json.*
import org.junit.Assert.*
import org.junit.Test

class CameraModeTest {
    private fun state(zwo: Boolean = false, busy: Boolean = false, active: String? = null) = RemoteState(authenticated = true, busy = busy, snapshot = buildJsonObject {
        put("connected", true); put("live_view", true)
        put("camera_module", buildJsonObject { put("id", if (zwo) "zwo_asi" else "canon_eos_1100d") })
        active?.let { name -> put(name, buildJsonObject { if(name == "sequence") put("state", "Running") else put("active", true) }) }
    })
    @Test fun availableModesRespectBackendWithoutInventingVideoSupport() {
        assertEquals(listOf(CameraMode.Photo, CameraMode.Sequence, CameraMode.Focus, CameraMode.Stack), CameraMode.available(false))
        assertEquals(listOf(CameraMode.Capture, CameraMode.Focus, CameraMode.Stack), CameraMode.available(true))
        assertEquals(CameraMode.Capture, resolveMode(state(true), CameraMode.Photo))
        assertEquals(CameraMode.Photo, resolveMode(state(), CameraMode.Capture))
    }
    @Test fun nativeRecordingOwnsStopEvenWhileAcknowledgementIsPending() {
        val state = state(true, busy = true, active = "frame_capture")
        assertEquals(CameraMode.Capture, resolveMode(state, CameraMode.Focus))
        val action = shutterAction(state, CameraMode.Focus)
        assertEquals(PrimaryAction.StopCapture, action.action); assertTrue(action.enabled); assertTrue(action.stop)
    }
    @Test fun stackAndAutofocusCannotFallThroughToTakePhoto() {
        assertEquals(PrimaryAction.StopStack, shutterAction(state(active = "live_stack"), CameraMode.Photo).action)
        assertEquals(PrimaryAction.StopStarFocus, shutterAction(state(active = "autofocus"), CameraMode.Photo).action)
        assertEquals(PrimaryAction.StopPointFocus, shutterAction(state(active = "point_autofocus"), CameraMode.Photo).action)
        assertEquals(PrimaryAction.Settings, shutterAction(state(true), CameraMode.Focus).action)
    }
    @Test fun aSinglePhotoDoesNotJumpTheCarouselToSequence() {
        assertEquals(CameraMode.Photo, resolveMode(state(active = "sequence"), CameraMode.Photo))
        assertEquals(PrimaryAction.StopSequence, shutterAction(state(active = "sequence"), CameraMode.Photo).action)
    }

    @Test fun disconnectedOrBusyCameraCannotStartAnExposure() {
        assertFalse(shutterAction(RemoteState(), CameraMode.Photo).enabled)
        assertFalse(shutterAction(state(busy = true), CameraMode.Photo).enabled)
        assertFalse(shutterAction(state(true, busy = true), CameraMode.Capture).enabled)
        assertTrue(shutterAction(state(), CameraMode.Photo).enabled)
    }

    @Test fun nativeFinalizationBlocksRepeatedStopAndNewCapture() {
        val previous = state(true)
        val finishing = previous.copy(snapshot = JsonObject(previous.snapshot + ("frame_capture" to buildJsonObject { put("finishing", true) })))
        assertEquals(CameraMode.Capture, resolveMode(finishing, CameraMode.Stack))
        assertEquals(PrimaryAction.StopCapture, shutterAction(finishing, CameraMode.Stack).action)
        assertFalse(shutterAction(finishing, CameraMode.Stack).enabled)
    }
}
