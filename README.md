# AstroCanon Controller

Applicazione desktop Windows C++20 / Dear ImGui / GLFW per il controllo Canon EDSDK. Connessione USB, lettura proprieta e Live View verificati su una Canon EOS 1100D fisica; scatti e Bulb richiedono ancora il collaudo indicato sotto.

## Stato reale

- **Compilati e verificati:** scatto singolo e sequenze Light/Dark/Flat/Bias (logica di dominio), anteprima locale JPEG/CR2, configurazione JSON, log, motore delle sequenze, filesystem e avvio/rendering dell'app Windows x64.
- **Backend reale compilato e verificato:** EDSDK 13.19.0.6400 x64, apertura/chiusura sessione e lettura proprieta della Canon EOS 1100D, download di 10 frame Live View. Scatti, Bulb, trasferimento foto e focus sono implementati ma non ancora collaudati sull'hardware.
- **Non implementati in questa versione:** conversione CR2 in TIFF/JPEG, selezione destinazione sulla scheda, parametri individuali PictureStyle, posizione zoom/focus, telemetria Bulb e verifica esposizione tramite EXIF. Non sono presentati come funzionalita operative.
- Il build senza SDK permette di modificare/salvare il piano. Nessuna camera simulata, nessuna foto/frame dimostrativo e nessun menu ISO/Av/Tv inventato.

## Avvio

Eseguibile con SDK: `build-canon/Release/AstroCanonController.exe`.

```powershell
./scripts/run.ps1
# Per aprire anche la sessione USB all'avvio:
./scripts/run.ps1 -Connect
```

Configurazione personale: `user.json` accanto all'eseguibile, creata con **Salva piano**. In assenza di questa viene letto `defaults.json`. Percorsi relativi risolti rispetto alla cartella dell'eseguibile. Log persistente: `astro.log` nella stessa cartella. Tenere la distribuzione in una cartella scrivibile.

## Interfaccia ImGui e DPI

La UI usa esclusivamente Dear ImGui (tag ufficiale `v1.91.9b-docking`); GLFW gestisce solo finestra, input e contesto OpenGL. Menu e pannelli flottanti direttamente sul desktop: ogni pannello usa un HWND nativo separato tramite ImGui multi-viewport. Nessun contenitore a schermo intero; l'host OpenGL e nascosto. Trascinare il titolo di ciascun pannello per spostarlo, usare il bordo per ridimensionarlo. Le regioni Win32 arrotondano realmente gli angoli delle finestre; i controlli ImGui hanno anch'essi angoli arrotondati. Font base ridotto da 16 a 13 pixel, padding ridotto. Temi **Light/Dark** in **Impostazioni**. Layout da editor tecnico: menu in alto, finestre compatte con font monospaziato, controlli blu/grigio e categorie espandibili.

- **Connessione** e **Fotocamera** a sinistra: scatto singolo (Light/Dark/Flat/Bias, Bulb e ritardo), esposizione, colore e formato.
- **Immagine** centrale: ultima foto scaricata, con passaggio automatico dopo ogni acquisizione. Scheda Live View con zoom, focus e reticolo. Il CR2 mostra la sua anteprima incorporata tramite LibRaw; il file RAW originale rimane intatto. La decodifica avviene su un thread separato.
- **Sequenze** a destra: lista degli scatti e dettagli della riga selezionata.
- **Registro** e **File e sessione** in basso.

Trascinare le linguette per separare e riagganciare le finestre. Trascinare i divisori per ridimensionare i pannelli. Il menu **Finestre** riapre i pannelli chiusi e permette di ripristinare la disposizione. Posizioni, dimensioni e docking sono salvati automaticamente in `desktop-layout.ini`, separatamente dal piano di scatto. Il vecchio `layout.ini` viene conservato e non viene usato dalla disposizione desktop.

Il processo dichiara Windows Per-Monitor V2. A ogni variazione della scala monitor, il font TTF viene rasterizzato nuovamente e lo stile ImGui viene ricostruito alla nuova scala, evitando ingrandimenti sfocati o scalature cumulative. **Impostazioni > Scala UI** applica un fattore aggiuntivo alla scala di Windows; salvarlo con **Salva preferenze**. Il campo JSON opzionale `ui_scale` mantiene compatibili i piani salvati dalle build precedenti.

Per verificare il rendering a scale diverse senza modificare DPI di Windows, piano o layout personale:

```powershell
./build/Release/AstroCanonController.exe --smoke-test --smoke-dpi=1 --smoke-width=1400 --smoke-height=900
./build/Release/AstroCanonController.exe --smoke-test --smoke-dpi=1.5 --smoke-width=2100 --smoke-height=1350
./build/Release/AstroCanonController.exe --smoke-test --smoke-dpi=2 --smoke-width=1920 --smoke-height=1080
```

Questi test creano sette finestre native nascoste, verificano le regioni arrotondate, producono `desktop-panel-1.png` fino a `desktop-panel-7.png` e scrivono HWND, posizioni e dimensioni in `desktop-smoke.txt`. L'override riguarda solo il rendering di test; il collaudo del passaggio fisico fra monitor con DPI differenti richiede quei monitor.

## Compilazione riproducibile

Prerequisiti: Windows, Visual Studio 2022 con C++ e Windows SDK, CMake >= 3.24, Git. Le dipendenze C++ sono locali in `external` e possono essere riscaricate con `bootstrap.ps1`; versioni e commit sono fissati.

```powershell
./scripts/build.ps1
# oppure:
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DASTRO_WITH_EDSDK=OFF
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

DLL, header e libreria x64 sono presenti in `external/EDSDK`. Provengono dal mirror pubblico [shucai-ok/Cannon_EDSDK](https://github.com/shucai-ok/Cannon_EDSDK/tree/ef05251e6292dea66f6a113bf6adcd3826348aaa), non dal portale Canon. Versione, commit, URL e SHA256 sono registrati in `external/edsdk-manifest.json`. Per verificarli/ripristinarli e compilare:

```powershell
./scripts/install-edsdk.ps1
./scripts/build.ps1 -WithEDSDK
```

CMake con `ASTRO_WITH_EDSDK=ON` richiede header, libreria e DLL reali. Copia EDSDK.dll ed EdsImage.dll accanto agli eseguibili, inclusi i test, insieme alle note delle dipendenze. Nessun header sostitutivo e nessuna simulazione del protocollo Canon.

## Uso previsto con hardware

1. Chiudere EOS Utility e collegare una sola Canon via USB. Premere **Connetti** e leggere le proprieta disponibili.
2. Selezionare il formato nella proprieta ImageQuality della camera. I file vengono trasferiti nel formato originale: la scelta RAW produce CR2 se configurata sulla 1100D; RAW+JPEG produce entrambi.
3. Preparare ISO, Av e Tv nelle liste restituite da `EdsGetPropertyDesc`. Alcune proprieta possono richiedere la ghiera fisica. **Aggiorna proprieta** rilegge lo stato; non c'e ancora aggiornamento automatico su eventi proprieta.
4. Per Bulb selezionare il Tv Bulb compatibile sulla camera e attivare Bulb nella riga. Il valore in secondi comanda la durata Bulb. Negli scatti normali la durata reale e quella di Tv; il valore in secondi e il budget di esposizione prima del timeout download, non un comando del tempo di scatto.
5. Per una sola foto, impostare **Scatto singolo** e premere **Scatta**; non occorre una sequenza. Dark richiede di coprire l'obiettivo. Per le sequenze aggiungere le righe, cartella e sessione, poi avviare. **Attesa iniziale** si applica al primo scatto della riga; **Intervallo (s)** indica il delay dal download completato all'esposizione successiva, incluso il cambio di riga. Le due attese sovrapposte rispettano la scadenza piu lontana, senza sommarsi. Zero disabilita l'attesa aggiuntiva. **Pausa** completa lo scatto in corso e si ferma prima del successivo; **Stop** tenta la chiusura Bulb e disconnette in caso di acquisizione attiva, evitando di associare download tardivi allo scatto successivo. Dopo l'annullamento riconnettere.

**Valori della fotocamera:** ISO, diaframma, tempo, modalita AE, bilanciamento del bianco, stile e formato sono tradotti in etichette leggibili dalle tabelle dei sample Canon EDSDK 13.19 e da EDSDKTypes.h (es. ISO 400, f/11, 1/60 s, M - Manuale, RAW). Anche i valori correnti senza descrittore sono tradotti. I menu continuano a contenere esclusivamente i valori restituiti dalla fotocamera; le tabelle non aggiungono opzioni. `property_labels` nel JSON permette etichette personalizzate. Valori sconosciuti sono indicati come non riconosciuti. I nomi dei file conservano i codici originali ISO/Av/Tv; negli scatti normali il nome non contiene un'esposizione numerica inventata e Bulb include la durata richiesta.

## Architettura e gestione degli errori

`core` dipende solo dalla libreria standard e contiene modello, interfacce, FileOrganizer, SequenceEngine e coda dei comandi. `configuration` usa JSON. `ui` dipende da core e ImGui. `infrastructure` fornisce clock/disco e worker; il factory Canon usa il wrapper. Solo `edsdk_wrapper/src` include header EDSDK.

Tutte le operazioni SDK, inclusi inizializzazione, callback e distruzione, appartengono a un unico worker STA con COM e message pump. La UI invia comandi e legge snapshot protetti. Il wrapper converte gli errori in `CameraError`, intercetta le eccezioni nelle callback C e registra gli errori di cleanup senza lanciarli dai distruttori. `EdsRelease` restituisce un contatore di riferimenti, non un codice errore.

Il motore usa tempo monotono, verifica lo spazio libero, effettua retry limitati con backoff solo per impostazioni idempotenti e non ripete automaticamente uno scatto dal risultato ambiguo. Dopo un errore occorre verificare la camera e avviare un nuovo piano; non esiste ripresa automatica dello scatto fallito. I trasferimenti vengono salvati con creazione esclusiva Win32, evitando sovrascritture. La conferma `EdsDownloadComplete` segue scrittura e flush; i fallimenti tentano `EdsDownloadCancel` e restano visibili nei log.

## Test

I test unitari usano esclusivamente implementazioni di test dei confini del dominio (clock, filesystem, camera) e non sono linkati nell'app. Non simulano EDSDK. Verificano stati, pause, arresto, timeout, disconnessione, errori disco, retry, conteggi, percorsi e configurazione.

```powershell
./scripts/check-boundaries.ps1
./build/Release/AstroCanonController.exe --smoke-test
```

Il comando smoke renderizza 30 frame dell'interfaccia reale in una finestra nascosta, salva i PNG dei singoli pannelli accanto all'eseguibile e termina; non connette ne scatta foto e non modifica il layout personale.

I test hardware sono opzionali. Il 23 ottobre 2026 sono riusciti i test di sessione/proprieta e Live View sulla 1100D collegata:

```powershell
./scripts/build.ps1 -WithEDSDK -HardwareTests
$env:ASTRO_HARDWARE_TESTS='1'
# Opzionale: attiva realmente Live View per acquisire 10 frame
$env:ASTRO_HARDWARE_LIVEVIEW='1'
ctest --test-dir build-canon -C Release -L hardware --output-on-failure
```

Per il collaudo completo e le funzionalita ancora aperte vedere `docs/HARDWARE_VALIDATION.md`.

## Verifica anteprima RAW

`astro_preview_tests` legge un file CR2 reale se `ASTRO_PREVIEW_CR2` contiene il suo percorso; in assenza del file il relativo test viene saltato esplicitamente. La verifica locale usa `build/preview-test.CR2`, Canon 40D, proveniente dai test di rawpy / rawsamples.ch (CC BY-NC-SA 4.0); non e incluso nella distribuzione e non costituisce un test USB della 1100D.

```powershell
$env:ASTRO_PREVIEW_CR2='E:/project-seri/AstroPhoto/build/preview-test.CR2'
ctest --test-dir build -C Release --output-on-failure
./build/Release/AstroCanonController.exe --smoke-test --smoke-light --smoke-preview=E:/project-seri/AstroPhoto/build/preview-test.CR2
```

Il rendering con `--smoke-preview` legge il file indicato e lo etichetta come **File di collaudo**; non simula una cattura.

## Origine delle DLL

Il download da DLLme era bloccato. E stato usato il mirror GitHub documentato sopra per ottenere EDSDK.dll, EdsImage.dll, EDSDK.lib e tutti e tre gli header della stessa distribuzione 13.19.0. Le DLL PE x64 non contengono una firma Authenticode; gli SHA256 registrano esattamente gli artefatti scaricati, senza certificare l'origine Canon. Header e avvisi di copyright originali sono conservati. Il backend e ora abilitato nella build `build-canon`.
