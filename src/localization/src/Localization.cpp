#include <astro/Localization.hpp>
#include <map>
#include <optional>
#include <vector>
namespace astro {
namespace {
#include "TranslationCatalog.inc"
const std::map<std::string,std::string,std::less<>> messages{
    {"Pausa dopo lo scatto e l'elaborazione in corso","Pausing after current exposure and processing"},
    {"Live stack in pausa","Live stack paused"},
    {"Live stack ripreso","Live stack resumed"},
    {"Ricostruzione dai CR2 originali","Rebuilding from original CR2 files"},
    {"Recupero con quadrilateri / raffinamento affine","Quad recovery / affine refinement"},
    {"Budget memoria superato dallo stack / drizzle; ridurre scala o aumentare budget","Stack / drizzle exceeds memory budget; lower scale or increase budget"},

    {"Live stack avviato","Live stack started"},
    {"Live stack completato","Live stack finished"},
    {"Live stack fermato dall'utente","Live stack stopped by user"},
    {"Arresto dopo lo scatto e l'elaborazione in corso","Stopping after current exposure and processing"},
    {"Esposizione / download RAW","Exposure / RAW download"},
    {"Decodifica RAW / allineamento / integrazione","Decoding RAW / aligning / integrating"},
    {"Integrato (allineamento disabilitato)","Integrated (alignment disabled)"},
    {"Allineato e integrato","Aligned and integrated"},
    {"Riferimento accettato","Reference accepted"},
    {"Il riferimento richiede piu stelle distribuite e non sature","Reference requires more distributed, unsaturated stars"},
    {"Stelle distribuite insufficienti per l'allineamento","Insufficient distributed alignment stars"},
    {"Allineamento scartato: corrispondenze, geometria o residui insufficienti","Alignment rejected: insufficient unique matches, geometry or residual quality"},
    {"Nessun frame ha superato la validazione RAW/allineamento; originali conservati","No frame passed RAW/alignment validation; originals retained"},
    {"La fotocamera e stata disconnessa durante il live stack","Camera disconnected during live stack"},
    {"Parametri della fotocamera modificati durante il live stack; avviare un nuovo stack","Camera settings changed during live stack; start a new stack"},
    {"Timeout dello scatto/download nel live stack","Live stack capture/download timed out"},
    {"Spazio disco insufficiente per il live stack","Insufficient disk space for live stack"},
    {"Spazio disco insufficiente per il prossimo RAW e lo stack finale","Insufficient disk space for the next RAW and final stack"},
    {"Il live stack richiede RAW, esposizione manuale e ISO fisso; impostarli in Fotocamera","Live stacking requires RAW, manual exposure and fixed ISO; set them in Camera"},
    {"Fermare il live stack e attendere il salvataggio prima di altre operazioni","Stop live stacking and wait for final save before other camera actions"},
    {"Live stack gia attivo","Live stack already active"},
    {"Il RAW supera il limite di memoria configurato","RAW exceeds configured memory budget"},
    {"Le dimensioni RAW sono cambiate","RAW dimensions changed"},
    {"Nessun CR2 scaricato; selezionare RAW in Fotocamera","No CR2 was downloaded; select RAW in Camera"},
    {"Parametri live stack non validi (intervallo minimo 2 secondi)","Invalid live stack settings (delay must be at least 2 seconds)"},
    {"Quantita/esposizione/ritardo non validi (durata massima 24h)","Invalid count/exposure/delay (maximum duration 24h)"},
    {"Scala UI fuori intervallo (0.75 - 2.0)","UI scale out of range (0.75 - 2.0)"},
    {"ID modulo fotocamera non valido","Invalid camera module ID"},
    {"ID modulo fotocamera duplicato","Duplicate camera module ID"},
    {"Modulo fotocamera ambiguo: selezionare manualmente il modulo corrispondente","Ambiguous camera module: select the matching module manually"},
    {"Informazioni","Info"},
    {"Avviso","Warning"},
    {"Errore","Error"},
    {"Scorrimento automatico","Auto-scroll"},
    {"Canon EDSDK disponibile","Canon EDSDK available"},
    {"EDSDK assente: editor disponibile, controllo hardware disabilitato","EDSDK unavailable: editor available, hardware control disabled"},
    {"EDSDK non installato: ricompilare con ASTRO_WITH_EDSDK=ON","EDSDK not installed: rebuild with ASTRO_WITH_EDSDK=ON"},
    {"Impossibile aprire astro.log; log disponibile solo nella UI","Cannot open astro.log; logging is only available in the UI"},
    {"Sequenza avviata","Sequence started"},
    {"Sequenza gia attiva","Sequence already active"},
    {"Sequenza vuota","Empty sequence"},
    {"Fotocamera disconnessa durante la sequenza","Camera disconnected during the sequence"},
    {"Spazio disco insufficiente","Insufficient disk space"},
    {"Trasferimento senza file","Transfer completed without files"},
    {"Timeout in attesa del download; verificare la fotocamera prima di riprovare","Download timed out; check the camera before retrying"},
    {"Autofocus stellare avviato","Stellar autofocus started"},
    {"Autofocus stellare gia in corso","Stellar autofocus already running"},
    {"Arrestare la sequenza prima di modificare la fotocamera","Stop the sequence before changing camera settings"},
    {"Fotocamera connessa: standby automatico di Windows e spegnimento schermo bloccati","Camera connected: Windows automatic sleep and display timeout inhibited"},
    {"Le richieste di alimentazione della connessione richiedono Windows","Connection power requests require Windows"},
    {"Timer del worker fotocamera","Camera worker timer"},
    {"Chiusura richiesta di alimentazione","CloseHandle power request"},
    {"Canon riconnessa; scatti, autofocus e Live View restano fermi","Canon reconnected; capture, autofocus and Live View remain stopped"},
    {"Riconnessione sospesa: verificare alimentazione/USB e premere Connetti","Reconnection suspended: check power/USB and press Connect"},
    {"Canon: rinnovo timer temporaneamente occupato; nuovo tentativo al prossimo intervallo","Canon: shutdown timer renewal busy; retrying at the next interval"},
    {"Canon: rinnovo timer non supportato; disabilitare Auto power off sulla fotocamera","Canon: shutdown timer renewal unsupported; disable Auto power off on the camera"},
    {"Canon: SDK reinizializzato per aggiornare il rilevamento USB","Canon: SDK reinitialized to refresh USB detection"},
    {"Canon ha notificato Shutdown (spegnimento, standby o perdita USB)","Canon reported Shutdown (power off, standby or USB loss)"},
    {"Riconnessione Canon non riuscita","Canon reconnection failed"},
    {"Salvataggio: solo PC (nessuna copia sulla scheda)","Saving: PC only (no copy on the camera card)"},
    {"Salvataggio: PC + scheda fotocamera","Saving: PC + camera card"},
    {"Fotocamera occupata","Camera busy"},
    {"Operazione non supportata dalla fotocamera/SDK","Operation not supported by the camera/SDK"},
    {"Oggetto non ancora pronto","Object not ready yet"},
    {"Connessione USB interrotta","USB connection lost"},
    {"Fotocamera USB non trovata","USB camera not found"},
    {"Riferimento alla fotocamera non piu valido","Camera handle is no longer valid"},
    {"Errore del bus USB","USB bus error"},
    {"Sessione gia aperta; chiudere EOS Utility e riconnettere","Session already open; close EOS Utility and reconnect"},
    {"Sessione gia aperta","Session already open"},
    {"USB in uso da un altro programma (es. EOS Utility)","USB in use by another program (e.g. EOS Utility)"},
    {"Errore della scheda; verificare presenza, spazio e stato","Card error; check presence, free space and status"},
    {"Errore Canon EDSDK","Canon EDSDK error"},
    {"Nessuna fotocamera Canon rilevata","No Canon camera detected"},
    {"Collegare una sola fotocamera Canon","Connect only one Canon camera"},
    {"EDSDK deve essere chiamato dal suo worker STA","EDSDK must be called from its STA worker"},
    {"Proprieta non supportata","Unsupported property"},
    {"Descrittore proprieta non valido","Invalid property descriptor"},
    {"Estensione file camera non valida","Invalid camera file extension"},
    {"Dimensione download non valida","Invalid download size"},
    {"Nomi file esauriti","No available file names"},
    {"Scatto precedente ancora attivo","Previous capture is still active"},
    {"Il Tv corrente non corrisponde alla scelta Bulb; impostarlo sulla fotocamera o nel menu Tv","Current Tv does not match Bulb selection; set it on the camera or in the Tv menu"},
    {"Frame Live View troppo grande","Live View frame is too large"},
    {"Attivare Live View per la messa a fuoco","Enable Live View for focusing"},
    {"Attivare Live View per lo zoom","Enable Live View for zoom"},
    {"Movimento focus non disponibile: collegare un obiettivo compatibile e impostarlo su AF","Lens drive unavailable: attach a compatible lens and set it to AF"},
    {"Passo focus non valido","Invalid focus step"},
    {"Zoom non valido per il modulo camera","Invalid zoom for the camera module"},
    {"La fotocamera non ha applicato la destinazione di salvataggio","Save destination not applied"},
    {"Disconnettere prima di selezionare un modulo fotocamera","Disconnect before selecting a camera module"},
    {"Proprieta non supportata dal modulo fotocamera selezionato","Property is not supported by the selected camera module"},
    {"Il riavvio SDK richiede una sessione chiusa","SDK reset requires a closed session"},
    {"Coda comandi fotocamera piena","Camera command queue is full"},
    {"Calibrazione corsa: richiesti obiettivo AF, M ed esposizione fissa","AF lens, M and fixed exposure required for travel calibration"},
    {"Fotocamera disconnessa durante la calibrazione corsa","Camera disconnected during travel calibration"},
    {"Fotocamera o obiettivo AF non disponibili durante la calibrazione corsa","Camera or AF lens unavailable during travel calibration"},
    {"Geometria Live View cambiata durante la calibrazione corsa","Live View geometry changed during travel calibration"},
    {"Nessuna stella persistente nella corsa calibrata dell'obiettivo","No persistent stars within calibrated lens travel"},
    {"Limiti ottici calibrati; obiettivo azzerato al limite vicino","Optical travel limits calibrated; lens reset near"},
    {"Geometria del segnale di calibrazione corsa cambiata","Travel calibration signal geometry changed"},
    {"Corsa non misurabile","Travel range is not measurable"},
    {"Opzioni calibrazione corsa non valide","Invalid focus travel calibration options"},
    {"Limiti qualita curva focus non validi","Invalid focus curve quality limits"},
    {"Passo motore focus non valido","Invalid focus drive step"},
    {"Opzioni ricerca stelle non valide","Invalid stellar acquisition options"},
    {"Opzioni autofocus stellare non valide","Invalid stellar autofocus options"},
    {"Impostazioni recupero connessione non valide","Invalid connection recovery settings"},
    {"Destinazione salvataggio non valida","Invalid save destination"},
    {"Forma mirino non valida","Invalid crosshair shape"},
    {"Nome sessione non valido","Invalid session name"},
    {"Nome sessione riservato da Windows","Session name reserved by Windows"},
    {"Intervallo tra scatti non valido","Invalid interval between captures"},
    {"Configurazione fuori intervallo","Configuration out of range"},
    {"Codice etichetta non valido","Invalid label code"},
    {"Codice proprieta fuori intervallo","Property code out of range"},
    {"Codice proprieta non intero","Property code is not an integer"},
    {"Etichette non valide","Invalid labels"},
    {"Errore scrittura configurazione","Configuration write error"},
    {"Intero fuori intervallo","Integer out of range"},
    {"Oggetto JSON richiesto","JSON object required"},
    {"reserve_bytes deve essere positivo","reserve_bytes must be positive"},
    {"sequence deve essere un array","sequence must be an array"},
    {"settings deve essere un oggetto","settings must be an object"},
    {"Valore intero richiesto","Integer value required"},
    {"Versione configurazione non supportata","Unsupported configuration version"},
    {"Proprieta sconosciuta","Unknown property"},
    {"Tipo frame sconosciuto","Unknown frame type"},
    {"Geometria Live View 10x Canon 1100D non valida","Invalid Canon 1100D 10x Live View geometry"},
    {"Lingua non supportata","Unsupported language"},
    {"Soglia peaking fuori intervallo","Peaking threshold out of range"},
    {"Inizializzazione SDK","SDK initialization"},
    {"Fine Bulb","End Bulb"},
    {"Inizio Bulb","Start Bulb"},
    {"Rimozione handler oggetti","Remove object event handler"},
    {"Rimozione handler stato","Remove state event handler"},
    {"Chiusura sessione","Close session"},
    {"Rinnovo timer spegnimento Canon","Renew Canon shutdown timer"},
    {"Riavvio SDK: terminazione","SDK restart: terminate"},
    {"Riavvio SDK: inizializzazione","SDK restart: initialize"},
    {"Annullamento trasferimento fuori sequenza","Cancel out-of-sequence transfer"},
    {"Informazioni file","File information"},
    {"Stream download","Download stream"},
    {"Puntatore download","Download pointer"},
    {"Dimensione download","Download size"},
    {"Conferma download","Complete download"},
    {"Elenco fotocamere","Camera list"},
    {"Numero fotocamere","Camera count"},
    {"Selezione fotocamera","Select camera"},
    {"Apertura sessione","Open session"},
    {"Nome fotocamera","Camera name"},
    {"Registrazione eventi file","Register file events"},
    {"Registrazione eventi stato","Register state events"},
    {"Eventi SDK","SDK events"},
    {"Capacita host","Host capacity"},
    {"Scatto","Capture"},
    {"Stream Live View","Live View stream"},
    {"Frame Live View","Live View frame"},
    {"Download Live View","Live View download"},
    {"Dati Live View","Live View data"},
    {"Dimensione Live View","Live View size"},
    {"Zoom del frame Live View","Live View frame zoom"},
    {"Coordinate Live View","Live View coordinates"},
    {"Origine Live View","Live View origin"},
    {"Rettangolo zoom Live View","Live View zoom rectangle"},
    {"Lettura presenza obiettivo","Read lens presence"},
    {"Lettura AF/MF obiettivo","Read lens AF/MF"},
    {"Messa a fuoco","Focus drive"},
    {"Stato AF/MF non riconosciuto","Unrecognized AF/MF state"},
    {"Stato obiettivo non disponibile","Lens status unavailable"},
    {"Stato obiettivo non riconosciuto","Unrecognized lens state"},
    {"PowerSetRequest schermo","PowerSetRequest display"},
    {"PowerSetRequest sistema","PowerSetRequest system"},
};
const auto reverse=[]{
    std::map<std::string,std::string,std::less<>> result;
    for(const auto& [it,en]:translations)result.try_emplace(en,it);
    for(const auto& [it,en]:messages)result.insert_or_assign(en,it);
    return result;
}();
struct Pattern {std::string_view italian,english;bool nestedLast{};};
const Pattern patterns[]{
    {"Live stack terminato con frame scartati: {} / {}","Live stack finished with rejected frames: {} / {}"},
    {"Live stack frame {}","Live stack frame {}"},
    {"Server WebSocket in ascolto sulla porta {}","WebSocket server listening on port {}",false},
    {"Dispositivo autenticato: {}","Device authenticated: {}",false},
    {"Dispositivo disconnesso: {}","Device disconnected: {}",false},
    {"Ricerca ampia: movimenti {}, stelle {}, miglior HFD {}","Coarse survey: moves {}, stars {}, best HFD {}",false},
    {"Misura focus: passo {}, comandi {}, direzione {}, HFD {}, rumore {}, delta {}, soglia {}, frame {}, tempo ms {}","Focus measure: drive {}, commands {}, direction {}, HFD {}, noise {}, delta {}, threshold {}, frames {}, elapsed ms {}",false},
    {"Modulo fotocamera sconosciuto: {}","Unknown camera module: {}",false},
    {"Il modulo {} non corrisponde alla fotocamera rilevata: {}","Camera module {} does not match detected camera: {}",false},
    {"Nessun modulo fotocamera per il modello rilevato: {}","No camera module for detected model: {}",false},
    {"Configurazione salvata: {}","Configuration saved: {}",false},
    {"Connessa: {}","Connected: {}",false},
    {"Salvato: {}","Saved: {}",false},
    {"Scatto singolo: {}","Single capture: {}",false},
    {"Prossimo scatto tra {}","Next capture in {}",false},
    {"Modulo fotocamera: {}","Camera module: {}",false},
    {"Canon: sessione aperta, rinnovo timer ogni {} s","Canon: session opened, shutdown timer renewed every {} s",false},
    {"Canon: spegnimento automatico tra {} s; rinnovo richiesto","Canon: automatic shutdown in {} s; renewal requested",false},
    {"Riconnessione {}/{}: {}","Reconnection {}/{}: {}",true},
    {"AF stelle: limiti ottici verificati, azzeramento vicino; corsa stimata {} comandi ampi, movimenti calibrazione {}","Star AF: optical limits verified, reset near; estimated range {} large commands, calibration moves {}",false},
    {"AF stelle: passo Canon {} verificato, HFD {} px","Star AF: verified Canon drive {}, HFD {} px",false},
    {"AF stelle: passata {}, passo Canon {}, distanza {}, movimenti ricerca {}","Star AF: pass {}, Canon drive {}, spacing {}, search moves {}",false},
    {"AF stelle: posizione {}, HFD {} px, dispersione {}","Star AF: step {}, HFD {} px, scatter {}",false},
    {"Analisi Live View: {}","Live View analysis: {}",true},
    {"Eventi SDK durante disconnessione: {}","SDK events during disconnection: {}",true},
    {"DownloadCancel: {}","DownloadCancel: {}",true},
    {"TerminateSDK: {}","TerminateSDK: {}",true},
    {"Lettura {}","Read {}",false},
    {"Impostazione {}","Set {}",false},
    {"Descrizione {}","Describe {}",false},
    {"Valori ammessi {}","Allowed values for {}",false},
    {"Valore non disponibile: {}","Value unavailable: {}",false},
    {"CoInitializeEx fallita: {}","CoInitializeEx failed: {}",false},
    {"Creazione file fallita: {}","File creation failed: {}",false},
    {"Scrittura download incompleta: {}","Incomplete download write: {}",false},
    {"Impossibile leggere {}","Cannot read {}",false},
    {"Chiave sconosciuta: {}","Unknown key: {}",false},
    {"Errore sostituzione configurazione: {}","Configuration replacement error: {}",false},
    {"Proprieta sconosciuta: {}","Unknown property: {}",false},
    {"Tipo frame sconosciuto: {}","Unknown frame type: {}",false},
};
std::optional<std::vector<std::string>> match(std::string_view text,std::string_view pattern){
    std::vector<std::string> values;
    while(true){
        const auto marker=pattern.find("{}");
        if(marker==std::string_view::npos)return text==pattern?std::optional{values}:std::nullopt;
        if(!text.starts_with(pattern.substr(0,marker)))return {};
        text.remove_prefix(marker);pattern.remove_prefix(marker+2);
        const auto next=pattern.find("{}");const auto delimiter=pattern.substr(0,next);
        const auto end=delimiter.empty()?text.size():text.find(delimiter);
        if(end==std::string_view::npos)return {};
        values.emplace_back(text.substr(0,end));text.remove_prefix(end);
    }
}
std::string format(std::string_view pattern,const std::vector<std::string>& values){
    std::string result;
    for(const auto& value:values){const auto marker=pattern.find("{}");result+=pattern.substr(0,marker);result+=value;pattern.remove_prefix(marker+2);}
    result+=pattern;return result;
}
std::string translate(std::string_view text,bool english,int depth){
    if(depth>12)return std::string(text);
    // Normalize legacy bilingual diagnostics, without changing opaque values.
    if(text=="La fotocamera non ha applicato la destinazione di salvataggio / Save destination not applied")text="La fotocamera non ha applicato la destinazione di salvataggio";
    if(text=="Lingua non supportata / Unsupported language")text="Lingua non supportata";
    if(text=="Soglia peaking fuori intervallo / Peaking threshold out of range")text="Soglia peaking fuori intervallo";
    if(english){
        if(auto it=messages.find(text);it!=messages.end())return it->second;
        if(auto it=translations.find(text);it!=translations.end())return it->second;
    }else if(auto it=reverse.find(text);it!=reverse.end())return it->second;
    for(const std::string_view delimiter:{"; arresto: ","; stopping: "})
        if(const auto stop=text.find(delimiter);stop!=std::string_view::npos)
            return translate(text.substr(0,stop),english,depth+1)+(english?"; stopping: ":"; arresto: ")+translate(text.substr(stop+delimiter.size()),english,depth+1);
    // Split structured SDK errors before matching property-value templates.
    if(const auto code=text.rfind(" (0x");code!=std::string_view::npos&&text.ends_with(")"))
        return translate(text.substr(0,code),english,depth+1)+std::string(text.substr(code));
    if(const auto separator=text.rfind(": ");separator!=std::string_view::npos){
        const auto head=text.substr(0,separator);const auto localized=translate(head,english,depth+1);
        bool known=messages.contains(head)||translations.contains(head)||reverse.contains(head);
        for(const auto& p:patterns)if(match(head,p.italian)||match(head,p.english)){known=true;break;}
        if(localized!=head||known)return localized+": "+translate(text.substr(separator+2),english,depth+1);
    }
    for(const auto& pattern:patterns){
        // Match either source language so mixed nested SDK errors are normalized.
        auto values=match(text,pattern.italian);if(!values)values=match(text,pattern.english);
        if(values){if(pattern.nestedLast&&!values->empty())values->back()=translate(values->back(),english,depth+1);return format(english?pattern.english:pattern.italian,*values);}
    }
    if(const auto separator=text.find(" | ");separator!=std::string_view::npos)
        return translate(text.substr(0,separator),english,depth+1)+std::string(text.substr(separator));
    return std::string(text);
}
}
std::string localizeMessage(std::string_view text,std::string_view language){return translate(text,language=="en",0);}
std::string localizeLogLevel(std::string_view level,std::string_view language){
    if(language=="it"){if(level=="Info")return "Informazioni";if(level=="Warning")return "Avviso";if(level=="Error")return "Errore";}
    return std::string(level);
}
}
