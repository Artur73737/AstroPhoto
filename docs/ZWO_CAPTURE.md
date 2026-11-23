# ZWO capture controls (Windows 0.10.5)

The Camera panel separates single-capture options, capture format/area, native
camera controls, image controls, auto-exposure options and temperature. Names,
ranges, Auto support, pixel formats and binning factors come from the connected
ASI camera. Unsupported controls are not substituted with Canon settings.

## Capture format and area

- **Pixel format:** only the RAW8, RGB24, RAW16 and MONO8 modes advertised by the
  connected device are offered. RGB24 is the SDK's BGR byte order internally.
- **Binning:** only SDK-supported factors are selectable. Hardware binning is a
  separate SDK control; it defaults to Off, not to the SDK's retained value.
- **Area:** full sensor and proportional centered presets, or a custom ROI.
  Select ROI lets you drag over Live View. Apply capture area/stream validates
  the rectangle, stops video, changes the SDK geometry and restarts video.
  Width must be a multiple of eight, height a multiple of two. Coordinates
  are in the binned image. Invalid settings restore the previous geometry/modes.
- **Debayer preview:** changes only the displayed JPEG, never the stored native
  sensor samples. Raw preview brightness uses a fixed linear scale so exposure
  and Gain changes remain visible.
- **Preview rate:** an application delivery cap, with Maximum meaning no
  artificial cap. It does not promise that the camera, exposure or USB link can
  attain a listed rate. Apply commits this setting along with the area.

High Speed defaults to On when supported. It is the vendor SDK mode and may
change sensor readout precision; RAW16 storage does not imply sixteen effective
ADC bits. USB bandwidth is another independent camera control. The Canon Turbo
Live Stream option is hidden and ignored for ASI; it never writes USB bandwidth.
Choose High Speed and USB settings appropriate for the camera and connection.

## Continuous capture

When an ASI is connected, **ZWO Capture** replaces the Canon Sequences panel.
The Canon Single shot section is hidden. Set exposure, Gain, Auto, pixel format,
ROI and binning in Camera, then choose Start capture or a Quick Capture preset.
The preview and recorder consume the same continuous SDK stream; there is no
stop/expose/download/restart cycle between recorded frames and no Canon shutter
delay. Canon plans remain saved and reappear when a Canon is connected.

Quick Capture starts immediately with 100, 200, 500, 1000, 2000, 3000 or 5000
frames, or a duration of 10, 20, 30, 60, 90, 120 or 180 seconds. Custom counts,
custom duration and **Until stopped** are available. Frame and time limits use
received native frames and elapsed host time, not UI redraw counts. A duration
can end before a long exposure produces its next frame.

The output selector sits beside Light/Dark/Flat/Bias in ZWO Capture. These are
file labels: ASI cameras do not close a mechanical shutter for Dark/Bias. Cover
the optical path and select suitable exposure settings yourself.

Output file is next to Light/Dark/Flat/Bias. FITS stores native RAW data or
three-channel RGB data; RAW8 values expand exactly to unsigned 16-bit FITS values.
PNG and TIFF preserve native 8/16-bit samples: RAW Bayer stays a mosaic, with a
small JSON sidecar recording its CFA, device name and dimensions. The app uses
the sidecar to debayer the captured-image preview. Keep it beside its image.

SER now stores the complete acquisition in **one multi-frame file**, preserving
native samples, dimensions, bit depth and color identifier. Its frame count is
checkpointed while writing and the UTC timestamp trailer is appended at the end.
Timestamps represent host UTC at successful frame retrieval, not hardware
exposure-start timestamps; the optional local-time header remains unspecified.
ADV and AVI are not offered because their writers are not implemented.

FITS/PNG/TIFF output creates numbered native images instead of a movie. Each
capture uses `{output}/{local-date}/{session}/captures/{type}_{unique-id}/`.
`capture.json` records limits, camera/ROI/control snapshot, written counts,
SDK/queue losses and errors. `frames.csv` records per-frame host timestamps and
SDK exposure/Gain readback, including Auto flags. Values reported while Auto is
active are not precision exposure measurements. Such FITS are displayable but
excluded from fixed-exposure RAW16 calibration. RAW8/MONO8 data likewise are not
substitutes for native RAW16 calibration files.

The recorder uses a separate disk thread with a configurable bounded buffer.
If storage cannot keep up or the free-space reserve is reached, it stops with
an explicit error instead of silently skipping queued frames. SDK losses are
reported separately and may occur at rates above host/USB capabilities. A
completed frame-count capture means that many frames were saved, not that the
SDK lost no earlier frames.

Stop ends collection, drains accepted frames and finalizes files while Live
View keeps running. Settings and geometry stay locked until finalization.
Camera loss and shutdown also finish accepted disk writes. A writer/finalization
failure leaves the SER `.partial` file, last committed header count and error
manifest. An early stop caused by producer loss/backpressure can still finalize
the successfully written prefix into a readable SER with an error manifest.
Automatic repair of interrupted recordings is not provided. The stream's
delivery cap controls preview encoding only during recording; storage still
receives native frames. With Maximum selected, preview JPEG generation is capped
at 60 FPS during recording while native capture remains uncapped. Stellar-AF
field analysis is suspended during recording; display peaking remains available.

Canon-only single-shot/sequence actions are rejected by the ASI core path.
Live stacking and True North retain their scientific RAW16 FITS path regardless
of the selected preview/output mode and cannot overlap continuous recording.
The low-level single-exposure API remains for these scientific workflows.

The implementation follows the vendor's `ASIStartVideoCapture` /
`ASIGetVideoData` contract in the bundled ASI SDK header. Related upstream
references: [ZWO SDK](https://www.zwoastro.com/software/product-sdk/),
[SharpCap capture model](https://docs.sharpcap.co.uk/2.9/9_CapturingVideosandImages.htm),
[SER specification](https://grischa-hahn.hier-im-netz.de/astro/ser/SER%20Doc%20V3b.pdf).
SharpCap is used as workflow reference; no SharpCap code is copied.

## Interaction and responsiveness

Wheel zooms around the pointer, middle-drag pans and double-click fits the image.
Live View and current capture keep separate display transforms. Zoom is a GPU
crop and does not change camera ROI, native zoom or saved pixels. Focus clicks
map back through this transform to their original image coordinates.

Native sliders update during a drag. Submissions are throttled to 20 Hz, with
the final value sent on release. Pending values for the same control replace
older queued values; acquisition commands remain ordering barriers. The core
owns all SDK calls and publishes actual readback. Manual Apply remains available.
An additional bounded image-processing worker handles decoding, histogram and
peaking; the UI uploads textures without doing those CPU operations itself.

## Validation

The opt-in ASI662MC suite uses the real SDK/device, not a simulated backend.
It covers all four advertised pixel modes, FITS/PNG/TIFF/SER captures, SDK bins
1 and 2, preview decoding, rejected ROI rollback, control locking and video
restart. See [hardware validation](HARDWARE_VALIDATION.md) for recorded runs.
Tests against other ASI models and nighttime field performance remain outstanding.
