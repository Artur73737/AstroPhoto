package com.astrophoto.settings

import java.math.BigDecimal
import java.math.RoundingMode
import kotlin.math.*

internal fun nativeDisplay(raw: Long, factor: Long = 1): String =
    BigDecimal.valueOf(raw).divide(BigDecimal.valueOf(factor)).stripTrailingZeros().toPlainString()
internal fun nativeRaw(text: String, factor: Long = 1): Long? = try {
    BigDecimal(text).multiply(BigDecimal.valueOf(factor)).setScale(0, RoundingMode.HALF_UP).longValueExact()
} catch (_: NumberFormatException) { null } catch (_: ArithmeticException) { null }
internal fun nativeFraction(value: Long, minimum: Long, maximum: Long, logarithmic: Boolean): Float {
    if (maximum <= minimum) return 0f
    val v = value.coerceIn(minimum, maximum).toDouble()
    return (if (logarithmic && minimum >= 0)
        (ln1p(v) - ln1p(minimum.toDouble())) / (ln1p(maximum.toDouble()) - ln1p(minimum.toDouble()))
    else (v - minimum) / (maximum.toDouble() - minimum)).toFloat().coerceIn(0f, 1f)
}
internal fun nativeSliderValue(fraction: Float, minimum: Long, maximum: Long, logarithmic: Boolean): Long {
    if (maximum <= minimum) return minimum
    val f = fraction.coerceIn(0f, 1f).toDouble()
    if (f == 0.0) return minimum
    if (f == 1.0) return maximum
    val raw = if (logarithmic && minimum >= 0)
        expm1(ln1p(minimum.toDouble()) + f * (ln1p(maximum.toDouble()) - ln1p(minimum.toDouble())))
    else minimum + f * (maximum.toDouble() - minimum)
    return raw.roundToLong().coerceIn(minimum, maximum)
}
