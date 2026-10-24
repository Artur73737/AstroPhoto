# Local dependencies

`scripts/bootstrap.ps1` downloads the public dependencies into this directory and verifies their pinned Git commits. Downloaded source directories are ignored by Git.

| Directory | Version | Upstream | License |
| --- | --- | --- | --- |
| imgui | 1.91.9b-docking | https://github.com/ocornut/imgui | MIT; Cousine font: Apache-2.0 |
| glfw | 3.4 | https://github.com/glfw/glfw | zlib |
| googletest | 1.16.0 | https://github.com/google/googletest | BSD-3-Clause |
| libraw | 0.22.2 | https://github.com/LibRaw/LibRaw | CDDL-1.0 option |
| json | 3.11.3 | https://github.com/nlohmann/json | MIT |
| hocus-focus (reference) | bced3c30d8a86114e2578039149667242737b942 | https://github.com/ghilios/hocus-focus | MPL-2.0 |

Preserve the distributions' license and copyright notices.

The complete Hocus Focus reference checkout is restored separately with
`scripts/fetch-hocus-focus.ps1`. Its selected numerical algorithms are adapted
to C++ in the tracked core sources; the NINA plugin is not a build dependency.
See [adaptation notes](../docs/HOCUS_FOCUS.md) for source provenance and scope.

## Canon EDSDK

The local development package is EDSDK 13.19.0.6400 x64, obtained from the public mirror https://github.com/shucai-ok/Cannon_EDSDK at commit `ef05251e6292dea66f6a113bf6adcd3826348aaa`.

The repository contains `edsdk-manifest.json` with exact URLs and SHA-256 hashes, but no SDK binaries or headers. Run `scripts/install-edsdk.ps1` from the project root to verify or restore those local files. This mirror is not the official Canon download service. The hashes identify artifacts rather than authenticate their publisher; the DLLs have no Authenticode signature. Canon SDK terms and retained notices apply.

```text
EDSDK/
  EDSDK/Header/EDSDK.h
  EDSDK/Header/EDSDKErrors.h
  EDSDK/Header/EDSDKTypes.h
  EDSDK_64/Library/EDSDK.lib
  EDSDK_64/Dll/EDSDK.dll
  EDSDK_64/Dll/EdsImage.dll
  Document/readme.txt
  Document/ReleaseNote.txt
  Sample/Property/...
```

For another SDK layout, configure `EDSDK_ROOT` or the individual `EDSDK_INCLUDE_DIR`, `EDSDK_LIBRARY`, `EDSDK_DLL` and `EDSDK_IMAGE_DLL` CMake paths. Use matching versions and architecture. CMake copies both runtime DLLs beside the app and tests.

Connection, property access and Live View have been tested on a physical Canon EOS 1100D with this package. These operations do not require the package's optional DPP development libraries. The Canon sample property tables are retained locally as references for the readable labels.
