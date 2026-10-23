#include <astro/ConfigManager.hpp>
#include <gtest/gtest.h>
#include <fstream>
#include <limits>
using namespace astro;
namespace {
struct ConfigTest:testing::Test {
    std::filesystem::path dir;
    void SetUp()override{dir=std::filesystem::temp_directory_path()/("astro-config-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);}
    void TearDown()override{std::filesystem::remove_all(dir);}
};
TEST_F(ConfigTest,RoundTripPreservesSettingsAndLabels) {
    Config c;c.outputDirectory=pathFromUtf8("output/caff\xc3\xa8");c.sessionName="Orione";c.sequence.push_back({});c.sequence[0].settings[Property::Iso]=72;c.labels[Property::Iso][72]="ISO 100";
    auto path=dir/"config.json";ConfigManager::save(path,c);auto out=ConfigManager::load(path);
    EXPECT_EQ(out.outputDirectory,c.outputDirectory);EXPECT_EQ(out.sessionName,"Orione");EXPECT_EQ(out.sequence[0].settings.at(Property::Iso),72);EXPECT_EQ(out.labels.at(Property::Iso).at(72),"ISO 100");
    c.sessionName="Andromeda";ConfigManager::save(path,c);EXPECT_EQ(ConfigManager::load(path).sessionName,"Andromeda");EXPECT_FALSE(std::filesystem::exists(path.string()+".tmp"));
}
TEST_F(ConfigTest,InvalidSavePreservesPreviousFile) {
    Config c;auto p=dir/"config.json";ConfigManager::save(p,c);c.tickMs=0;
    EXPECT_THROW(ConfigManager::save(p,c),std::invalid_argument);EXPECT_GT(ConfigManager::load(p).tickMs,0);
}
TEST_F(ConfigTest,ConnectionRecoverySettingsRoundTripAndBounds) {
    Config c;c.keepAliveSeconds=5;c.reconnectAttempts=4;c.reconnectDelayMs=500;
    auto path=dir/"connection.json";ConfigManager::save(path,c);auto restored=ConfigManager::load(path);
    EXPECT_EQ(restored.keepAliveSeconds,5);EXPECT_EQ(restored.reconnectAttempts,4);EXPECT_EQ(restored.reconnectDelayMs,500);
    c.keepAliveSeconds=0;
    EXPECT_THROW(validate(c),std::invalid_argument);
    c.keepAliveSeconds=10;c.reconnectAttempts=-1;
    EXPECT_THROW(validate(c),std::invalid_argument);
    c.reconnectAttempts=0;EXPECT_NO_THROW(validate(c));
    c.reconnectDelayMs=0;
    EXPECT_THROW(validate(c),std::invalid_argument);
}
TEST_F(ConfigTest,StarAutofocusRoundTripAndBounds){Config c;c.starFocus.step=2;c.starFocus.positions=11;c.starFocus.framesPerPoint=7;c.starFocus.backlash=4;c.starFocus.detectionSnr=8;auto p=dir/"stars.json";ConfigManager::save(p,c);auto restored=ConfigManager::load(p);EXPECT_EQ(restored.starFocus.step,2);EXPECT_EQ(restored.starFocus.positions,11);EXPECT_EQ(restored.starFocus.framesPerPoint,7);EXPECT_EQ(restored.starFocus.backlash,4);EXPECT_EQ(restored.starFocus.detectionSnr,8);c.starFocus.positions=10;
    EXPECT_THROW(validate(c),std::invalid_argument);c.starFocus.positions=9;c.starFocus.verificationTolerance=1;
    EXPECT_THROW(validate(c),std::invalid_argument);}
TEST_F(ConfigTest,SingleShotIntervalAndThemeRoundTrip) {
    Config c;c.lightTheme=true;c.singleCapture.type=FrameType::Dark;c.singleCapture.bulb=true;c.singleCapture.exposureSeconds=15;
    c.sequence.push_back({});c.sequence[0].intervalSeconds=3.5;
    auto path=dir/"capture.json";ConfigManager::save(path,c);auto out=ConfigManager::load(path);
    EXPECT_TRUE(out.lightTheme);EXPECT_EQ(out.singleCapture.type,FrameType::Dark);EXPECT_TRUE(out.singleCapture.bulb);
    EXPECT_EQ(out.singleCapture.exposureSeconds,15);EXPECT_EQ(out.sequence[0].intervalSeconds,3.5);
    c.sequence[0].intervalSeconds=-1;
    EXPECT_THROW(validate(c),std::invalid_argument);
}
TEST_F(ConfigTest,LanguageAndFocusPeakingPersist) {
    Config c;c.language="en";c.focusPeaking=true;c.starPeaking=true;c.peakingThreshold=62;
    auto p=dir/"preferences.json";ConfigManager::save(p,c);auto restored=ConfigManager::load(p);
    EXPECT_EQ(restored.language,"en");EXPECT_TRUE(restored.focusPeaking);EXPECT_TRUE(restored.starPeaking);EXPECT_FLOAT_EQ(restored.peakingThreshold,62);
    c.language="unknown";
    EXPECT_THROW(validate(c),std::invalid_argument);
    c.language="it";c.peakingThreshold=101;
    EXPECT_THROW(validate(c),std::invalid_argument);
}
TEST_F(ConfigTest,DestinationAndReticlePreferencesPersist) {
    Config c;c.saveDestination=SaveDestination::PcAndCamera;c.crosshairEnabled=false;c.crosshairShape="target";
    auto path=dir/"reticle.json";ConfigManager::save(path,c);auto restored=ConfigManager::load(path);
    EXPECT_EQ(restored.saveDestination,SaveDestination::PcAndCamera);EXPECT_FALSE(restored.crosshairEnabled);EXPECT_EQ(restored.crosshairShape,"target");
    for(const auto* shape:{"cross","gap_cross","circle","target","thirds"}){c.crosshairShape=shape;EXPECT_NO_THROW(validate(c));}
    c.crosshairShape="invalid";
    EXPECT_THROW(validate(c),std::invalid_argument);
    EXPECT_EQ(Config{}.saveDestination,SaveDestination::PcOnly);
}
TEST_F(ConfigTest,UiScalePersistsAndOldConfigKeepsAutomaticDpi) {
    Config c;c.uiScale=1.25f;auto path=dir/"ui.json";ConfigManager::save(path,c);EXPECT_FLOAT_EQ(ConfigManager::load(path).uiScale,1.25f);
    std::ifstream in(path);std::string data((std::istreambuf_iterator<char>(in)),{});in.close();
    auto start=data.find("  \"ui_scale\":");ASSERT_NE(start,std::string::npos);
    auto end=data.find('\n',start);data.erase(start,end-start+1);{std::ofstream out(path);out<<data;}
    EXPECT_FLOAT_EQ(ConfigManager::load(path).uiScale,1.0f);
}
TEST(Configuration,RejectsInvalidUiScale) {
    for(float scale:{0.0f,-1.0f,3.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        Config c;c.uiScale=scale;
    EXPECT_THROW(validate(c),std::invalid_argument);
    }
}
TEST_F(ConfigTest,RejectsMalformedAndMissingConfiguration) {
    EXPECT_THROW(ConfigManager::load(dir/"missing.json"),std::runtime_error);
    auto p=dir/"bad.json";{std::ofstream f(p);f<<"{ invalid";}EXPECT_THROW(ConfigManager::load(p),std::exception);
    {std::ofstream f(p);f<<"{\"schema_version\":2}";}EXPECT_THROW(ConfigManager::load(p),std::invalid_argument);
}
TEST_F(ConfigTest,RejectsMisspelledKeysAndFractionalIntegers) {
    Config c;auto p=dir/"config.json";ConfigManager::save(p,c);
    std::ifstream f(p);std::string text((std::istreambuf_iterator<char>(f)),{});f.close();
    auto pos=text.find("\"tick_ms\": 20");ASSERT_NE(pos,std::string::npos);text.replace(pos,13,"\"tick_ms\": 2.5");{std::ofstream o(p);o<<text;}EXPECT_THROW(ConfigManager::load(p),std::exception);
    {std::ofstream o(p);o<<"{\"schema_version\":1,\"typo\":0}";}EXPECT_THROW(ConfigManager::load(p),std::invalid_argument);
}
TEST(Configuration,RejectsNonFiniteExposureAndInvalidFrameType) {
    Config c;c.sequence.push_back({});c.sequence[0].exposureSeconds=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(validate(c),std::invalid_argument);
    c.sequence[0].exposureSeconds=1;c.sequence[0].type=static_cast<FrameType>(99);
    EXPECT_THROW(validate(c),std::invalid_argument);
}
TEST(Configuration,ShippedDefaultsAreValid) {auto c=ConfigManager::load(std::filesystem::path(ASTRO_SOURCE_DIR)/"src/configuration/defaults.json");EXPECT_TRUE(c.sequence.empty());EXPECT_GT(c.downloadTimeoutSeconds,0);}
}
