import { mkdir, writeFile } from "node:fs/promises";
import { fetchReleases, normalizeReleases } from "../src/lib/releases.ts";

// Request JSON metadata only. MSI and APK installers and checksum files stay on GitHub.
const headers = process.env.GITHUB_TOKEN ? { Authorization: `Bearer ${process.env.GITHUB_TOKEN}` } : {};
const releases = normalizeReleases(await fetchReleases(fetch, headers));
const snapshot = { checkedAt: new Date().toISOString(), releases };
const target = new URL("../src/content/releases.json", import.meta.url);
await mkdir(new URL("../src/content/", import.meta.url), { recursive: true });
await writeFile(target, JSON.stringify(snapshot, null, 2) + "\n");
const count = releases.reduce((total, release) => total + release.installers.length, 0);
console.log(`Synchronized metadata for ${releases.length} releases and ${count} MSI/APK links. Installers remain on GitHub.`);
