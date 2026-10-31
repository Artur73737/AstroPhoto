import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { fetchReleases, normalizeReleases, releaseUrl } from "../src/lib/releases.ts";
const snapshot = JSON.parse(await readFile(new URL("../src/content/releases.json", import.meta.url), "utf8"));
test("the real release catalog contains GitHub SHA-256 hashes and original GitHub asset URLs", () => {
  assert.ok(snapshot.releases.length);
  for (const release of snapshot.releases) for (const file of release.installers) {
    if (file.sha256) assert.match(file.sha256, /^[a-f0-9]{64}$/);
    assert.equal(releaseUrl(file.url), file.url);
    assert.ok(file.size > 0);
  }
});
test("preserves the original filename independently from the release tag", () => {
  const old = snapshot.releases.find(item => item.tag === "v0.0.1");
  if (old) assert.equal(old.installers[0].name, "AstroPhoto-0.1.1-x64.msi");
});
test("rejects unexpected download origins and repositories", () => {
  assert.throws(() => releaseUrl("https://example.com/file.msi"));
  assert.throws(() => releaseUrl("https://github.com/other/repo/releases/download/v1/file.msi"));
});
test("paginates GitHub releases past 100 entries", async () => {
  const pages = [];
  const release = { id: 1, assets: [] };
  const result = await fetchReleases(async url => {
    pages.push(url);
    return Response.json(pages.length === 1 ? Array(100).fill(release) : [release]);
  });
  assert.equal(result.length, 101);
  assert.match(pages[1], /page=2$/);
  assert.ok(pages.every(url => url.startsWith("https://api.github.com/repos/Artur73737/AstroPhoto/")));
});
test("rejects an API failure instead of replacing the catalog with invented data", async () => {
  await assert.rejects(fetchReleases(async () => new Response("", { status: 403 })), /403/);
});
test("excludes drafts, preserves prereleases and does not invent a missing digest", () => {
  const asset = { id: 12, name: "actual.msi", state: "uploaded", size: 42, digest: null, browser_download_url: "https://github.com/Artur73737/AstroPhoto/releases/download/v1/actual.msi", updated_at: "2026-10-31T00:00:00Z" };
  const release = { id: 1, name: null, tag_name: "v1", html_url: "https://github.com/Artur73737/AstroPhoto/releases/tag/v1", published_at: "2026-10-31T00:00:00Z", draft: false, prerelease: true, assets: [asset] };
  const result = normalizeReleases([release, { ...release, id: 2, draft: true }]);
  assert.equal(result.length, 1);
  assert.equal(result[0].prerelease, true);
  assert.equal(result[0].installers[0].sha256, null);
});
