#include <astro/CameraModule.hpp>
#include <gtest/gtest.h>
using namespace astro;
TEST(Canon1100DModule,CapturePreservesBulbCommandsAndTransferCounts){
    const auto& m=camera_models::find("canon_eos_1100d");
    const auto raw=m.capturePolicy(0x0064ff0f),jpeg=m.capturePolicy(0x0013ff0f),both=m.capturePolicy(0x00640013);
    EXPECT_EQ(raw.expectedFiles,1);EXPECT_EQ(jpeg.expectedFiles,1);EXPECT_EQ(both.expectedFiles,2);
    EXPECT_EQ(raw.bulbTv,0x0c);EXPECT_EQ(raw.takePictureCommand,0);EXPECT_EQ(raw.bulbStartCommand,2);EXPECT_EQ(raw.bulbEndCommand,3);
}
TEST(Canon1100DModule,LensSafetyAndThreeDriveSizesRemainIntact){
    const auto& m=camera_models::find("canon_eos_1100d");
    EXPECT_EQ(m.lensState(0,0),LensFocusState::NoLens);EXPECT_EQ(m.lensState(2,0),LensFocusState::Unknown);
    EXPECT_EQ(m.lensState(1,3),LensFocusState::ManualFocus);EXPECT_EQ(m.lensState(1,99),LensFocusState::Unknown);
    for(unsigned af=0;af<3;++af)EXPECT_EQ(m.lensState(1,af),LensFocusState::Ready);
    for(int step=1;step<=3;++step){EXPECT_EQ(m.focusDrive(-step),step);EXPECT_EQ(m.focusDrive(step),0x8000+step);}
    for(int bad:{-4,0,4})EXPECT_THROW(m.focusDrive(bad),std::invalid_argument);
    EXPECT_TRUE(m.manualExposureForFocus(3,72,88));EXPECT_FALSE(m.manualExposureForFocus(0,72,88));
    EXPECT_FALSE(m.manualExposureForFocus(3,0,88));EXPECT_FALSE(m.manualExposureForFocus(3,72,0x0c));
}
TEST(Canon1100DModule,OnlyTenXNeedsTheValidatedCrop){
    const auto& m=camera_models::find("canon_eos_1100d");LiveFrame f;
    for(int zoom:{1,5}){f.zoom=zoom;EXPECT_EQ(m.previewRegion(f).width,1);}
    f.zoom=10;EXPECT_THROW(m.previewRegion(f),CameraError);
    f.coordinateWidth=4272;f.coordinateHeight=2848;f.imageX=1708;f.imageY=1138;f.zoomRect={1922,1281,428,286};
    const auto crop=m.previewRegion(f);EXPECT_DOUBLE_EQ(crop.x,.25);EXPECT_DOUBLE_EQ(crop.y,.25);EXPECT_DOUBLE_EQ(crop.width,.5);
    EXPECT_EQ(m.info().zoomRatios,(std::vector<int>{1,5,10}));EXPECT_EQ(m.info().focusSteps,(std::vector<int>{1,2,3}));EXPECT_EQ(m.info().rawFormat,"CR2");
}
