#include <astro/PropertyLabels.hpp>
#include <gtest/gtest.h>
using namespace astro;
using edsdk::propertyLabel;
TEST(CanonLabels,ValuesReportedByThe1100D) {
    EXPECT_EQ(propertyLabel(Property::Iso,0x58),"400");
    EXPECT_EQ(propertyLabel(Property::Aperture,0x40),"f/11");
    EXPECT_EQ(propertyLabel(Property::Shutter,0x68),"1/60 s");
    EXPECT_EQ(propertyLabel(Property::ExposureMode,3),"M - Manuale");
    EXPECT_EQ(propertyLabel(Property::WhiteBalance,8),"Ombra");
    EXPECT_EQ(propertyLabel(Property::PictureStyle,0x84),"Neutro");
    EXPECT_EQ(propertyLabel(Property::ImageQuality,0x0064ff0f),"RAW");
}
TEST(CanonLabels,ExposureUnitsAndCanonRoundedSteps) {
    EXPECT_EQ(propertyLabel(Property::Shutter,0x0c),"Bulb");
    EXPECT_EQ(propertyLabel(Property::Shutter,0x10),"30 s");
    EXPECT_EQ(propertyLabel(Property::Shutter,0x2b),"3.2 s");
    EXPECT_EQ(propertyLabel(Property::Shutter,0x44),"0.3 s");
    EXPECT_EQ(propertyLabel(Property::Shutter,0x45),"0.3 s");
    EXPECT_EQ(propertyLabel(Property::Shutter,0x98),"1/4000 s");
    EXPECT_EQ(propertyLabel(Property::Aperture,0x14),"f/1.8");
    EXPECT_EQ(propertyLabel(Property::Aperture,0x15),"f/1.8");
    EXPECT_EQ(propertyLabel(Property::Iso,0),"Auto");
    EXPECT_EQ(propertyLabel(Property::ColorTemperature,5200),"5200 K");
}
TEST(CanonLabels,RawJpegAndUnknownValuesAreNotInvented) {
    EXPECT_EQ(propertyLabel(Property::ImageQuality,0x00640013),"RAW + JPEG L Fine");
    EXPECT_EQ(propertyLabel(Property::ImageQuality,0x0013ff0f),"JPEG L Fine");
    EXPECT_EQ(propertyLabel(Property::ImageQuality,0x0012ff0f),"JPEG L Normale");
    EXPECT_EQ(propertyLabel(Property::Aperture,0),"Non disponibile");
    EXPECT_EQ(propertyLabel(Property::Iso,0x12345678),"Non riconosciuto");
}
