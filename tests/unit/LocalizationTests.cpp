#include <astro/Localization.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
using astro::localizeMessage;
TEST(LogLocalization,BothLanguagesNormalizeMixedSourceMessages){
    EXPECT_EQ(localizeMessage("Canon EDSDK disponibile","en"),"Canon EDSDK available");
    EXPECT_EQ(localizeMessage("Sequence started","it"),"Sequenza avviata");
    EXPECT_EQ(localizeMessage("Stellar autofocus started","it"),"Autofocus stellare avviato");
    EXPECT_EQ(localizeMessage("Canon EDSDK disponibile","it"),"Canon EDSDK disponibile");
    EXPECT_EQ(localizeMessage("Canon: sessione aperta, rinnovo timer ogni 10 s","en"),"Canon: session opened, shutdown timer renewed every 10 s");
    EXPECT_EQ(localizeMessage("Canon: session opened, shutdown timer renewed every 10 s","it"),"Canon: sessione aperta, rinnovo timer ogni 10 s");
}
TEST(LogLocalization,StructuredAutofocusNumbersRemainUnchanged){
    const std::string en="Star AF: pass 3, Canon drive 2, spacing 8, search moves 12";
    const std::string it="AF stelle: passata 3, passo Canon 2, distanza 8, movimenti ricerca 12";
    EXPECT_EQ(localizeMessage(en,"it"),it);EXPECT_EQ(localizeMessage(it,"en"),en);
    EXPECT_EQ(localizeMessage("Star AF: step -22, HFD 2.421933 px, scatter 0.043165","it"),"AF stelle: posizione -22, HFD 2.421933 px, dispersione 0.043165");
    EXPECT_EQ(localizeMessage("Best measured focus restored; fine precision unconfirmed (small-drive response below noise)","it"),"Miglior fuoco misurato ripristinato; precisione fine non confermata (risposta del passo piccolo sotto il rumore)");
}
TEST(LogLocalization,NestedSdkAndRecoveryErrorsRetainCodes){
    const std::string it="Riconnessione 2/12: Lettura ISO: Fotocamera occupata (0x81)";
    const std::string en="Reconnection 2/12: Read ISO: Camera busy (0x81)";
    EXPECT_EQ(localizeMessage(it,"en"),en);EXPECT_EQ(localizeMessage(en,"it"),it);
    EXPECT_EQ(localizeMessage("Riavvio SDK: terminazione: Errore Canon EDSDK (0xABCDEF)","en"),"SDK restart: terminate: Canon EDSDK error (0xABCDEF)");
    EXPECT_EQ(localizeMessage("Analisi Live View: Fotocamera non connessa","en"),"Live View analysis: Camera not connected");
    EXPECT_EQ(localizeMessage("Fotocamera occupata; arresto: Connessione USB interrotta","en"),"Camera busy; stopping: USB connection lost");
    EXPECT_EQ(localizeMessage("Camera busy; stopping: USB connection lost","it"),"Fotocamera occupata; arresto: Connessione USB interrotta");
}
TEST(LogLocalization,FilePathsAndUnknownDiagnosticsAreOpaque){
    const std::string path=R"(C:\foto\Sequenza completata\Warning\stelle e citta.CR2)";
    EXPECT_EQ(localizeMessage("Salvato: "+path,"en"),"Saved: "+path);
    EXPECT_EQ(localizeMessage("Saved: "+path,"it"),"Salvato: "+path);
    EXPECT_EQ(localizeMessage("Configurazione salvata: "+path,"en"),"Configuration saved: "+path);
    EXPECT_EQ(localizeMessage("External diagnostic #773: opaque vendor payload","it"),"External diagnostic #773: opaque vendor payload");
}
TEST(LogLocalization,SeverityAndLegacyBilingualErrors){
    EXPECT_EQ(astro::localizeLogLevel("Info","it"),"Informazioni");
    EXPECT_EQ(astro::localizeLogLevel("Warning","it"),"Avviso");
    EXPECT_EQ(astro::localizeLogLevel("Error","it"),"Errore");
    EXPECT_EQ(astro::localizeLogLevel("Warning","en"),"Warning");
    EXPECT_EQ(localizeMessage("Lingua non supportata / Unsupported language","it"),"Lingua non supportata");
    EXPECT_EQ(localizeMessage("Lingua non supportata / Unsupported language","en"),"Unsupported language");
}
TEST(LogLocalization,ConcurrentLanguagesDoNotLeakAcrossThreads){
    std::atomic<int> errors{};
    std::jthread en([&]{for(int i=0;i<1000;++i)if(localizeMessage("Connessa: Canon EOS 1100D","en")!="Connected: Canon EOS 1100D")++errors;});
    std::jthread it([&]{for(int i=0;i<1000;++i)if(localizeMessage("Connected: Canon EOS 1100D","it")!="Connessa: Canon EOS 1100D")++errors;});
    en.join();it.join();EXPECT_EQ(errors,0);
}
