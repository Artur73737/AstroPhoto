package com.astrophoto.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.IOException
import java.util.concurrent.TimeUnit

data class UpdateInfo(
    val version: String,
    val currentVersion: String,
    val title: String,
    val releaseUrl: String,
    val downloadUrl: String,
    val notes: String,
    val publishedAt: String,
)

data class UpdateCheckResult(val latest: UpdateInfo, val updateAvailable: Boolean)

class UpdateChecker internal constructor(
    private val repository: String,
    private val client: OkHttpClient,
) {
    constructor(repository: String) : this(repository, defaultClient())

    suspend fun check(currentVersion: String): UpdateInfo? = inspect(currentVersion).let { if (it.updateAvailable) it.latest else null }

    suspend fun inspect(currentVersion: String): UpdateCheckResult = withContext(Dispatchers.IO) {
        require(repository.matches(Regex("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"))) {
            "Invalid GitHub repository"
        }
        val request = Request.Builder()
            .url("https://api.github.com/repos/$repository/releases?per_page=30")
            .header("Accept", "application/vnd.github+json")
            .header("X-GitHub-Api-Version", "2022-11-28")
            .header("User-Agent", "AstroPhotoRemote/$currentVersion")
            .build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) throw IOException("GitHub returned HTTP ${response.code}")
            val body = response.body?.string() ?: throw IOException("GitHub returned an empty response")
            val parser = ReleaseParser(repository)
            val latest = parser.latest(body, currentVersion) ?: throw IOException("No valid published Android APK was found in the GitHub response")
            UpdateCheckResult(latest, parser.isNewerVersion(latest.version, currentVersion))
        }
    }

    private companion object {
        fun defaultClient(): OkHttpClient = OkHttpClient.Builder()
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(15, TimeUnit.SECONDS)
            .callTimeout(20, TimeUnit.SECONDS)
            .build()
    }
}

internal class ReleaseParser(private val repository: String) {
    private val releaseUrlPrefix = "https://github.com/$repository/releases/"
    private val versionPattern = Regex("""(\d+)\.(\d+)\.(\d+)""")

    fun parse(json: String, currentVersion: String): UpdateInfo? = latest(json, currentVersion)?.takeIf { isNewerVersion(it.version, currentVersion) }

    fun latest(json: String, currentVersion: String): UpdateInfo? = runCatching {
        val releases = Json.parseToJsonElement(json).jsonArray
        var best: UpdateInfo? = null
        for (release in releases) {
            val releaseObject = release.jsonObject
            if (releaseObject.boolean("draft") || releaseObject.boolean("prerelease")) continue
            val releaseUrl = releaseObject.text("html_url")
            if (!releaseUrl.startsWith(releaseUrlPrefix)) continue
            val assets = releaseObject["assets"] as? JsonArray ?: continue
            for (asset in assets) {
                val assetObject = asset.jsonObject
                val version = installerVersion(assetObject.text("name")) ?: continue
                val downloadUrl = assetObject.text("browser_download_url")
                if (!downloadUrl.startsWith("https://github.com/$repository/releases/download/")) continue
                val currentBest = best
                if (currentBest != null && !isNewerVersion(version, currentBest.version)) continue
                best = UpdateInfo(
                    version = version,
                    currentVersion = currentVersion,
                    title = releaseObject.text("name"),
                    releaseUrl = releaseUrl,
                    downloadUrl = downloadUrl,
                    notes = releaseObject.text("body"),
                    publishedAt = releaseObject.text("published_at"),
                )
            }
        }
        best
    }.getOrNull()

    fun installerVersion(name: String): String? {
        if (!name.endsWith(".apk", ignoreCase = true)) return null
        return versionPattern.find(name)?.value
    }

    fun isNewerVersion(candidate: String, current: String): Boolean {
        val left = versionPattern.matchEntire(candidate)?.groupValues ?: return false
        val right = versionPattern.matchEntire(current)?.groupValues ?: return false
        for (index in 1..3) {
            val candidatePart = left[index].toIntOrNull() ?: return false
            val currentPart = right[index].toIntOrNull() ?: return false
            if (candidatePart != currentPart) return candidatePart > currentPart
        }
        return false
    }

    private fun JsonObject.text(key: String): String =
        (this[key] as? kotlinx.serialization.json.JsonPrimitive)?.contentOrNull.orEmpty()

    private fun JsonObject.boolean(key: String): Boolean =
        (this[key] as? kotlinx.serialization.json.JsonPrimitive)?.booleanOrNull ?: false
}
