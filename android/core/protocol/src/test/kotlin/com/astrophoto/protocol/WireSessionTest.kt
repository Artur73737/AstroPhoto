package com.astrophoto.protocol

import kotlin.test.*
import kotlinx.serialization.json.*
import java.nio.ByteBuffer
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class WireSessionTest {
    private val key = "a".repeat(64)
    private val nonce = "b".repeat(64)
    private fun ready() = WireSession(key).also { it.authenticate(buildJsonObject { put("type", "challenge"); put("version", 1); put("nonce", nonce) }, "Device from OS") }
    @Test fun matchesRfc4231() { assertEquals("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", WireSession.hmac("\u000b".repeat(20), "Hi There")) }
    @Test fun qrRejectsOtherSchemesCredentialsAndUnexpectedKeys() {
        assertEquals("ws://192.168.1.10:47654/astrophoto", PairingCode.parse("astrophoto-pair:1|ws://192.168.1.10:47654/astrophoto|$key").address)
        listOf("https://192.168.1.10:47654/astrophoto", "ws://user@192.168.1.10:47654/astrophoto", "ws://192.168.1.10:47654/elsewhere").forEach { assertFails { PairingCode.parse("astrophoto-pair:1|$it|$key") } }
        assertFails { PairingCode.parse("astrophoto-pair:1|ws://192.168.1.10:47654/astrophoto|short") }
    }
    @Test fun signedResponsesRejectReplayAndTampering() {
        val session = ready(); val body = "{\"id\":1}"
        val wire = buildJsonObject { put("seq", 1); put("payload", body); put("mac", WireSession.hmac(key, "server\n$nonce\n1\n$body")) }
        assertEquals("1", session.receive(wire).text("id")); assertFails { session.receive(wire) }
        assertFails { ready().receive(JsonObject(wire + ("payload" to JsonPrimitive("{}")))) }
        assertFails { ready().receive(Json.parseToJsonElement(ready().send(buildJsonObject { put("id", 1) })).jsonObject) }
    }
    @Test fun binarySignatureCoversPixelsAndSharesResponseSequence() {
        val session = ready(); val pixels = byteArrayOf(-1, -40, 0, 12, -1, -39)
        val payload = "{\"id\":7,\"mime_type\":\"image/jpeg\"}"
        val mac = Mac.getInstance("HmacSHA256").apply { init(SecretKeySpec(key.toByteArray(), "HmacSHA256")); update("server-image\n$nonce\n1\n$payload\n".toByteArray()) }
        val signature = mac.doFinal(pixels).joinToString("") { "%02x".format(it) }
        val header = buildJsonObject { put("seq", 1); put("payload", payload); put("mac", signature) }.toString().toByteArray()
        val packet = ByteBuffer.allocate(4 + header.size + pixels.size).putInt(header.size).put(header).put(pixels).array()
        assertContentEquals(pixels, session.receiveImage(packet).second)
        assertFails { session.receiveImage(packet) }
        packet[packet.lastIndex] = 0; assertFails { ready().receiveImage(packet) }
    }
}
