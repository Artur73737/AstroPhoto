import assert from "node:assert/strict";
import { readFile, access, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import path from "node:path";
const root = fileURLToPath(new URL("../out/", import.meta.url));
const html = await readFile(path.join(root, "index.html"), "utf8");
const base = process.env.NEXT_PUBLIC_BASE_PATH || "";
assert.ok(html.includes("Your camera."));
assert.ok(html.includes("AstroPhoto"));
assert.ok(!/lorem ipsum|placeholder visual|lorem@/i.test(html), "Template content remains");
assert.ok(!/[\p{Emoji_Presentation}\uFE0F]/u.test(html), "Unexpected emoji in exported page");
for (const section of ["workspace", "features", "ownership", "downloads", "start"]) assert.ok(html.includes(`id="${section}"`), `Missing ${section}`);
const urls = [...html.matchAll(/(?:src|href)="([^"]+)"/g)].map(match => match[1]);
let checked = 0;
for (const url of new Set(urls)) {
  if (url.startsWith("#")) {
    assert.ok(html.includes(`id="${url.slice(1)}"`), `Broken anchor ${url}`);
  } else if (url.startsWith("/")) {
    assert.ok(!base || url.startsWith(base + "/"), `Missing base path: ${url}`);
    const local = decodeURIComponent(url.slice(base.length).split("?")[0]);
    await access(path.join(root, local));
    checked++;
  }
}
const snapshot = JSON.parse(await readFile(new URL("../src/content/releases.json", import.meta.url), "utf8"));
for (const release of snapshot.releases) for (const installer of release.installers) {
  assert.ok(html.includes(installer.name), `Missing installer ${installer.name}`);
  if (installer.sha256) assert.ok(html.includes(installer.sha256), `Missing SHA-256 ${installer.name}`);
}
for (const filename of ["all.png", "live-view.png", "camera.png", "sequences.png"]) await access(path.join(root, "screenshots", filename));
const outputFiles = await readdir(root, { recursive: true });
assert.ok(!outputFiles.some(file => /\.(msi|exe|zip)$/i.test(file)), "Installer or archive copied into the website");
console.log(`Static export verified: ${checked} local assets, all release installers/hashes, navigation anchors, screenshot files and base path "${base}".`);
