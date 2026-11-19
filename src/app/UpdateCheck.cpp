#define NOMINMAX
#include "UpdateCheck.hpp"
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <regex>

#ifndef ASTRO_UPDATE_REPOSITORY
#error ASTRO_UPDATE_REPOSITORY must name the GitHub owner/repository used for update checks
#endif

namespace astro::update {
namespace {
using Json = nlohmann::json;
constexpr const char* kRepository = ASTRO_UPDATE_REPOSITORY;
constexpr std::size_t kMaximumResponseBytes = 8 * 1024 * 1024;

std::string repositoryUrlPrefix() {
    return "https://github.com/" + std::string(kRepository) + "/releases/";
}

std::wstring releaseApiPath() {
    const std::string path = "/repos/" + std::string(kRepository) + "/releases?per_page=30";
    return {path.begin(), path.end()};
}

std::optional<std::string> normalizedVersion(std::string_view value) {
    if (value.starts_with('v') || value.starts_with('V')) value.remove_prefix(1);
    const auto first = value.find('.');
    if (first == std::string_view::npos) return std::nullopt;
    const auto second = value.find('.', first + 1);
    if (second == std::string_view::npos || value.find('.', second + 1) != std::string_view::npos) return std::nullopt;
    for (const char character : value)
        if (!std::isdigit(static_cast<unsigned char>(character)) && character != '.') return std::nullopt;
    if (first == 0 || second == first + 1 || second + 1 == value.size()) return std::nullopt;
    return std::string(value);
}

std::string text(const Json& object, const char* key) {
    const auto entry = object.find(key);
    return entry != object.end() && entry->is_string() ? entry->get<std::string>() : std::string{};
}

bool isPublishedRelease(const Json& release) {
    return release.is_object() && !release.value("draft", false) && !release.value("prerelease", false);
}

std::string describeWinHttpError(const char* operation) {
    return std::string(operation) + " failed (Windows error " + std::to_string(GetLastError()) + ")";
}

struct InternetHandle {
    HINTERNET value{};
    ~InternetHandle() {
        if (value) WinHttpCloseHandle(value);
    }
    explicit operator bool() const { return value != nullptr; }
};

struct FetchResult {
    bool succeeded{};
    std::string body;
    std::string error;
};

FetchResult fetchReleases() {
    InternetHandle session(WinHttpOpen(L"AstroPhoto-Updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return {false, {}, describeWinHttpError("WinHttpOpen")};
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000);
    InternetHandle connection(WinHttpConnect(session.value, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) return {false, {}, describeWinHttpError("WinHttpConnect")};
    const auto path = releaseApiPath();
    InternetHandle request(WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) return {false, {}, describeWinHttpError("WinHttpOpenRequest")};
    const wchar_t* headers =
        L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\nUser-Agent: "
        L"AstroPhoto-Updater\r\n";
    if (!WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        return {false, {}, describeWinHttpError("WinHttpSendRequest")};
    if (!WinHttpReceiveResponse(request.value, nullptr))
        return {false, {}, describeWinHttpError("WinHttpReceiveResponse")};
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
        return {false, {}, describeWinHttpError("WinHttpQueryHeaders")};
    if (status != 200) return {false, {}, "GitHub returned HTTP " + std::to_string(status)};
    std::string body;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available))
            return {false, {}, describeWinHttpError("WinHttpQueryDataAvailable")};
        if (available == 0) break;
        if (body.size() + available > kMaximumResponseBytes) return {false, {}, "GitHub response is too large"};
        const auto offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.value, body.data() + offset, available, &read))
            return {false, {}, describeWinHttpError("WinHttpReadData")};
        body.resize(offset + read);
    }
    return {true, std::move(body), {}};
}

std::string_view withoutLeadingZeroes(std::string_view value) {
    while (value.size() > 1 && value.front() == '0')
        value.remove_prefix(1);
    return value;
}

int compareNumericPart(std::string_view left, std::string_view right) {
    left = withoutLeadingZeroes(left);
    right = withoutLeadingZeroes(right);
    if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
    if (left == right) return 0;
    return left < right ? -1 : 1;
}
}  // namespace

bool isNewerVersion(std::string_view candidate, std::string_view current) {
    const auto left = normalizedVersion(candidate);
    const auto right = normalizedVersion(current);
    if (!left || !right) return false;
    std::size_t leftStart = 0, rightStart = 0;
    for (int part = 0; part < 3; ++part) {
        const auto leftEnd = left->find('.', leftStart);
        const auto rightEnd = right->find('.', rightStart);
        const auto comparison = compareNumericPart(std::string_view(*left).substr(leftStart, leftEnd - leftStart),
                                                   std::string_view(*right).substr(rightStart, rightEnd - rightStart));
        if (comparison != 0) return comparison > 0;
        leftStart = leftEnd == std::string::npos ? left->size() : leftEnd + 1;
        rightStart = rightEnd == std::string::npos ? right->size() : rightEnd + 1;
    }
    return false;
}

std::optional<std::string> installerVersionFromAsset(std::string_view assetName, Platform platform) {
    std::string name(assetName);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    const std::string suffix = platform == Platform::Windows ? ".msi" : ".apk";
    if (!name.ends_with(suffix)) return std::nullopt;
    const std::regex versionPattern(R"((\d+)\.(\d+)\.(\d+))");
    std::smatch match;
    if (!std::regex_search(name, match, versionPattern)) return std::nullopt;
    return normalizedVersion(match.str());
}

Result parseReleaseList(std::string_view json, std::string_view currentVersion, Platform platform) {
    try {
        const auto releases = Json::parse(json);
        if (!releases.is_array()) return {false, std::nullopt, "GitHub release response is not an array"};
        const auto releaseUrlPrefix = repositoryUrlPrefix();
        std::optional<UpdateInfo> best;
        for (const auto& release : releases) {
            if (!isPublishedRelease(release)) continue;
            const auto releaseUrl = text(release, "html_url");
            if (!releaseUrl.starts_with(releaseUrlPrefix)) continue;
            const auto assets = release.find("assets");
            if (assets == release.end() || !assets->is_array()) continue;
            for (const auto& asset : *assets) {
                if (!asset.is_object()) continue;
                const auto version = installerVersionFromAsset(text(asset, "name"), platform);
                if (!version || !isNewerVersion(*version, currentVersion)) continue;
                if (best && !isNewerVersion(*version, best->version)) continue;
                best = UpdateInfo{*version,
                                  std::string(currentVersion),
                                  text(release, "name"),
                                  releaseUrl,
                                  text(asset, "browser_download_url"),
                                  text(release, "body"),
                                  text(release, "published_at")};
            }
        }
        return {true, std::move(best), {}};
    } catch (const std::exception& error) {
        return {false, std::nullopt, std::string("Invalid GitHub response: ") + error.what()};
    }
}

Checker::Checker(std::string currentVersion, Platform platform)
    : currentVersion_(std::move(currentVersion)), platform_(platform) {}

Checker::~Checker() {
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_ = std::jthread{};
    }
}

void Checker::start() {
    if (running_.exchange(true)) return;
    {
        std::lock_guard lock(mutex_);
        result_.reset();
    }
    thread_ = std::jthread([this] { run(); });
}

std::optional<Result> Checker::poll() {
    std::lock_guard lock(mutex_);
    if (!result_) return std::nullopt;
    auto result = std::move(result_);
    result_.reset();
    return result;
}

void Checker::run() {
    const auto fetched = fetchReleases();
    Result result;
    if (!fetched.succeeded)
        result = {false, std::nullopt, fetched.error};
    else
        result = parseReleaseList(fetched.body, currentVersion_, platform_);
    {
        std::lock_guard lock(mutex_);
        result_ = std::move(result);
    }
    running_ = false;
}
}  // namespace astro::update
