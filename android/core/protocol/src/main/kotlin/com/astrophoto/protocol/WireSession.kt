package com.astrophoto.protocol

import kotlinx.serialization.json.*
import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec
import java.nio.ByteBuffer

fun JsonObject.text(key: String, fallback: String = ""): String = (get(key) as? JsonPrimitive)?.contentOrNull ?: if (key == "type") (get(key) as? JsonArray)?.firstOrNull()?.jsonPrimitive?.content ?: fallback else fallback
fun JsonObject.flag(key: String) = (get(key) as? JsonPrimitive)?.booleanOrNull ?: false
fun JsonObject.obj(key: String) = get(key) as? JsonObject ?: JsonObject(emptyMap())
fun JsonObject.array(key: String) = get(key) as? JsonArray ?: JsonArray(emptyList())
fun json(vararg values: Pair<String, JsonElement>) = JsonObject(mapOf(*values))

/** Identical wire contract to the C++ host. No pairing secret is sent on the LAN. */
class WireSession(private val key: String) {
    private var nonce = ""
    private var sent = 0L
    private var received = 0L
    fun authenticate(challenge: JsonObject, device: String): JsonObject {
        require(challenge.text("type") == "challenge" && challenge.text("version") == "1") { "Unsupported server protocol" }
        require(key.matches(Regex("[a-fA-F0-9]{64}"))) { "Enter the 64-character pairing key from the PC" }
        require(device.isNotBlank() && device.toByteArray().size <= 96 && device.none { it.code < 32 || it.code == 127 }) { "Invalid device name" }
        nonce = challenge.text("nonce")
        require(nonce.matches(Regex("[a-f0-9]{64}"))) { "Invalid server challenge" }
        return buildJsonObject {
            put("type", "authenticate"); put("device_name", device)
            put("proof", hmac(key, "auth\n$nonce\n$device"))
        }
    }
    @Synchronized fun send(payload: JsonObject): String {
        check(nonce.isNotEmpty())
        val body = payload.toString(); val sequence = ++sent
        return buildJsonObject {
            put("seq", sequence); put("payload", body)
            put("mac", hmac(key, "client\n$nonce\n$sequence\n$body"))
        }.toString()
    }
    @Synchronized fun receive(wire: JsonObject): JsonObject {
        val sequence = wire["seq"]?.jsonPrimitive?.long ?: error("Missing response sequence")
        require(sequence == received + 1) { "Replayed or out-of-order server response" }
        val body = wire.text("payload")
        require(body.length <= 48 * 1024 * 1024) { "Server response exceeds image budget" }
        val expected = hmac(key, "server\n$nonce\n$sequence\n$body")
        require(MessageDigest.isEqual(expected.toByteArray(), wire.text("mac").toByteArray())) { "Server authentication failed" }
        val result = Json.parseToJsonElement(body).jsonObject
        received = sequence
        return result
    }
    @Synchronized fun receiveImage(packet: ByteArray): Pair<JsonObject, ByteArray> {
        require(packet.size in 5..(4 * 1024 * 1024 + 65536)) { "Invalid binary preview size" }
        val headerSize = ByteBuffer.wrap(packet, 0, 4).int
        require(headerSize in 1..65536 && headerSize + 4 < packet.size) { "Invalid image header" }
        val header = Json.parseToJsonElement(packet.copyOfRange(4, 4 + headerSize).toString(Charsets.UTF_8)).jsonObject
        val sequence = header["seq"]!!.jsonPrimitive.long
        require(sequence == received + 1) { "Replayed binary frame" }
        val body = header.text("payload")
        val image = packet.copyOfRange(4 + headerSize, packet.size)
        val mac = Mac.getInstance("HmacSHA256").apply { init(SecretKeySpec(key.toByteArray(Charsets.UTF_8), "HmacSHA256")) }
        mac.update("server-image\n$nonce\n$sequence\n$body\n".toByteArray(Charsets.UTF_8))
        val expected = mac.doFinal(image).joinToString("") { "%02x".format(it) }
        require(MessageDigest.isEqual(expected.toByteArray(), header.text("mac").toByteArray())) { "Invalid image signature" }
        val metadata = Json.parseToJsonElement(body).jsonObject
        require(metadata.text("mime_type") == "image/jpeg") { "Unexpected preview format" }
        received = sequence
        return metadata to image
    }
    companion object {
        fun hmac(key: String, text: String): String {
            val mac = Mac.getInstance("HmacSHA256")
            mac.init(SecretKeySpec(key.toByteArray(Charsets.UTF_8), "HmacSHA256"))
            return mac.doFinal(text.toByteArray(Charsets.UTF_8)).joinToString("") { "%02x".format(it) }
        }
    }
}
