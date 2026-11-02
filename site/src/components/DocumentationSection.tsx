"use client";

import { useState } from "react";
import { repository } from "@/lib/paths";

const guides = [
  {
    title: "Connect the camera",
    label: "01 / Windows",
    paragraphs: [
      "Connect the Canon by USB, switch it on and close EOS Utility or other camera-control applications. Open AstroPhoto and connect from the Camera menu. The EOS 1100D is the physically tested model; other bodies require their own camera module and validation.",
      "Set exposure mode, ISO, shutter, aperture and image quality in the camera panel. The available values come from the connected camera. Choose an output directory and a session name before taking your first shot. Select PC only if you do not want a second copy on the camera card.",
    ],
    path: "README.md#using-the-application", link: "Camera setup and capture",
  },
  {
    title: "Pair your Android phone",
    label: "02 / Local network",
    paragraphs: [
      "Keep the Windows app running and put both devices on the same reachable local network. On Windows, open Settings → Android / WebSocket and start the server. Select the correct network interface if the PC has more than one. On the phone, open Connection → Scan PC pairing QR.",
      "The QR expires after 120 seconds and accepts one pairing. The PC dialog closes after authentication. The foreground service keeps the connection while you switch apps; removing the task or killing the app ends it. Photos stay on the PC. Use a trusted network: pairing authenticates the connection, but WebSocket traffic is not encrypted.",
    ],
    path: "docs/ANDROID_REMOTE.md", link: "Pairing, controls and connection lifecycle",
  },
  {
    title: "Focus and Live View",
    label: "03 / Image panel",
    paragraphs: [
      "Start Live View to frame the subject. Measure reports sharpness around a selected point without moving the lens. Auto point focus asks the camera to focus at that point and reports success only after camera confirmation. Motor controls require an attached lens set to AF; Near and Far use Small, Medium or Large commands, not degrees.",
      "Normal peaking highlights edges; Stars peaking selects compact sources. Stellar autofocus follows Large → Medium → Small and reports when motor resolution or the image signal limits precision. Live View is a short-exposure video feed: selecting a 30-second shutter does not turn it into a 30-second preview. Check an actual capture when faint stars are missing.",
    ],
    path: "docs/HARDWARE_VALIDATION.md", link: "Tested behaviour and focus limitations",
  },
  {
    title: "Build a RAW live stack",
    label: "04 / Acquisition",
    paragraphs: [
      "Choose RAW or RAW+JPEG, manual exposure and fixed ISO. In Image, select Live stacking, then choose a frame count or Until stopped. The stack uses your current camera settings. Each exposure is downloaded and processed before the next capture; the configurable delay defaults to 3 seconds and has a 2-second minimum.",
      "Enable Align stars for a star field. Failed registration rejects the frame instead of mixing an unaligned image into the stack. Disable it only when you intentionally want fixed-pixel integration, such as a stationary bench test. The counter distinguishes accepted frames from shutter releases. Pause finishes the current frame and parks safely; Resume continues. Stop saves the result. Rebuild an existing CR2 folder without new shutter commands, or use a homogeneous Light sequence plan.",
    ],
    path: "docs/LIVE_STACKING.md", link: "Stack pipeline, rejection and saved files",
  },
  {
    title: "Adjust the stack display",
    label: "05 / Image processing",
    paragraphs: [
      "Low, Mid and High, with optional Asinh, sit with the stack settings and change the display stretch, not the saved linear data. Matching Bias, Dark, Flat and Dark-flat folders calibrate Bayer data before demosaicing. Optional background correction, RGB drizzle and affine/quad registration are experimental pending Canon night-sky and calibration-set validation.",
      "Horizontal banding reduction is optional and off by default. Start conservatively: strength controls the correction, radius controls the neighbouring rows and maximum offset caps it. Real horizontal structures can also be affected. This is not dark, bias or flat calibration. The JSON report records corrections and rejected frames so you can review the run.",
    ],
    path: "docs/LIVE_STACKING.md#horizontal-banding", link: "Banding controls and processing limits",
  },
  {
    title: "Update and troubleshoot",
    label: "06 / Maintenance",
    paragraphs: [
      "Windows and Android have separate version numbers. Use the version shown on each download, not the shared GitHub release tag. Install the signed release APK over a previous release signed with the same key. A debug build uses a different key and must be removed first if Android refuses the update.",
      "If pairing fails, check the selected PC address, QR expiry and Windows Firewall access on your trusted network. If the camera disconnects, inspect the camera log and USB connection before restarting a capture. Recovery does not replay shutter commands or automatically resume an interrupted sequence.",
    ],
    path: "docs/USER_GUIDE.md#troubleshooting", link: "Troubleshooting and version checks",
  },
];

function DurationEstimate() {
  const [frames, setFrames] = useState("50");
  const [exposure, setExposure] = useState("30");
  const [delay, setDelay] = useState("3");
  const count = Number(frames), seconds = Number(exposure), interval = Number(delay);
  const valid = frames !== "" && exposure !== "" && delay !== "" && Number.isInteger(count) && count >= 1 && count <= 10000 && seconds > 0 && seconds <= 86400 && interval >= 2 && interval <= 3600;
  const duration = valid ? Math.ceil(count * seconds + (count - 1) * interval) : 0;
  const formatted = `${Math.floor(duration / 3600)}h ${Math.floor(duration % 3600 / 60)}m ${duration % 60}s`;
  return <aside className="session-tool" aria-labelledby="duration-title">
    <p className="hud-label">Session calculator</p>
    <h3 id="duration-title">Time at the camera</h3>
    <p>Estimate a fixed-count stack before you start.</p>
    <div className="session-inputs">
      <label>Frames<input type="number" min="1" max="10000" step="1" value={frames} onChange={event => setFrames(event.target.value)} /></label>
      <label>Exposure / s<input type="number" min="0.001" max="86400" step="any" value={exposure} onChange={event => setExposure(event.target.value)} /></label>
      <label>Delay / s<input type="number" min="2" max="3600" step="any" value={delay} onChange={event => setDelay(event.target.value)} /></label>
    </div>
    <output aria-live="polite">{valid ? formatted : "Check the values above"}</output>
    <p className="tool-note">Exposure time + gaps between shots. Downloads and RAW processing add time. This calculator does not change your camera settings.</p>
    <div className="storage-note"><p className="hud-label">Saved on the PC</p><p>Original CR2 frames<br />Linear FITS · optional TIFF / XISF<br />JSON report · image checkpoints</p><a className="text-link" href={`${repository}/blob/main/docs/LIVE_STACKING.md`}>File formats and folder structure ↗</a></div>
  </aside>;
}

export function DocumentationSection() {
  return <section id="documentation" className="section-shell">
    <div className="section-eyebrow"><span>05 / Field guide</span><span>Setup · capture · review</span></div>
    <div className="section-heading"><h2>From connection<br />to saved frames.</h2><p>Practical notes for Windows and Android. Open a topic here, or follow its reference for the full settings and implementation details.</p></div>
    <div className="documentation-layout"><div className="guide-list">{guides.map(guide => <details key={guide.label} className="guide-topic"><summary><span><small>{guide.label}</small>{guide.title}</span><span className="guide-indicator" aria-hidden="true" /></summary><div className="guide-copy">{guide.paragraphs.map(paragraph => <p key={paragraph}>{paragraph}</p>)}<a className="text-link" href={`${repository}/blob/main/${guide.path}`}>{guide.link} ↗</a></div></details>)}<div className="guide-references"><a href={`${repository}/blob/main/docs/USER_GUIDE.md`}>Full user guide ↗</a><a href={`${repository}/blob/main/docs/MCP.md`}>MCP reference ↗</a><a href={`${repository}/blob/main/README.md#build-and-run`}>Build from source ↗</a></div></div><DurationEstimate /></div>
  </section>;
}
