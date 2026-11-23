package com.astrophoto.network

import com.astrophoto.protocol.*
import kotlinx.serialization.json.*

data class ExposureProgress(val seconds: Double, val elapsed: Double) {
    val fraction: Float get() = if (seconds > 0) (elapsed / seconds).coerceIn(0.0, 1.0).toFloat() else 0f
}

/** Server-reported estimate, interpolated on the phone's own monotonic clock. */
fun RemoteState.streamExposureProgress(nowNanos: Long = System.nanoTime()): ExposureProgress? {
    val exposure = snapshot.obj("stream_exposure")
    val seconds = exposure["seconds"]?.jsonPrimitive?.doubleOrNull ?: return null
    if (!isZwo || !exposure.flag("active") || !seconds.isFinite() || seconds <= .25) return null
    val base = exposure["elapsed_seconds"]?.jsonPrimitive?.doubleOrNull ?: 0.0
    if (!base.isFinite()) return null
    return ExposureProgress(seconds, base.coerceAtLeast(0.0) + (nowNanos - snapshotSampleNanos).coerceAtLeast(0) / 1e9)
}
