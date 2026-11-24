import { publicAsset, repository } from "@/lib/paths";
import captures from "@/content/screenshots.json";

const screens = [
  { image: "android-046-workspace.png", title: "A clear viewfinder", text: "Swipe the mode strip below the shutter. General settings sit on the left; the selected mode's parameters on the right. Landscape uses a side rail." },
  { image: "android-046-focus.png", title: "Focus controls when needed", text: "Point measurement, supported lens commands and Normal/Stars peaking open in a settings sheet. Unsupported or busy-camera actions remain disabled." },
  { image: "android-046-general.png", title: "Separate session and camera settings", text: "Preview preferences and general settings stay separate from Canon Photo/Sequence and ZWO native-frame Capture parameters. Advanced configuration and host tools remain accessible." },
  { image: "android-046-updates.png", title: "Check the published APK", text: "Check for updates from General settings. The GitHub API supplies the Android version independently of Windows release tags, with no camera or PC connection required." },
] as const;

export function AndroidSection() {
  return <section id="android" className="section-shell">
    <div className="section-eyebrow"><span>Companion / Android</span><span>Kotlin · Jetpack Compose · Material 3</span></div>
    <div className="section-heading"><h2>Step inside.<br /><span className="font-serif font-normal italic">Stay in control.</span></h2><p>Leave the USB-connected camera with your Windows PC. Pair by QR, then manage your session over the local network. Enable Turbo Live for a faster preview with the same image quality and focus overlays. The connection stays active in the background; photos stay on the PC.</p></div>
    <div className="android-gallery" role="region" aria-label="Android application screenshots" tabIndex={0}>
      {screens.map(screen => {
        const dimensions = captures.images[screen.image];
        return <figure className="android-screen" key={screen.image}>
          <a href={publicAsset(`/screenshots/${screen.image}`)} target="_blank" rel="noreferrer" aria-label={`Open full-resolution screenshot: ${screen.title}`}><img src={publicAsset(`/screenshots/${screen.image}`)} alt={`AstroPhoto Remote on Pixel 8 Pro: ${screen.title}`} width={dimensions.width} height={dimensions.height} loading="lazy" decoding="async" /></a>
          <figcaption><h3>{screen.title}</h3><p>{screen.text}</p></figcaption>
        </figure>;
      })}
    </div>
    <div className="android-caption"><p>Actual Android 0.4.6 captures from the connected Pixel 8 Pro, 24 November 2026, without a camera. No preview is simulated. Pairing, settings and the update API were tested against the installed Windows host without a camera; optical acquisition was not requalified.</p><a className="text-link" href="#downloads">Get the Android APK ↓</a><a className="text-link" href={`${repository}/blob/main/docs/ANDROID_REMOTE.md`}>Pairing and setup ↗</a></div>
  </section>;
}
