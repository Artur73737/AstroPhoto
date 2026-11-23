# Camera model modules

The Canon EOS 1100D module also advertises native point-autofocus support and
owns the legacy Live AF mode and AF-frame size policy. The EDSDK adapter executes
the commands and reads fresh selected-point focus status; other camera models
must explicitly implement their own point-AF policy before advertising support.

AstroPhoto ships one model module: **Canon EOS 1100D**. This refactor preserves
its existing behavior, including Bulb, RAW/JPEG transfer accounting, AF/MF lens
gating, three Canon lens-drive sizes and the metadata-based 10x Live View crop.
No other camera models or untested regional aliases are registered.

## Selection

Use the dropdown at the top of **Connection** before connecting:

- **Automatic detection** (default) resolves the camera's ProductName against
  the module's exact model names, ignoring case and redundant whitespace.
- **Canon EOS 1100D** explicitly selects the module, but still verifies that the
  attached body matches. It cannot force 1100D behavior onto an unknown camera.

The active module is displayed separately from the requested selection. Save
preferences to persist it as `camera_module` (`auto` or `canon_eos_1100d`). Old
settings files without this key default to `auto`; existing exposure plans,
focus tuning, file locations and UI preferences are preserved. Selection is
locked during connection/reconnection and acquisition. Disconnect first to
change it; reconnecting does not require restarting the application.

MCP exposes the same setting through `update_settings`, the registry in
`get_state.state.camera_modules`, and the resolved module/capabilities in
`get_state.state.camera_module`. Its settings and zoom schemas are generated
from the compiled registry. Use `save_settings` to persist a selection.

## Layout and boundaries

```text
src/cameras/
  models/registry/              # SDK-free policy API and generated registry
  models/canon_eos_1100d/       # module, capture, focus, live_view, tests
  canon/edsdk/                 # camera/property SDK boundary
  zwo/sdk/                     # isolated ASI C API owner
  zwo/adapter/                 # shared ICamera integration

Each leaf feature has include/astro/... and src directories.
```

Modules contain immutable C++ policy and protocol data. They never access USB,
EDSDK handles, the filesystem or the UI. Only `edsdk_wrapper` includes Canon SDK
headers and executes commands. It resolves the module before registering camera
callbacks or renewing the camera timer; failed selection closes the session.
Disconnect clears the active module and a new connection verifies the identity
again. The worker remains the sole camera owner; UI and MCP still submit commands
through the mailbox on their existing separate threads.

Canon-wide property-label decoding, SDK/COM lifetime, download acknowledgement,
save-destination restoration, power protection and connection recovery remain
shared. Actual ISO/Av/Tv choices still come from `EdsGetPropertyDesc`, not a model
table. User-adjustable exposure/autofocus settings remain user configuration;
choosing a module does not silently overwrite them. The autofocus algorithms
and the large -> medium -> small sequence are retained.

## Adding a model later

1. Create `models/<model_id>/` with its own `CMakeLists.txt`, `include`, `src`
   submodules and `tests`. Implement `CameraModule` with the model's verified
   identity, properties, capture/focus policy and preview geometry.
2. In that folder's CMake file, add its sources to `astro_camera_models` and call
   `astro_register_camera_module(<unique_factory_function>)`. The factory is a
   function in `astro::camera_models` returning `const CameraModule&` backed by
   an immutable object with static lifetime. Register test source paths through
   the `ASTRO_CAMERA_MODULE_TESTS` global property, as the 1100D module does.
3. Reconfigure/rebuild. CMake discovers the folder; the registry, UI and MCP
   enumerate it without editing a model switch in the application. Modules are
   compiled C++ components, not hot-loaded scripts or DLLs. Copying source into
   an installed application's directory does not enable it without rebuilding.
4. Validate on that physical model before adding its names/capabilities. Unknown
   and ambiguous auto matches are rejected; there is no generic fallback that
   assumes another body behaves like a 1100D.

Additional transport operations, if a future body genuinely needs them, belong
in the shared EDSDK adapter with a corresponding SDK-free policy extension.
Do not put SDK includes into a model folder or copy the entire adapter per model.

## Tests

Software tests cover selection, mismatch rejection, legacy config migration,
MCP schema validation, 1100D capture/focus policy and the existing 10x pixel crop.
The opt-in `Hardware.CameraModuleAutomaticManualAndReconnect` test checks auto,
manual and auto selection again with the physical 1100D in one process. Existing
Live View/zoom tests exercise real frames. These tests do not take still photos.

## ZWO provider

`zwo_asi` selects the ASI provider, not a Canon model policy. Auto discovery prefers
an enumerated ASI; select the Canon model explicitly when both are attached. Native
controls and RAW16 FITS differ from Canon ISO/Av/Tv and CR2. The core router keeps
one stable ICamera boundary and owns only one connected backend. All SDK work
remains on the camera worker. Physical ASI validation is pending.
