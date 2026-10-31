# AstroPhoto website

English presentation of the real C++20 application, built with Next.js and exported as static HTML for GitHub Pages.

## Local development

Requires Node.js 24 and npm.

```powershell
cd site
npm ci
npm run prepare:assets
npm run sync:releases
npm run dev
```

Development runs at `http://127.0.0.1:4173/`. The explicit loopback address and port avoid Windows TCP reservations that can block the default port 3000 with `EACCES`. If `NEXT_PUBLIC_BASE_PATH` is set, append that path to the URL. Development and static preview both use port 4173; stop one before starting the other.

`prepare:assets` copies the complete seven-panel workspace and three unmodified native MCP panel captures from `../docs/site-captures/` and the real application SVG logo. The main image is the user-supplied `all.png`, preserved unchanged and showing the complete application UI. The capture manifest records source, pixel sizes, frame revisions and the panels used. The Live View image shows a physical Canon aimed at an indoor monitor test scene; it is not presented as a night-sky photograph.

## Build for GitHub Pages

```powershell
$env:NEXT_PUBLIC_BASE_PATH = '/AstroPhoto'
npm run build
npm run check:export
npm run preview
```

The static output is `site/out/`; preview serves it at `http://127.0.0.1:4173/AstroPhoto/`. Clear `NEXT_PUBLIC_BASE_PATH` for a build served at the domain root.

`npm run build` first synchronizes release JSON metadata. It does **not** download, copy, cache, package or mirror MSI installers, checksum files or release archives. Links point directly to the original GitHub release assets.

## Release synchronization

- `scripts/sync-releases.mjs` reads the public GitHub Releases API, follows pagination and saves a compact JSON metadata snapshot.
- Original release tags and filenames are preserved independently.
- SHA-256 values are read from each GitHub asset's `digest` field. No hash is invented or computed by downloading installers. If GitHub has no digest, the page says so and links any attached checksum.
- The page refreshes metadata directly from GitHub on load and with **Refresh releases**. On API/network failure, it keeps the last synchronized snapshot and reports the failure.
- All public releases and their uploaded MSI assets are shown, including prereleases. The primary button selects the newest non-prerelease that has an MSI.
- Builds without MSI assets remain visible with their release notes. Deleted releases disappear after the next successful sync.
- `GITHUB_TOKEN` is optional for local builds and is used only by the build-time API calls. No token enters the browser bundle.
- The site contains no application installer binaries. Downloads start only when a visitor follows a GitHub download link.

## GitHub Pages workflow

`../.github/workflows/pages.yml` validates and deploys the static export on changes to the site, native MCP captures or SVG, and on release changes. Manual dispatch, a daily reconciliation and a `release-assets-updated` repository dispatch also refresh the catalog. PRs build and validate without deploying.

Push these files to `main`, then select **Settings → Pages → Build and deployment → Source → GitHub Actions** in the repository. Run **GitHub Pages** from the Actions tab for the first deployment if necessary. The expected project URL is `https://artur73737.github.io/AstroPhoto/`; deployment is confirmed only by a successful workflow run.

GitHub suppresses most workflow events created with the repository's own `GITHUB_TOKEN`. If another workflow publishes a release using that token, use a release-assets repository dispatch with suitable credentials or the scheduled/manual run. The browser still reads current release metadata directly.

## Validation

```powershell
npm test
npm run typecheck
npm run check:export
```

The release tests cover original metadata, missing hashes, draft/prerelease handling, API failure, pagination and link origins. Export checks validate local assets and base paths, section anchors, release links/hashes, absence of template copy and emojis, and absence of installer binaries in the output.

The application capabilities and limitations are based on the repository documentation. Stellar autofocus precision remains experimental; the page does not claim complete optical validation.

## Scrollbar and UI

Desktop scrolling uses a proportional draggable thumb with pointer capture, track click, a fixed 0–100 scale and a tabular percentage readout. Arrow keys, Page Up/Down, Home and End work on the focused control. Scrolling uses the native browser engine; wheel interception is disabled. Native scrollbars remain available on mobile and before JavaScript mounts. Document resizing updates the scroll range.

The component tests exercise pointer drag/cancel/release, track clicks, keyboard navigation, number formatting, menu closing/focus restoration and blocked theme storage in a DOM environment. They do not replace a graphical browser inspection.
