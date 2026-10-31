package com.astrophoto.protocol

import java.net.URI

data class PairingCode(val address: String, val key: String) {
    companion object {
        fun parse(text: String): PairingCode {
            require(text.length <= 1024) { "QR code is too large" }
            val fields = text.split('|')
            require(fields.size == 3 && fields[0] == "astrophoto-pair:1") { "This is not an AstroPhoto pairing QR" }
            val uri = URI(fields[1])
            require(uri.scheme == "ws" && uri.host != null && uri.path == "/astrophoto" && uri.port in 1024..65535 && uri.userInfo == null && uri.query == null && uri.fragment == null) { "Invalid local server address" }
            require(fields[2].matches(Regex("[a-f0-9]{64}"))) { "Invalid pairing key" }
            return PairingCode(fields[1], fields[2])
        }
    }
}
