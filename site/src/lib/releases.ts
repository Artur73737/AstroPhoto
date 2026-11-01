export const REPOSITORY = "Artur73737/AstroPhoto";
export const API = `https://api.github.com/repos/${REPOSITORY}`;
export interface GitHubAsset { id: number; name: string; size: number; state: string; digest: string | null; browser_download_url: string; updated_at: string; }
export interface GitHubRelease { id: number; tag_name: string; name: string | null; html_url: string; published_at: string; draft: boolean; prerelease: boolean; assets: GitHubAsset[]; }
export interface Installer { id: number; name: string; url: string; size: number; sha256: string | null; checksumUrl: string | null; updatedAt: string; }
export interface Release { id: number; tag: string; name: string; url: string; publishedAt: string; prerelease: boolean; installers: Installer[]; assets: { name: string; url: string }[]; }
export interface ReleaseSnapshot { checkedAt: string; releases: Release[]; }
type Fetcher = (url: string, init?: RequestInit) => Promise<Response>;

export const PAST_RELEASE_LIMIT = 3;
export function visibleReleases(releases: Release[]): Release[] {
  return releases.slice(0, PAST_RELEASE_LIMIT + 1);
}
export function installerPlatform(installer: Installer): "Windows" | "Android" {
  return /\.apk$/i.test(installer.name) ? "Android" : "Windows";
}
// Windows and Android have independent version numbers. Never use the bundle
// release tag as an APK version, even when both assets belong to the same release.
export function installerVersion(installer: Installer): string | null {
  return /^(?:AstroPhotoRemote-|AstroPhoto-)(\d+\.\d+\.\d+)(?:-x64)?\.(?:apk|msi)$/i.exec(installer.name)?.[1] ?? null;
}

export function latestForPlatform(releases: Release[], platform: "Windows" | "Android") {
  const candidates = releases.filter(item => !item.prerelease).flatMap(release =>
    release.installers.filter(file => installerPlatform(file) === platform).map(installer => ({ release, installer })));
  candidates.sort((a, b) => {
    const av = installerVersion(a.installer)?.split(".").map(Number);
    const bv = installerVersion(b.installer)?.split(".").map(Number);
    if (av && bv) {
      for (let i = 0; i < 3; i++) if (av[i] !== bv[i]) return bv[i] - av[i];
    } else if (av || bv) return av ? -1 : 1;
    return Date.parse(b.release.publishedAt) - Date.parse(a.release.publishedAt)
      || Date.parse(b.installer.updatedAt) - Date.parse(a.installer.updatedAt);
  });
  return candidates[0] ?? null;
}

export function releaseUrl(value: string): string {
  const url = new URL(value);
  if (url.origin !== "https://github.com" || !url.pathname.startsWith(`/${REPOSITORY}/releases/`)) throw new Error("Unexpected release URL");
  return url.href;
}

export function normalizeReleases(releases: GitHubRelease[]): Release[] {
  return releases.filter(release => !release.draft).map(release => {
    const assets = release.assets.filter(asset => asset.state === "uploaded");
    return {
      id: release.id, tag: release.tag_name, name: release.name || release.tag_name,
      url: releaseUrl(release.html_url), publishedAt: release.published_at, prerelease: release.prerelease,
      installers: assets.filter(asset => /\.(msi|apk)$/i.test(asset.name)).map(asset => ({
        id: asset.id, name: asset.name, url: releaseUrl(asset.browser_download_url), size: asset.size,
        sha256: /^sha256:[a-f\d]{64}$/i.test(asset.digest || "") ? asset.digest!.slice(7).toLowerCase() : null,
        checksumUrl: assets.find(sidecar => sidecar.name === `${asset.name}.sha256`)?.browser_download_url
          ? releaseUrl(assets.find(sidecar => sidecar.name === `${asset.name}.sha256`)!.browser_download_url) : null,
        updatedAt: asset.updated_at,
      })),
      assets: assets.filter(asset => !/\.(msi|apk)$/i.test(asset.name)).map(asset => ({ name: asset.name, url: releaseUrl(asset.browser_download_url) })),
    };
  }).sort((a,b) => Date.parse(b.publishedAt) - Date.parse(a.publishedAt));
}

export async function fetchReleases(fetcher: Fetcher = fetch, headers: HeadersInit = {}): Promise<GitHubRelease[]> {
  const all: GitHubRelease[] = [];
  for (let page = 1; ; page++) {
    const response = await fetcher(`${API}/releases?per_page=100&page=${page}`, {
      headers: { Accept: "application/vnd.github+json", ...headers }, cache: "no-store",
      signal: AbortSignal.timeout(20000),
    });
    if (!response.ok) throw new Error(`GitHub releases request failed (${response.status})`);
    const releases = await response.json() as GitHubRelease[];
    if (!Array.isArray(releases)) throw new Error("Invalid GitHub releases response");
    for (const release of releases) {
      // The embedded collection may be truncated for releases with many assets.
      if (release.assets.length >= 100) {
        const assets: GitHubAsset[] = [];
        for (let assetPage = 1; ; assetPage++) {
          const result = await fetcher(`${API}/releases/${release.id}/assets?per_page=100&page=${assetPage}`, { headers, signal: AbortSignal.timeout(20000), cache: "no-store" });
          if (!result.ok) throw new Error(`GitHub assets request failed (${result.status})`);
          const batch = await result.json() as GitHubAsset[];
          if (!Array.isArray(batch)) throw new Error("Invalid GitHub assets response");
          assets.push(...batch);
          if (batch.length < 100) break;
        }
        release.assets = assets;
      }
      all.push(release);
    }
    if (releases.length < 100) break;
  }
  return all;
}

