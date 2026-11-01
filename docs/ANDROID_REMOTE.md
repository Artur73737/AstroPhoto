# Android remote controller

AstroPhoto Remote is an English-language Kotlin / Jetpack Compose / Material 3
controller for the running Windows application. The Canon remains attached to
the PC. Photos are written by the PC; the Android application only holds decoded
previews in memory. It requests no storage permission and does not save camera
images, pairing keys or connection addresses.

## Pairing

1. Start the Windows application. Open **Settings → Android / WebSocket**.
2. Choose the listening port and press **Start WS server**. The server is off by
   default. A pairing dialog displays a QR for an active local IPv4 interface.
3. On Android, open **Connection → Scan PC pairing QR**. Allow the phone camera
   permission for QR scanning. The device name defaults to the Android device
   name, with the OS model as a fallback; it is editable before pairing.
4. Scan the QR. It is valid for **120 seconds and one successful pairing**.
   The PC shows the authenticated device name and address in server settings/logs.
5. The PC pairing dialog closes automatically after authentication. Live View starts when a connected, idle Canon becomes available.

Both devices must have a route to the selected local address. Select the correct
interface when the PC has Wi-Fi, Ethernet, USB tethering or virtual adapters.
After changing networks, generate a new QR; reconnecting never replays commands.
**New QR** restarts the server and disconnects the existing controller. Closing
the Android app or disconnecting the phone does not stop a running PC sequence.

If Windows Firewall prompts, allow the application on the trusted network used
for the session. The application does not disable the firewall or create broad
inbound exceptions. Do not publish or forward the port to the internet.

The optional Windows command line `--live-view --remote-server=47654` connects
the Canon, starts Live View and opens pairing. The port is configurable; it is
not an assumption about a user's device or network. Remote settings store only
the port in `remote.json`, never the pairing key.

## Controls

**Turbo Live** is available above the phone preview and as **Turbo Live Stream**
beside Live View on Windows. It applies immediately and can be saved with PC
preferences. Normal mode retains its approximately 8-FPS phone pacing; Turbo
allows up to 60 preview requests per second, bounded by actual camera throughput
and network/encoding time. JPEG quality and maximum preview dimensions are
unchanged; no focus analysis, peaking or histogram is switched off. One preview
request remains in flight at a time, preventing a growing queue of stale frames.
The FPS counter counts unique received camera frames, not repeated images.

Both 5× and 10× buttons centre the camera zoom area. The 1100D's native 10× mode
still transfers a 5× JPEG through USB; the application uses its reported geometry
to extract the correct inner crop on PC and Android.

- Camera-reported ISO, shutter, aperture, exposure mode, white balance, color
  temperature, picture style and image quality, including writable/available flags.
- Single Light/Dark/Flat/Bias captures, Bulb duration and delays.
- Sequence rows with count, exposure, initial delay, interval and per-row camera
  settings; start, pause, resume and stop.
- PC-only / PC-and-camera saving, output directory and session naming.
- Live View, camera-module zoom ratios, AF-gated lens drive steps, point focus,
  stellar autofocus controls and parameters, separate Off / Normal / Stars peaking modes, histograms and overlays.
- PC gallery listing and previews, including embedded CR2 previews.
- **General / Pro / All controls** sheets expose the full desktop configuration,
  presentation schema, logs, layout, command acknowledgements and diagnostics.
  Worker timing/retry settings explicitly require a PC application restart.

Camera commands use the same validated Control API and core mailbox as the
desktop. A command ID acknowledges submission, followed by completion/failure;
long exposures and sequences are followed through camera state. The phone
updates property labels from the real camera snapshot instead of optimistic
local values. Configuration updates are validated by the PC before application.

## Activity lifecycle and capture

### Focus controls (Windows 0.6.1 / Android 0.1.2)

The Focus tab has separate sections for point selection, lens motor, peaking and
stellar autofocus. **Off / Measure / Auto** are mutually exclusive: Off disables
image-tap focusing; Measure selects a local sharpness measurement without moving
the lens; Auto moves the camera's AF frame and runs native contrast autofocus.
Auto waits for a fresh 1× Live View frame before enabling taps. An attached lens
in AF mode is required. The rectangle turns green only after the camera confirms
focus. A failed/expired operation never reports OK. Stop point AF cancels it and
keeps Live View running. Targets near the edge are clamped to the camera's movable
AF frame area. Low-contrast subjects can fail to focus; this is not stellar HFD AF.

**Small / Medium / Large** select Canon motor-command sizes, and **− Near / + Far**
send one command each. These are not degrees. Peaking has its own **Off / Normal /
Stars** selector. **Star AF parameters** opens a dedicated sheet with boolean
switches and numeric fields validated against the PC schema; the required
Large → Medium → Small sequence is shown as read-only policy.

Controls lock while a command acknowledgement, capture, sequence or autofocus is
in progress. Stop remains available. The C++ worker independently rejects
conflicting commands, including multiple commands received before the next UI
refresh. Acknowledgement of AF start is not an optical success result.

Portrait uses a viewfinder above the controls. Landscape keeps a large viewfinder
on the left and a scrollable control rail on the right, with mode tabs always
accessible. Both layouts use English application labels, independent of the PC
language (user-entered names and paths are preserved).

Applying camera/configuration changes retains the last preview frame while the
next one becomes available. Transient EVF/texture gaps do not clear the display
or insert a shifting error banner. The core disables EVF before the shutter,
waits for exposure and download completion, then restores previously active
Live View. It does not resume EVF after a capture error or USB disconnection.

The Android service uses the platform's
[connected-device foreground service type](https://developer.android.com/develop/background-work/services/fgs/service-types#connected-device).
It holds a partial wake lock while the connection is active, releases it on
shutdown, and does not restart itself after task removal or a process kill.
The normal CHANGE_NETWORK_STATE permission satisfies the platform service
prerequisite; the application does not change network configuration. OS force
stop, loss of network routing, or a PC restart still ends the session.

## Transport and security

The C++ server uses IXWebSocket on a separate lifecycle thread and bounded
per-connection network workers. It never calls EDSDK. Hardware remains owned by
the existing core worker; UI requests cross the existing bounded ControlQueue.
Stopping/restarting the server does not block the ImGui render loop waiting for
network workers. Shutdown closes pending control requests before joining workers.

Pairing uses a randomly generated 256-bit secret and a fresh 256-bit challenge.
The client sends HMAC-SHA256 of `auth\n<nonce>\n<device-name>`. The secret is not
sent over WebSocket. Subsequent text and binary responses share monotonically
increasing sequence numbers. Direction-specific HMAC signatures prevent replay,
payload tampering and response reflection across sessions. Authentication must
complete within five seconds of the socket opening.

This version uses **authenticated `ws://`, not TLS**. Signing authenticates and
protects integrity; it does **not** hide photos or camera settings from network
observers. Use a trusted private network. A future WSS transport can retain the
same control/schema boundary without changing camera code.

Bounds: four socket slots, 120 inbound messages/second per connection, 1 MiB
maximum assembled inbound messages (enforced before allocation in the pinned
IX transport), 500 server log entries, 4 MiB maximum JPEG preview and bounded
slow-client output buffering. Browser origins are rejected. Authentication
errors never log the secret or request body.

## Images and state synchronization

With Turbo off, the phone requests approximately 8 preview frames/second and
the server caps image responses at 10 per second. Turbo allows up to 60 preview
requests per second. Both modes keep only one image request in flight; actual
delivery depends on camera throughput, processing and round-trip time.
Backgrounding Android pauses image streaming and reduces state polling. A connected-device foreground service retains the authenticated socket with a persistent notification. The PC sequence continues. Opening the app again reuses the same in-memory session; removing its task, choosing Disconnect or force-stopping the app ends the connection. Credentials are never persisted or automatically replayed after a process kill.
Displayed FPS counts new camera revisions, not duplicate responses.

Image replies are **binary JPEG**, limited to 1024 pixels on the longest side,
quality 0.78. The native camera-module preview crop is applied before resize, so
the 1100D 10× view does not duplicate its 5× view. JPEG encoding and gallery
decoding run on network workers, outside the UI/core threads. Processed previews
include the PC's peaking texture. Original photos remain untouched on disk.

Binary wire layout: four-byte big-endian JSON header length, UTF-8 header, JPEG
bytes. The header includes `seq`, `payload` (metadata JSON string with request ID,
MIME type and frame revision), and `mac`. Its signature covers
`server-image\n<nonce>\n<seq>\n<payload>\n<JPEG bytes>`.

Text requests are signed envelopes with an inner `{id, tool, arguments}` object.
The server exposes the existing control-tool catalog and adds `tools`,
`preview_live`, `preview_capture`, `gallery_list` and `gallery_image`. Gallery IDs
refer only to files previously listed beneath the configured PC output folder;
arbitrary client file paths are rejected. Listings are bounded to 200 recent
supported files and 10,000 visited entries and indicate truncation.

## Build and validation

See [the Android project README](../android/README.md) for build/test commands.
The Windows source is `src/remote/`; bootstrap pins IXWebSocket 11.4.6 and Nayuki
QR Code generator 1.8.0. Their licenses ship in the MSI.

The physical validation uses a Pixel 8 Pro and Canon EOS 1100D, actual LAN
WebSocket traffic, camera property readback, real single/sequence CR2 captures,
binary live/capture previews and PC gallery reads. The test restores the original
camera/configuration and leaves Live View running. It deliberately excludes
stellar autofocus and never claims telescope/sky focus accuracy from this test.
