"use client";
import { useCallback, useEffect, useState } from "react";
import { fetchReleases, normalizeReleases, visibleReleases, latestForPlatform, installerPlatform, type ReleaseSnapshot, type Installer } from "@/lib/releases";
import { repository } from "@/lib/paths";
import { Reveal } from "@/components/Reveal";

function CopyHash({ installer }: { installer: Installer }) {
  const [message, setMessage] = useState("Copy SHA-256");
  return <button type="button" className="text-link" aria-label={`Copy SHA-256 for ${installer.name}`} onClick={async () => {
    try { await navigator.clipboard.writeText(installer.sha256!); setMessage("Copied"); }
    catch { setMessage("Select and copy the hash below"); }
  }}>{message}</button>;
}

export function DownloadsSection({ initial }: { initial: ReleaseSnapshot }) {
  const [catalog, setCatalog] = useState(initial);
  const [status, setStatus] = useState("Release metadata synchronized from GitHub");
  const [busy, setBusy] = useState(false);
  const [expandedReleases, setExpandedReleases] = useState<Set<number>>(() => new Set());
  const refresh = useCallback(async () => {
    setBusy(true);
    setStatus("Checking GitHub for release updates…");
    try {
      const releases = normalizeReleases(await fetchReleases());
      setCatalog({ checkedAt: new Date().toISOString(), releases });
      setStatus("Up to date with GitHub Releases");
    } catch {
      setStatus("Live sync unavailable. Showing the last synchronized release metadata.");
    } finally { setBusy(false); }
  }, []);
  useEffect(() => { void refresh(); }, [refresh]);
  const latest = catalog.releases.find(release => !release.prerelease && release.installers.length > 0);
  const visible = visibleReleases(catalog.releases);
  const platforms = [latestForPlatform(catalog.releases, "Windows"), latestForPlatform(catalog.releases, "Android")].filter(item => item !== null);
  return <section id="downloads" className="section-shell">
    <div className="section-eyebrow"><span>04 / Downloads</span><span>Published on GitHub</span></div>
    <div className="section-heading"><Reveal><h2>Ready for your<br /><span className="font-serif font-normal italic">next session.</span></h2></Reveal><p>Install AstroPhoto on your Windows PC and AstroPhoto Remote on Android. The PC controls the USB camera and stores every photo; your phone provides the remote viewfinder and controls.</p></div>
    <div className="download-body">
      <div className="sync-status" role="status"><span>{status}<br /><span className="sync-date">Last checked: {new Date(catalog.checkedAt).toISOString().replace("T", " ").slice(0, 19)} UTC</span></span><button type="button" className="text-link" onClick={() => void refresh()} disabled={busy}>{busy ? "Checking…" : "Refresh releases"}</button></div>
      <div className="platform-downloads">{platforms.map(({ release, installer }) => {
        const android = installerPlatform(installer) === "Android";
        return <div className="latest-download" key={installer.id}>
          <div><p className="hud-label">{android ? "Android 8.0+ / APK" : "Windows 10 & 11 / x64 MSI"} · {release.tag}</p><h3>{android ? "AstroPhoto Remote" : "AstroPhoto for Windows"}</h3><p>{android ? "Pair by QR on your local network. Live View, capture, sequences and focus controls. Requires the running Windows app." : "Canon SDK and Visual C++ runtime included. Per-user installation, with no administrator rights needed at the default location."}</p></div>
          <a className="download-button" href={installer.url}>Download {android ? "Android APK" : "Windows MSI"} <span aria-hidden="true">↓</span></a>
        </div>;
      })}</div>
      <p className="release-note">The Windows MSI is not Authenticode-signed. The Android release APK is signed with the project Android key and distributed outside Google Play. SHA-256 verifies file integrity. Pair on a trusted local network: WebSocket traffic is authenticated but not encrypted.</p>
      <h3 className="archive-heading">Current release + three previous.</h3>
      {catalog.releases.length === 0 && <p>No public releases are currently listed by GitHub. <a className="text-link" href={`${repository}/releases`}>Check GitHub Releases ↗</a></p>}
      <div className="release-list">{visible.map(release => <article key={release.id} className="release-entry" data-expanded={expandedReleases.has(release.id)}>
        <div className="release-heading"><div><a href={release.url} className="release-tag">{release.tag} ↗</a><span className="release-kind">{release.prerelease ? "Pre-release" : release.id === latest?.id ? "Latest stable" : "Previous release"}</span></div><time dateTime={release.publishedAt}>{new Date(release.publishedAt).toISOString().slice(0,10)}</time></div>
        {release.installers.map(installer => <div key={installer.id} className="installer-entry">
          <div className="installer-top"><a href={installer.url} className="installer-name">{installer.name} ↓</a><span>{installerPlatform(installer)} · {(installer.size / 1048576).toFixed(2)} MiB</span></div>
          <div className="release-extra" id={`checksum-${release.id}-${installer.id}`}>
          <div className="hash-label"><span className="hud-label">SHA-256 / GitHub</span>{installer.sha256 && <CopyHash key={installer.sha256} installer={installer} />}</div>
          {installer.sha256 ? <code className="hash-value">{installer.sha256}</code> : <p>Hash not provided by GitHub. Check the attached checksum or release notes.</p>}
          {installer.checksumUrl && <a className="text-link checksum-link" href={installer.checksumUrl}>Download .sha256 ↗</a>}
          </div>
        </div>)}
        {!release.installers.length && <p>No MSI or APK attached to this release.</p>}
        <button type="button" className="release-details-toggle" aria-expanded={expandedReleases.has(release.id)} aria-controls={[...release.installers.map(installer => `checksum-${release.id}-${installer.id}`), `release-files-${release.id}`].join(" ")} onClick={() => setExpandedReleases(previous => {
          const next = new Set(previous);
          if (next.has(release.id)) next.delete(release.id); else next.add(release.id);
          return next;
        })}>Checksums &amp; release files <span aria-hidden="true">{expandedReleases.has(release.id) ? "−" : "+"}</span></button>
        <div className="release-extra" id={`release-files-${release.id}`}><div className="release-assets"><a href={release.url} className="text-link">Release notes ↗</a>{release.assets.map(asset => <a key={asset.url} href={asset.url} className="text-link">{asset.name} ↗</a>)}</div></div>
      </article>)}</div>
      <p className="release-note"><a className="text-link" href={`${repository}/releases`}>View the complete release history on GitHub ↗</a></p>
      <details className="verify-details"><summary>How to verify your download</summary><p>Open PowerShell in your Downloads folder and run the command below, replacing the filename with the MSI or APK you downloaded. Compare the full result with the SHA-256 shown above.</p><pre><code>Get-FileHash ./AstroPhoto-VERSION-x64.msi -Algorithm SHA256</code></pre><p>SHA-256 values come from GitHub’s release asset metadata. Installers and checksum files are hosted on GitHub and linked directly. This page refreshes release metadata from GitHub; the site build only reads that metadata.</p></details>
    </div>
  </section>;
}
