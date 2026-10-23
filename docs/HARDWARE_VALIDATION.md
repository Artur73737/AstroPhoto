# Collaudo Canon 1100D

Il 23 ottobre 2026: backend compilato con header/import library e DLL EDSDK 13.19.0.6400 x64. Windows rileva Canon EOS 1100D, USB VID_04A9/PID_3217. Riusciti apertura sessione, lettura proprieta, chiusura e download di 10 JPEG Live View. Questo verifica il collegamento reale, non certifica ancora scatti/Bulb/focus o tutte le condizioni di errore sotto elencate. Origine e hash SDK: `external/edsdk-manifest.json`.

1. Registrare versione EDSDK, architettura DLL, versione firmware e obiettivo. Verificare la 1100D nella compatibility list del pacchetto.
2. Compilare `ASTRO_WITH_EDSDK=ON`; verificare simboli e firme contro gli header reali. Nessun SDK sostitutivo ammesso.
3. Apertura/chiusura ripetute, USB assente, EOS Utility aperto, due camere collegate, spegnimento/disconnessione durante idle e download. Verificare che gli errori siano visibili e che non restino riferimenti SDK.
4. Leggere descrittori ISO/Av/Tv/WB/qualita e confrontarli con il display. Scrivere e rileggere solo valori disponibili. Verificare proprieta senza descrittore e quelle impostabili solo dalla ghiera. Aggiungere le etichette dalla documentazione ufficiale della versione.
5. Scatto normale RAW, JPEG e RAW+JPEG. Verificare numero di file, estensioni, integrita e corrispondenza EXIF. Il conteggio multiplo basato sull'encoding ImageQuality deve essere verificato per la versione effettiva.
6. Provare Bulb USB da 2 s e 30 s. Confermare la combinazione supportata da 1100D/firmware fra Tv Bulb, AE e comandi BulbStart/BulbEnd. Il sorgente non assume che il piano utente basti a provare la compatibilita. Verificare l'esposizione tramite EXIF con un parser reale; la dimensione del file non dimostra la durata.
7. Stop, timeout, chiusura finestra ed errore USB durante Bulb: verificare fisicamente chiusura otturatore. Un arresto con errore SDK non garantisce che l'otturatore si sia chiuso: registrare l'errore e verificare il corpo camera prima di riprendere.
8. Live View: almeno 10 JPEG reali, dimensioni decodificabili, stop con ripristino dei bit output precedenti. Zoom e focus solo con corpo/obiettivo compatibili.
9. Disco quasi pieno, cartella non scrivibile, percorsi Unicode, collisioni di nome e guasto I/O durante download. Verificare file originali intatti e artefatti parziali distinguibili.
10. Prima di implementare conversione RAW, verificare se il pacchetto Canon scelto include le API RAW necessarie. Non assumere che `EdsSaveImage` sia disponibile e supportato in qualsiasi EDSDK moderno. Conservare sempre il CR2 originale.

Restano da implementare anche salvataggio su scheda, telemetria Bulb, posizioni EVF, parametri PictureStyle e test automatici EXIF/cattura. I test attuali d'integrazione coprono sessione/proprieta in lettura e Live View, non sostituiscono questo collaudo.
