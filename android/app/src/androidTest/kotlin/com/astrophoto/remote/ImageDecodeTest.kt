package com.astrophoto.remote
import android.graphics.Bitmap
import android.util.Base64
import com.astrophoto.network.RemoteRepository
import kotlinx.serialization.json.*
import org.junit.Assert.*
import org.junit.Test
import java.io.ByteArrayOutputStream
class ImageDecodeTest {
    @Test fun smallPreviewUsesSampleSizeOneAndLargePreviewIsBounded() {
        for (width in listOf(8, 4096)) {
            val source = Bitmap.createBitmap(width, 16, Bitmap.Config.ARGB_8888)
            val bytes = ByteArrayOutputStream().also { source.compress(Bitmap.CompressFormat.JPEG, 80, it) }.toByteArray()
            val decoded = RemoteRepository.decodeImage(buildJsonObject { put("data", Base64.encodeToString(bytes, Base64.NO_WRAP)) })
            assertNotNull(decoded); assertTrue(decoded!!.width in 1..2048)
            if (width == 8) assertEquals(8, decoded.width)
        }
    }
}
