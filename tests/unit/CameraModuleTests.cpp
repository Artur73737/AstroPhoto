#include <astro/CameraModule.hpp>
#include <astro/ConfigManager.hpp>
#include <gtest/gtest.h>
#include <algorithm>
using namespace astro;
TEST(CameraModules,RegistryHasUniqueIdsAndContainsValidated1100D){
    ASSERT_FALSE(camera_models::modules().empty());
    EXPECT_EQ(camera_models::find("canon_eos_1100d").info().name,"Canon EOS 1100D");
    std::vector<std::string> ids;
    for(const auto* m:camera_models::modules()){
        EXPECT_FALSE(m->info().id.empty());EXPECT_FALSE(m->modelNames().empty());
        EXPECT_EQ(std::count(ids.begin(),ids.end(),m->info().id),0);ids.push_back(m->info().id);
    }
}
TEST(CameraModules,AutomaticAndManualSelectionResolveSameModule){
    const auto& detected=camera_models::resolve("auto","  CANON  EOS 1100D  ");
    EXPECT_EQ(&detected,&camera_models::resolve("canon_eos_1100d","Canon EOS 1100D"));
}
TEST(CameraModules,Canon1100dPointAfUsesLegacyLiveSinglePointPolicy){
    const auto& module=camera_models::find("canon_eos_1100d");
    EXPECT_TRUE(module.info().pointAutofocus);EXPECT_EQ(module.pointAutofocusMode(),1u);
    EXPECT_DOUBLE_EQ(module.pointAutofocusFrameFraction(),.2);
}
TEST(CameraModules,UnknownOrMismatchedModelsCannotUse1100DPolicy){
    for(const char* model:{"","Canon EOS 1100D Mark II","Canon EOS 1100","Other camera"}){
        EXPECT_THROW(camera_models::resolve("auto",model),CameraError);
        EXPECT_THROW(camera_models::resolve("canon_eos_1100d",model),CameraError);
    }
    EXPECT_THROW(camera_models::resolve("missing","Canon EOS 1100D"),std::invalid_argument);
}
TEST(CameraModules,SelectionRoundTripsWithoutChangingUserSettings){
    Config c;c.cameraModule="canon_eos_1100d";c.starFocus.settleMs=900;c.liveViewMs=150;c.sessionName="Existing session";
    auto json=ConfigManager::toJson(c);auto loaded=ConfigManager::fromJson(json);
    EXPECT_EQ(loaded.cameraModule,c.cameraModule);EXPECT_EQ(loaded.starFocus.settleMs,900);EXPECT_EQ(loaded.liveViewMs,150);EXPECT_EQ(loaded.sessionName,c.sessionName);
    const auto start=json.find("  \"camera_module\"");ASSERT_NE(start,std::string::npos);
    json.erase(start,json.find('\n',start)-start+1);
    EXPECT_EQ(ConfigManager::fromJson(json).cameraModule,"auto");
    c.cameraModule="missing";EXPECT_THROW(ConfigManager::toJson(c),std::invalid_argument);
}
