# Dipendenze

Le dipendenze pubbliche sono scaricate qui da `scripts/bootstrap.ps1`, con revisione Git verificata. Non richiedono installazioni globali.

| Cartella | Versione | Origine | Licenza |
|---|---|---|---|
| imgui | 1.91.9b-docking | https://github.com/ocornut/imgui | MIT; font Cousine: Apache 2.0 |
| glfw | 3.4 | https://github.com/glfw/glfw | zlib |
| googletest | 1.16.0 | https://github.com/google/googletest | BSD-3-Clause |
| libraw | 0.22.2 | https://github.com/LibRaw/LibRaw | LGPL-2.1 oppure CDDL-1.0 (scelta CDDL) |
| json | 3.11.3 | https://github.com/nlohmann/json | MIT |

Conservare i file di licenza delle distribuzioni. Il font monospaziato Cousine-Regular.ttf e gia presente in imgui/misc/fonts.

## Canon EDSDK

EDSDK 13.19.0.6400 x64 presente in `EDSDK`, dal mirror pubblico https://github.com/shucai-ok/Cannon_EDSDK al commit `ef05251e6292dea66f6a113bf6adcd3826348aaa`. Manifest con URL e SHA256: `edsdk-manifest.json`. Verifica/ripristino: `../scripts/install-edsdk.ps1`. Non e un download dal portale Canon; mantenere copyright e avvisi originali.

Struttura locale:

```
EDSDK/
  EDSDK/Header/EDSDK.h
  EDSDK/Header/EDSDKErrors.h
  EDSDK/Header/EDSDKTypes.h
  EDSDK_64/Library/EDSDK.lib
  EDSDK_64/Dll/EDSDK.dll
  EDSDK_64/Dll/EdsImage.dll
  Document/readme.txt
  Document/ReleaseNote.txt
```

Per layout diversi, configurare `EDSDK_INCLUDE_DIR`, `EDSDK_LIBRARY`, `EDSDK_DLL` ed `EDSDK_IMAGE_DLL`. CMake copia entrambe le DLL accanto ad app e test. Connessione, proprieta e Live View verificati sulla Canon 1100D usando questi file; non sono necessarie le librerie DPP per queste operazioni.
