import { publicAsset, repository } from "@/lib/paths";
import captures from "@/content/screenshots.json";

const screens = [
  { image: "Screenshot_20261031-212516.png", title: "Your remote viewfinder", text: "Live View, camera settings, histograms and previews of photos saved on the PC." },
  { image: "Screenshot_20261031-212741.png", title: "Focus at your fingertips", text: "Lens steps, point measurements and peaking. The current release includes Normal and Stars modes." },
  { image: "Screenshot_20261031-212745.png", title: "Set up the next exposure", text: "Camera-reported choices, single captures, sequences and PC saving preferences." },
  { image: "Screenshot_20261031-212752.png", title: "Keep the details in reach", text: "Advanced settings and presentation options, confirmed by the Windows controller." },
] as const;

export function AndroidSection() {
  return <section id="android" className="section-shell">
    <div className="section-eyebrow"><span>Companion / Android</span><span>Kotlin · Jetpack Compose · Material 3</span></div>
    <div className="section-heading"><h2>Step inside.<br /><span className="font-serif font-normal italic">Stay in control.</span></h2><p>Leave the USB-connected camera with your Windows PC. Pair your phone by scanning its QR, then manage the session over your local network. The connection stays active in the background; photos stay on the PC.</p></div>
    <div className="android-gallery" role="region" aria-label="Android application screenshots" tabIndex={0}>
      {screens.map(screen => {
        const dimensions = captures.images[screen.image];
        return <figure className="android-screen" key={screen.image}>
          <a href={publicAsset(`/screenshots/${screen.image}`)} target="_blank" rel="noreferrer" aria-label={`Open full-resolution screenshot: ${screen.title}`}><img src={publicAsset(`/screenshots/${screen.image}`)} alt={`AstroPhoto Remote on Pixel 8 Pro: ${screen.title}`} width={dimensions.width} height={dimensions.height} loading="lazy" decoding="async" /></a>
          <figcaption><h3>{screen.title}</h3><p>{screen.text}</p></figcaption>
        </figure>;
      })}
    </div>
    <div className="android-caption"><p>Original screenshots supplied by the project author, from a Canon EOS 1100D indoor monitor test. Some controls have evolved in the current release. Portrait and landscape layouts are supported.</p><a className="text-link" href="#downloads">Get the Android APK ↓</a><a className="text-link" href={`${repository}/blob/main/docs/ANDROID_REMOTE.md`}>Pairing and setup ↗</a></div>
  </section>;
}
