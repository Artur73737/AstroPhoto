#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace astro::remote {

std::string randomSecret();
std::string hmac(const std::string& key, const std::string& text);
bool constantEqual(const std::string&, const std::string&);
std::string base64Encode(const std::string& bytes);
std::string base64Decode(const std::string& text);

// Wire protocol version. Version 2 encrypts every payload with AES-256-GCM.
inline constexpr int kProtocolVersion = 2;

// Distinct per-direction labels make every (label, sequence) GCM IV unique
// within one connection key.
enum class Channel : std::uint32_t { Client = 1, Server = 2, ServerImageHeader = 3, ServerImagePixels = 4 };

// The per-connection encryption key is HMAC(pairing key, "encrypt\n" + nonce).
// The pairing key itself never leaves either device.
std::string sessionEncryptionKey(const std::string& pairingKey, const std::string& nonce);
// Returns ciphertext || 16-byte tag. The IV is the channel label followed by the
// big-endian sequence number; the same pair is also bound as additional data.
std::string seal(const std::string& sessionKey, Channel channel, std::uint64_t sequence, const std::string& plaintext);
// Throws std::runtime_error when the tag does not verify.
std::string open(const std::string& sessionKey, Channel channel, std::uint64_t sequence, const std::string& ciphertext);

// A new random nonce binds every packet to one connection. Sequence numbers
// reject replay; direction separation prevents reflected responses. Payloads
// are encrypted (AES-256-GCM) and additionally HMAC-signed over the ciphertext.
class Session {
public:
    explicit Session(std::string key);
    nlohmann::json challenge(const std::string& host) const;
    void authenticate(const nlohmann::json&);
    nlohmann::json receive(const nlohmann::json&);
    nlohmann::json send(const nlohmann::json&);
    std::string sendImage(const nlohmann::json& metadata, const std::string& jpeg);
    bool authenticated() const { return authenticated_; }
    const std::string& device() const { return device_; }

private:
    std::string key_, nonce_, encryptionKey_, device_;
    bool authenticated_{};
    std::uint64_t received_{}, sent_{};
};

}  // namespace astro::remote
