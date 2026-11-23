package com.astrophoto.network

import kotlinx.serialization.json.*
import kotlin.test.*

class StreamExposureTest {
    private fun state(seconds: Double = .5, active: Boolean = true, module: String = "zwo_asi") = RemoteState(
        authenticated = true, snapshotSampleNanos = 1_000_000_000,
        snapshot = buildJsonObject {
            put("camera_module", buildJsonObject { put("id", module) })
            put("stream_exposure", buildJsonObject { put("seconds", seconds); put("active", active); put("elapsed_seconds", .1) })
        }
    )
    @Test fun interpolatesOwnClockAndClampsUntilNativeReadout() {
        assertEquals(.5f, state().streamExposureProgress(1_150_000_000)!!.fraction)
        assertEquals(1f, state().streamExposureProgress(5_000_000_000)!!.fraction)
        assertEquals(.2f, state().streamExposureProgress(0)!!.fraction)
    }
    @Test fun onlyLongActiveAsiExposuresAppear() {
        assertNull(state(.25).streamExposureProgress())
        assertNull(state(active = false).streamExposureProgress())
        assertNull(state(module = "canon_eos_1100d").streamExposureProgress())
    }
    @Test fun recordingLocksCameraUntilFinalization() {
        val recording = state().copy(snapshot = buildJsonObject {
            put("frame_capture", buildJsonObject { put("active", true); put("finishing", true) })
        })
        assertTrue(recording.frameRecordingActive)
        assertFalse(recording.canChangeCamera)
        assertTrue(state().canChangeCamera)
    }
}
