# UI and operation-log language

Select **Settings > Language > Italiano / English**, or set `language` through
native MCP `update_settings`. The change takes effect immediately; use
**Save preferences** / `save_settings` to retain it after restart.

- The Log panel translates retained and new messages, severity labels and its
  controls. Text filtering searches the localized message currently shown.
- New lines in `astro.log` use the language active when they are written.
  Existing records are append-only and are not rewritten after switching.
- MCP `get_logs` returns the selected `language`, localized `message` and
  `level_label`. Its `level` identifier and level filter remain `Info`, `Warning`,
  `Error` for client compatibility. The `contains` filter searches localized text.
- Paths, model names, measurements and hexadecimal diagnostic codes are
  preserved. Application-owned Canon error descriptions and operation contexts
  are translated. Unknown OS/library/vendor error payloads are retained verbatim.

## Implementation

`src/localization` is a standalone C++ library with no UI, Windows or SDK
dependency. UI, infrastructure and MCP share its catalog. Exact translations
cover labels and fixed diagnostics; structured message templates keep variable
data separate from translated text. Nested recovery/SDK errors translate their
context and error description without replacing words inside file paths.

The UI retains source messages and caches translated display strings per
language. Disk logging reads the current language through a synchronized mailbox
flag, taking one language snapshot per line. Changing language does not change
camera configuration, restart the core worker or interrupt capture. MCP returns
the same catalog translations from the UI thread. Unknown messages fall back to
their original text rather than disappearing.

Add translations to `TranslationCatalog.inc` for presentation labels or to the
message/template catalog in `Localization.cpp` for operation diagnostics. Keep
template placeholders in the same order. Add regression coverage for nested
errors and any new parameterized form; never translate paths or numeric values.

Tests cover both directions, autofocus numbers, nested SDK errors, paths,
severity labels, simultaneous callers in different languages, and switching the
disk-log language without restarting the worker. Live application checks use the
native MCP connector, the real log panel and actual `astro.log` writes.
