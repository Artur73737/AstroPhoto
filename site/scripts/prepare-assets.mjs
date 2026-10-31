import { copyFile, mkdir, readFile } from "node:fs/promises";
const source = new URL("../../docs/site-captures/", import.meta.url);
const target = new URL("../public/screenshots/", import.meta.url);
const manifest = JSON.parse(await readFile(new URL("manifest.json", source), "utf8"));
await mkdir(target, { recursive: true });
for (const name of Object.keys(manifest.images)) await copyFile(new URL(name, source), new URL(name, target));
await copyFile(new URL("manifest.json", source), new URL("../src/content/screenshots.json", import.meta.url));
await copyFile(new URL("../../assets/astrophoto.svg", import.meta.url), new URL("../public/logo.svg", import.meta.url));
console.log("Prepared original Windows and Android application captures and the application logo.");
