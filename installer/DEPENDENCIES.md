# Included runtime dependencies

The AstroPhoto Windows x64 MSI is self-contained for Windows 10 and Windows 11.
Installation copies all application runtime dependencies automatically, offline,
beside `AstroCanonController.exe`:

- Official signed ZWO ASI 3.29 x64 USB driver staged using Windows PnPUtil, plus ASICamera2 1.42 runtime.
- ASTAP executable and complete G05/W08 star catalogs for offline DSS/True North solving.
- Three original DSS2 test fields and provenance for the clearly labelled numerical demo.
- Canon EDSDK camera runtime: `EDSDK.dll` and `EdsImage.dll`.
- Microsoft Visual C++ x64 runtime DLLs from the Visual Studio redistributable,
  including `msvcp140.dll`, `msvcp140_atomic_wait.dll`, `vcruntime140.dll` and
  `vcruntime140_1.dll`.
- The font, default configuration, application icon and third-party notices.

Dear ImGui, GLFW, LibRaw, JSON, HTTP, IXWebSocket, the QR code generator and the adapted Hocus Focus math are compiled
into the application. No Python, .NET, Visual Studio, Canon EOS Utility or
separate SDK installation is required to run AstroPhoto. Canon SDK headers and
import libraries are development files and are not needed on the destination PC.

The installer performs app-local deployment of Visual C++, rather than launching
a nested installer or modifying the machine-wide runtime. It installs for the
current user at the default location. Administrator consent is required for the signed ZWO driver installation. Windows
supplies the Universal CRT, USB/PTP device support and system libraries. A working
OpenGL graphics driver is required; Canon uses that inbox PTP support; the MSI installs the vendor ZWO driver package through Windows driver-store APIs.

`dependencies.json` records the packaged executable/DLL versions and SHA-256
hashes. The packaging script rejects missing runtime files, non-x64 binaries and
an executable/installer version mismatch. Canon notices are in `licenses/EDSDK`;
the original component terms continue to apply.

Deployment reference: [Microsoft Universal CRT deployment](https://learn.microsoft.com/en-us/cpp/windows/universal-crt-deployment).
