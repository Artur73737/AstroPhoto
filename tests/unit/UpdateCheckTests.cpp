#include <astro/services/updates/UpdateCheck.hpp>
#include <gtest/gtest.h>

namespace {
using astro::update::Platform;

const char* releasesJson() {
    return R"([
      {
        "tag_name": "v0.9.1",
        "name": "AstroPhoto 0.9.1 / Remote 0.4.1",
        "html_url": "https://github.com/Artur73737/AstroPhoto/releases/tag/v0.9.1",
        "body": "Automatic update notifications.",
        "published_at": "2026-11-19T10:00:00Z",
        "draft": false,
        "prerelease": false,
        "assets": [
          {"name": "AstroPhoto-0.9.1-x64.msi", "browser_download_url": "https://github.com/Artur73737/AstroPhoto/releases/download/v0.9.1/AstroPhoto-0.9.1-x64.msi"},
          {"name": "AstroPhotoRemote-0.4.1.apk", "browser_download_url": "https://github.com/Artur73737/AstroPhoto/releases/download/v0.9.1/AstroPhotoRemote-0.4.1.apk"}
        ]
      },
      {
        "tag_name": "v0.9.0",
        "name": "Older",
        "html_url": "https://github.com/Artur73737/AstroPhoto/releases/tag/v0.9.0",
        "body": "Older release",
        "published_at": "2026-11-15T10:00:00Z",
        "draft": false,
        "prerelease": false,
        "assets": [
          {"name": "AstroPhoto-0.9.0-x64.msi", "browser_download_url": "https://example.invalid/old.msi"}
        ]
      },
      {
        "tag_name": "v1.0.0-rc1",
        "name": "Preview",
        "html_url": "https://github.com/Artur73737/AstroPhoto/releases/tag/v1.0.0-rc1",
        "body": "Pre-release",
        "published_at": "2026-11-20T10:00:00Z",
        "draft": false,
        "prerelease": true,
        "assets": [
          {"name": "AstroPhoto-1.0.0-x64.msi", "browser_download_url": "https://example.invalid/preview.msi"}
        ]
      }
    ])";
}
}  // namespace

TEST(UpdateCheck, ComparesVersionsNumerically) {
    EXPECT_TRUE(astro::update::isNewerVersion("0.10.0", "0.9.9"));
    EXPECT_TRUE(astro::update::isNewerVersion("1.0.0", "0.99.99"));
    EXPECT_TRUE(astro::update::isNewerVersion("v0.9.1", "0.9.0"));
    EXPECT_TRUE(astro::update::isNewerVersion("1.0.100000000000000000000", "1.0.99999999999999999999"));
    EXPECT_TRUE(astro::update::isNewerVersion("1.0.02", "1.0.1"));
    EXPECT_FALSE(astro::update::isNewerVersion("1.0.0000001", "1.0.1"));
    EXPECT_FALSE(astro::update::isNewerVersion("0.9.0", "0.9.0"));
    EXPECT_FALSE(astro::update::isNewerVersion("0.9.0", "0.9.1"));
    EXPECT_FALSE(astro::update::isNewerVersion("0.9", "0.8.9"));
    EXPECT_FALSE(astro::update::isNewerVersion("0.9.0-beta", "0.8.0"));
}

TEST(UpdateCheck, ExtractsOnlyPlatformInstallerAssets) {
    using astro::update::installerVersionFromAsset;
    EXPECT_EQ(installerVersionFromAsset("AstroPhoto-0.9.1-x64.msi", Platform::Windows), "0.9.1");
    EXPECT_EQ(installerVersionFromAsset("AstroPhotoRemote-0.4.1.apk", Platform::Android), "0.4.1");
    EXPECT_FALSE(installerVersionFromAsset("AstroPhotoRemote-0.4.1.apk", Platform::Windows));
    EXPECT_FALSE(installerVersionFromAsset("AstroPhoto-0.9.1-x64.msi", Platform::Android));
    EXPECT_FALSE(installerVersionFromAsset("AstroPhoto-0.9.1-x64.msi.sha256", Platform::Windows));
    EXPECT_FALSE(installerVersionFromAsset("AstroPhoto-latest-x64.msi", Platform::Windows));
}

TEST(UpdateCheck, FindsNewerWindowsInstallerAndReleaseMetadata) {
    const auto result = astro::update::parseReleaseList(releasesJson(), "0.9.0", Platform::Windows);
    ASSERT_TRUE(result.succeeded);
    ASSERT_TRUE(result.update.has_value());
    EXPECT_EQ(result.update->version, "0.9.1");
    EXPECT_EQ(result.update->currentVersion, "0.9.0");
    EXPECT_EQ(result.update->releaseUrl, "https://github.com/Artur73737/AstroPhoto/releases/tag/v0.9.1");
    EXPECT_EQ(result.update->downloadUrl,
              "https://github.com/Artur73737/AstroPhoto/releases/download/v0.9.1/AstroPhoto-0.9.1-x64.msi");
    EXPECT_EQ(result.update->notes, "Automatic update notifications.");
}

TEST(UpdateCheck, IgnoresPrereleasesAndOtherPlatforms) {
    const auto android = astro::update::parseReleaseList(releasesJson(), "0.4.0", Platform::Android);
    ASSERT_TRUE(android.succeeded);
    ASSERT_TRUE(android.update.has_value());
    EXPECT_EQ(android.update->version, "0.4.1");

    const auto windows = astro::update::parseReleaseList(
        R"([{"html_url":"https://github.com/Artur73737/AstroPhoto/releases/tag/v0.4.1",
             "draft":false,"prerelease":false,
             "assets":[{"name":"AstroPhotoRemote-0.4.1.apk"}]}])",
        "0.9.0", Platform::Windows);
    ASSERT_TRUE(windows.succeeded);
    EXPECT_FALSE(windows.update.has_value());
}

TEST(UpdateCheck, ReportsUpToDateWithoutUpdate) {
    const auto result = astro::update::parseReleaseList(releasesJson(), "0.9.1", Platform::Windows);
    ASSERT_TRUE(result.succeeded);
    EXPECT_FALSE(result.update.has_value());
}

TEST(UpdateCheck, RejectsMalformedOrUntrustedResponses) {
    EXPECT_FALSE(astro::update::parseReleaseList("{}", "0.9.0", Platform::Windows).succeeded);
    EXPECT_FALSE(astro::update::parseReleaseList("not json", "0.9.0", Platform::Windows).succeeded);
    const auto untrusted = astro::update::parseReleaseList(
        R"([{"html_url":"https://example.invalid/release","draft":false,"prerelease":false,
             "assets":[{"name":"AstroPhoto-9.9.9-x64.msi"}]}])",
        "0.9.0", Platform::Windows);
    ASSERT_TRUE(untrusted.succeeded);
    EXPECT_FALSE(untrusted.update.has_value());
}
