# AstroPhoto Remote for Android

A camera-style remote for the Windows AstroPhoto host. Kotlin, Jetpack Compose
and Material 3; English interface; black viewfinder, cyan accents, shutter,
gallery thumbnail, camera zoom pills and rounded General/Pro control sheets.

## Modules

| Module | Responsibility |
| --- | --- |
| `app` | Activity, lifecycle and retained controller model |
| `core:protocol` | QR payload, JSON helpers, HMAC sessions and binary framing; JVM tests |
| `core:model` | Shared immutable application state |
| `core:network` | OkHttp WebSocket, correlated requests, state synchronization and bounded in-memory preview pipeline |
| `core:designsystem` | Material 3 theme and shared presentation components |
| `feature:connection` | Device name, local QR scanner and pairing UI |
| `feature:liveview` | Image viewport, zoom, histogram and point-focus overlays |
| `feature:controller` | Camera screen, shutter, sequence and lens/AF controls |
| `feature:settings` | Camera-reported choices and schema-driven full settings/control sheets |
| `feature:gallery` | Remote PC file listing and transient previews |

Composable functions do not implement authentication or socket framing. Hardware
commands are never retried automatically. All supported property choices come
from the connected PC/camera, rather than a hardcoded 1100D table on Android.

## Build

Use JDK 17 and an Android SDK with platform/build tools 35. Set `ANDROID_HOME` or
create an untracked `local.properties` containing your own `sdk.dir`. The Gradle
wrapper pins Gradle 8.14.3. AGP 8.7.3, Kotlin 2.0.21 and Compose BOM 2024.12.01 are
pinned rather than floating. Minimum Android is 8.0 (API 26), target API 35.

```powershell
./gradlew.bat :app:assembleDebug :core:protocol:test :app:lintDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

The debug APK is developer-signed and installable by ADB. A distribution release
requires the publisher's release keystore; no private key is committed here.
The source has been physically tested on Pixel 8 Pro running Android 17.

## Pair and operate

Start **Settings → Android / WebSocket → Start WS server** on the PC, then
**Connection → Scan PC pairing QR** on Android. Select the PC's reachable local
network interface. A QR is single-use and expires in two minutes. The name is
read from Android and editable; no particular phone or computer name is assumed.

Use Photo for one exposure, Sequence for a configured plan, and Focus for the
three lens drive sizes, point measurements, normal/star peaking and stellar autofocus. Portrait and landscape have dedicated layouts.
The lens buttons are disabled unless the connected lens reports AF readiness.
General settings contain capture plans and saving destinations; Pro contains all
AF/presentation/worker options. All controls exposes the validated host catalog.

A foreground service preserves the pairing while the app is in the background. Its notification offers Disconnect; dismissing the application task or killing the app ends the session. Returning to an existing task needs no new QR.

Photo files and configuration persistence belong to the PC. Android holds JPEG
previews and pairing credentials in RAM. QR camera frames are not saved.
See [protocol, security and deployment details](../docs/ANDROID_REMOTE.md).

## Tests

JVM tests cover HMAC reference vectors, QR validation, tampering, replay,
direction separation and binary frame signatures. Windows tests exercise actual
WebSocket sockets, server lifecycle, JPEG encoding/cropping and gallery bounds.

The Android instrumentation hardware test is **opt-in**: it changes camera
properties, takes three real photographs into a dedicated PC test session, then
restores settings and leaves Live View enabled. It does not run stellar AF.

```powershell
./gradlew.bat :app:assembleDebugAndroidTest
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
# Put a freshly captured PC pairing-dialog PNG into the debug app's cache as
# pairing.png, using adb run-as. It is decoded with ZXing and deleted by the test.
adb shell am instrument -w -e hardware true `
  -e class com.astrophoto.remote.RemoteHardwareTest `
  com.astrophoto.remote.test/androidx.test.runner.AndroidJUnitRunner
```

The test uses the same QR parser, repository and Compose screen as the installed
app. A successful command acknowledgement is followed by camera readback and/or
download completion. No Canon SDK behavior is simulated.

## Signed distribution APK

From the repository root, set `JAVA_HOME` (JDK 17) and optionally `ANDROID_HOME`,
then run `./scripts/package-android.ps1`. This builds the non-debug release,
runs release lint, verifies its APK signature and produces
`out/Android/AstroPhotoRemote-0.1.1.apk` with a SHA-256 sidecar.

The first run creates a project signing identity in the ignored
`android/.signing/` directory. **Back up that directory securely**: future updates
must use the same key. Neither the key nor its credentials belong in Git or a
GitHub release. CI can instead supply `ASTRO_ANDROID_KEYSTORE`,
`ASTRO_ANDROID_STORE_PASSWORD` and `ASTRO_ANDROID_KEY_PASSWORD` as secrets when
invoking Gradle directly. The key alias is `astrophoto`.

The release key differs from the development debug key. A phone with the old
debug APK must remove it once before installing the signed release. Camera
photos stay on the Windows PC and are not affected.
