package com.astrophoto.network

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class UpdateCheckerTest {
    private val parser = ReleaseParser("example/repo")

    private val releases = """
        [
          {
            "tag_name": "v9.9.9",
            "name": "Current",
            "html_url": "https://github.com/example/repo/releases/tag/v9.9.9",
            "body": "Current release notes.",
            "published_at": "2026-11-19T10:00:00Z",
            "draft": false,
            "prerelease": false,
            "assets": [
              {"name": "AstroPhotoRemote-0.4.1.apk", "browser_download_url": "https://github.com/example/repo/releases/download/v9.9.9/AstroPhotoRemote-0.4.1.apk"},
              {"name": "AstroPhotoRemote-0.4.1.apk.sha256", "browser_download_url": "https://example.invalid/checksum"}
            ]
          },
          {
            "tag_name": "v9.9.8",
            "name": "Preview",
            "html_url": "https://github.com/example/repo/releases/tag/v9.9.8",
            "body": "Preview release.",
            "published_at": "2026-11-20T10:00:00Z",
            "draft": false,
            "prerelease": true,
            "assets": [
              {"name": "AstroPhotoRemote-9.0.0.apk", "browser_download_url": "https://example.invalid/preview.apk"}
            ]
          },
          {
            "tag_name": "v9.9.7",
            "name": "Windows only",
            "html_url": "https://github.com/example/repo/releases/tag/v9.9.7",
            "body": "No APK.",
            "published_at": "2026-11-18T10:00:00Z",
            "draft": false,
            "prerelease": false,
            "assets": [
              {"name": "AstroPhoto-0.9.9-x64.msi", "browser_download_url": "https://example.invalid/app.msi"}
            ]
          }
        ]
    """.trimIndent()

    @Test
    fun selectsNewestPublishedApkFromApiPayload() {
        val update = parser.parse(releases, "0.4.0")
        requireNotNull(update)
        assertEquals("0.4.1", update.version)
        assertEquals("0.4.0", update.currentVersion)
        assertEquals("Current", update.title)
        assertEquals("https://github.com/example/repo/releases/tag/v9.9.9", update.releaseUrl)
        assertEquals(
            "https://github.com/example/repo/releases/download/v9.9.9/AstroPhotoRemote-0.4.1.apk",
            update.downloadUrl,
        )
        assertEquals("Current release notes.", update.notes)
    }

    @Test
    fun ignoresPrereleasesChecksumsAndOtherPlatformAssets() {
        val update = parser.parse(releases, "0.4.0")
        requireNotNull(update)
        assertFalse(update.version == "9.0.0")
        assertFalse(update.version == "0.9.9")
        assertNull(parser.installerVersion("AstroPhotoRemote-0.4.1.apk.sha256"))
        assertNull(parser.installerVersion("AstroPhoto-0.9.9-x64.msi"))
    }

    @Test
    fun returnsNullWhenInstalledVersionIsCurrentOrNewer() {
        assertNull(parser.parse(releases, "0.4.1"))
        assertNull(parser.parse(releases, "0.5.0"))
    }

    @Test fun manualCheckShowsTheActualPublishedVersionEvenWhenInstalledIsNewer() {
        assertEquals("0.4.1", parser.latest(releases, "0.4.6")?.version)
        assertEquals("0.4.1", parser.latest(releases, "0.4.1")?.version)
        assertNull(parser.parse(releases, "0.4.6"))
    }

    @Test fun refusesApkLinksOutsideTheConfiguredRepository() {
        val altered = releases.replace("https://github.com/example/repo/releases/download/v9.9.9/AstroPhotoRemote-0.4.1.apk", "https://other.invalid/file.apk")
        assertNull(parser.latest(altered, "0.0.0"))
    }

    @Test
    fun comparesVersionNumbersNumerically() {
        assertTrue(parser.isNewerVersion("0.10.0", "0.9.9"))
        assertTrue(parser.isNewerVersion("1.0.0", "0.99.99"))
        assertFalse(parser.isNewerVersion("0.9.0", "0.9.0"))
        assertFalse(parser.isNewerVersion("0.9.0", "0.9.1"))
        assertFalse(parser.isNewerVersion("0.9", "0.8.9"))
        assertFalse(parser.isNewerVersion("0.9.0-beta", "0.8.0"))
    }

    @Test
    fun rejectsMalformedOrUntrustedPayloads() {
        assertNull(parser.parse("not json", "0.4.0"))
        assertNull(parser.parse("{}", "0.4.0"))
        val untrusted = """
            [{"html_url":"https://example.invalid/release","draft":false,"prerelease":false,
              "assets":[{"name":"AstroPhotoRemote-1.0.0.apk"}]}]
        """.trimIndent()
        assertNull(parser.parse(untrusted, "0.4.0"))
    }

    @Test
    fun ignoresAssetsWithoutAVersionNumber() {
        assertNull(parser.installerVersion("AstroPhotoRemote.apk"))
        assertEquals("0.4.1", parser.installerVersion("AstroPhotoRemote-0.4.1.apk"))
    }
}
