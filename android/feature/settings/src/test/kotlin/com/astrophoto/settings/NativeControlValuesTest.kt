package com.astrophoto.settings
import org.junit.Assert.*
import org.junit.Test
class NativeControlValuesTest {
    @Test fun exposureUnitsRetainExactMicrosecondTicks() {
        assertEquals(33333L, nativeRaw("33.333", 1000))
        assertEquals(32L, nativeRaw("0.000032", 1000000))
        assertEquals("0.000032", nativeDisplay(32, 1000000))
        assertEquals(2000000000L, nativeRaw("2000", 1000000))
        assertNull(nativeRaw("NaN")); assertNull(nativeRaw("999999999999999999999999"))
    }
    @Test fun logSliderCoversTheActualCameraRangeAndGainZero() {
        assertEquals(32L, nativeSliderValue(0f, 32, 2000000000, true))
        assertEquals(2000000000L, nativeSliderValue(1f, 32, 2000000000, true))
        assertEquals(0L, nativeSliderValue(0f, 0, 600, false))
        assertEquals(300L, nativeSliderValue(.5f, 0, 600, false))
        val value = nativeSliderValue(nativeFraction(33333, 32, 2000000000, true), 32, 2000000000, true)
        assertTrue(kotlin.math.abs(value - 33333) <= 1)
    }
}
