#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <astro/RemoteAuth.hpp>
#include <stdexcept>
#include <vector>

namespace astro::remote {
namespace {

constexpr std::size_t kTagSize = 16;
constexpr std::size_t kMaxClientPayload = 1024 * 1024;

std::string hex(const unsigned char* bytes, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::runtime_error("Invalid hexadecimal digit");
}

std::string unhex(const std::string& text) {
    if (text.size() % 2) throw std::runtime_error("Invalid hexadecimal length");
    std::string result(text.size() / 2, '\0');
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(hexValue(text[2 * i]) << 4 | hexValue(text[2 * i + 1]));
    return result;
}

void check(NTSTATUS status) {
    if (status < 0) throw std::runtime_error("Windows cryptography operation failed");
}

struct AlgorithmHandle {
    BCRYPT_ALG_HANDLE handle{};
    ~AlgorithmHandle() {
        if (handle) BCryptCloseAlgorithmProvider(handle, 0);
    }
};

struct KeyHandle {
    BCRYPT_KEY_HANDLE handle{};
    ~KeyHandle() {
        if (handle) BCryptDestroyKey(handle);
    }
};

std::array<unsigned char, 12> gcmIv(Channel channel, std::uint64_t sequence) {
    std::array<unsigned char, 12> iv{};
    const auto label = static_cast<std::uint32_t>(channel);
    for (int i = 0; i < 4; ++i)
        iv[i] = static_cast<unsigned char>(label >> (24 - 8 * i));
    for (int i = 0; i < 8; ++i)
        iv[4 + i] = static_cast<unsigned char>(sequence >> (56 - 8 * i));
    return iv;
}

// Runs one AES-256-GCM operation. `tag` is written on encryption and verified on decryption.
std::string aesGcm(bool encrypt, const std::string& key, Channel channel, std::uint64_t sequence,
                   const std::string& input, unsigned char* tag) {
    if (key.size() != 32) throw std::runtime_error("Invalid session key");
    AlgorithmHandle algorithm;
    check(BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_AES_ALGORITHM, nullptr, 0));
    check(BCryptSetProperty(algorithm.handle, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                            sizeof(BCRYPT_CHAIN_MODE_GCM), 0));
    KeyHandle keyHandle;
    check(BCryptGenerateSymmetricKey(algorithm.handle, &keyHandle.handle, nullptr, 0, (PUCHAR)key.data(),
                                     static_cast<ULONG>(key.size()), 0));
    auto iv = gcmIv(channel, sequence);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = iv.data();
    info.cbNonce = static_cast<ULONG>(iv.size());
    info.pbAuthData = iv.data();  // channel + sequence are authenticated as additional data
    info.cbAuthData = static_cast<ULONG>(iv.size());
    info.pbTag = tag;
    info.cbTag = kTagSize;
    std::string output(input.size(), '\0');
    ULONG written{};
    auto* in = (PUCHAR)input.data();
    auto* out = (PUCHAR)output.data();
    const auto size = static_cast<ULONG>(input.size());
    const NTSTATUS status = encrypt
                                ? BCryptEncrypt(keyHandle.handle, in, size, &info, nullptr, 0, out, size, &written, 0)
                                : BCryptDecrypt(keyHandle.handle, in, size, &info, nullptr, 0, out, size, &written, 0);
    if (status < 0) throw std::runtime_error(encrypt ? "Encryption failed" : "Decryption failed");
    output.resize(written);
    return output;
}

std::string mac(const std::string& key, const char* label, const std::string& nonce, std::uint64_t sequence,
                const std::string& payload) {
    return hmac(key, std::string(label) + "\n" + nonce + "\n" + std::to_string(sequence) + "\n" + payload);
}

}  // namespace

std::string randomSecret() {
    unsigned char bytes[32];
    check(BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    return hex(bytes, sizeof(bytes));
}

std::string hmac(const std::string& key, const std::string& text) {
    AlgorithmHandle algorithm;
    check(
        BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG));
    BCRYPT_HASH_HANDLE hash{};
    check(BCryptCreateHash(algorithm.handle, &hash, nullptr, 0, (PUCHAR)key.data(), static_cast<ULONG>(key.size()), 0));
    struct Hash {
        BCRYPT_HASH_HANDLE h;
        ~Hash() { BCryptDestroyHash(h); }
    } hashGuard{hash};
    check(BCryptHashData(hash, (PUCHAR)text.data(), static_cast<ULONG>(text.size()), 0));
    unsigned char digest[32];
    check(BCryptFinishHash(hash, digest, sizeof(digest), 0));
    return hex(digest, sizeof(digest));
}

bool constantEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char difference{};
    for (std::size_t i = 0; i < a.size(); ++i)
        difference |= a[i] ^ b[i];
    return difference == 0;
}

std::string base64Encode(const std::string& bytes) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const auto n = std::uint32_t(std::uint8_t(bytes[i])) << 16 | std::uint32_t(std::uint8_t(bytes[i + 1])) << 8 |
                       std::uint8_t(bytes[i + 2]);
        result += table[n >> 18];
        result += table[n >> 12 & 63];
        result += table[n >> 6 & 63];
        result += table[n & 63];
    }
    if (const auto rest = bytes.size() - i) {
        auto n = std::uint32_t(std::uint8_t(bytes[i])) << 16;
        if (rest == 2) n |= std::uint32_t(std::uint8_t(bytes[i + 1])) << 8;
        result += table[n >> 18];
        result += table[n >> 12 & 63];
        result += rest == 2 ? table[n >> 6 & 63] : '=';
        result += '=';
    }
    return result;
}

std::string base64Decode(const std::string& text) {
    if (text.size() % 4) throw std::runtime_error("Invalid base64 length");
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        throw std::runtime_error("Invalid base64 character");
    };
    std::string result;
    result.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const bool last = i + 4 == text.size();
        const int pad = last ? (text[i + 3] == '=') + (text[i + 2] == '=') : 0;
        if (pad == 1 && text[i + 2] == '=') throw std::runtime_error("Invalid base64 padding");
        std::uint32_t n = value(text[i]) << 18 | value(text[i + 1]) << 12;
        if (pad < 2) n |= value(text[i + 2]) << 6;
        if (pad < 1) n |= value(text[i + 3]);
        result += static_cast<char>(n >> 16);
        if (pad < 2) result += static_cast<char>(n >> 8 & 255);
        if (pad < 1) result += static_cast<char>(n & 255);
    }
    return result;
}

std::string sessionEncryptionKey(const std::string& pairingKey, const std::string& nonce) {
    return unhex(hmac(pairingKey, "encrypt\n" + nonce));
}

std::string seal(const std::string& sessionKey, Channel channel, std::uint64_t sequence, const std::string& plaintext) {
    unsigned char tag[kTagSize];
    auto result = aesGcm(true, sessionKey, channel, sequence, plaintext, tag);
    result.append(reinterpret_cast<const char*>(tag), kTagSize);
    return result;
}

std::string open(const std::string& sessionKey, Channel channel, std::uint64_t sequence,
                 const std::string& ciphertext) {
    if (ciphertext.size() < kTagSize) throw std::runtime_error("Truncated ciphertext");
    unsigned char tag[kTagSize];
    std::copy(ciphertext.end() - kTagSize, ciphertext.end(), tag);
    return aesGcm(false, sessionKey, channel, sequence, ciphertext.substr(0, ciphertext.size() - kTagSize), tag);
}

Session::Session(std::string key) : key_(std::move(key)), nonce_(randomSecret()) {
    encryptionKey_ = sessionEncryptionKey(key_, nonce_);
}

nlohmann::json Session::challenge(const std::string& host) const {
    return {{"type", "challenge"}, {"version", kProtocolVersion}, {"nonce", nonce_}, {"server_name", host}};
}

void Session::authenticate(const nlohmann::json& request) {
    if (authenticated_ || request.value("type", std::string{}) != "authenticate")
        throw std::runtime_error("Authentication required");
    const auto name = request.at("device_name").get<std::string>();
    const bool invalidName = name.empty() || name.size() > 96 ||
                             std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; });
    if (invalidName) throw std::runtime_error("Invalid device name");
    if (!constantEqual(request.at("proof").get<std::string>(), hmac(key_, "auth\n" + nonce_ + "\n" + name)))
        throw std::runtime_error("Authentication failed");
    device_ = name;
    authenticated_ = true;
}

nlohmann::json Session::receive(const nlohmann::json& wire) {
    if (!authenticated_) throw std::runtime_error("Authentication required");
    const auto sequence = wire.at("seq").get<std::uint64_t>();
    const auto payload = wire.at("payload").get<std::string>();
    const bool valid = sequence == received_ + 1 && payload.size() <= kMaxClientPayload * 4 / 3 + 64 &&
                       constantEqual(wire.at("mac").get<std::string>(), mac(key_, "client", nonce_, sequence, payload));
    if (!valid) throw std::runtime_error("Invalid signature or replayed request");
    const auto plain = open(encryptionKey_, Channel::Client, sequence, base64Decode(payload));
    if (plain.size() > kMaxClientPayload) throw std::runtime_error("Request too large");
    auto result = nlohmann::json::parse(plain);
    ++received_;
    return result;
}

nlohmann::json Session::send(const nlohmann::json& value) {
    if (!authenticated_) throw std::runtime_error("Authentication required");
    const auto sequence = ++sent_;
    const auto payload = base64Encode(seal(encryptionKey_, Channel::Server, sequence, value.dump()));
    return {{"seq", sequence}, {"payload", payload}, {"mac", mac(key_, "server", nonce_, sequence, payload)}};
}

std::string Session::sendImage(const nlohmann::json& metadata, const std::string& jpeg) {
    if (!authenticated_) throw std::runtime_error("Authentication required");
    if (jpeg.size() > 4 * 1024 * 1024) throw std::runtime_error("Preview exceeds network budget");
    const auto sequence = ++sent_;
    const auto payload = base64Encode(seal(encryptionKey_, Channel::ServerImageHeader, sequence, metadata.dump()));
    const auto pixels = seal(encryptionKey_, Channel::ServerImagePixels, sequence, jpeg);
    const auto header = nlohmann::json{{"seq", sequence},
                                       {"payload", payload},
                                       {"mac", mac(key_, "server-image", nonce_, sequence, payload + "\n" + pixels)}}
                            .dump();
    const auto size = static_cast<std::uint32_t>(header.size());
    std::string packet;
    for (int shift : {24, 16, 8, 0})
        packet.push_back(static_cast<char>(size >> shift));
    packet += header;
    packet += pixels;
    return packet;
}

}  // namespace astro::remote
