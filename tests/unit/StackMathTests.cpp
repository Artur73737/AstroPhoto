#include <astro/StackMath.hpp>
#include <astro/Calibration.hpp>
#include <astro/PlateSolve.hpp>
#include <cstring>
#include <astro/Core.hpp>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <fstream>
#include <cstdlib>
#include <bit>
#include <thread>
using namespace astro;using namespace astro::stacking;
namespace {
std::vector<Point> field(){std::mt19937 random(41);std::uniform_real_distribution<double> x(25,490),y(25,360);std::vector<Point> p;for(int n=0;n<65;++n)p.push_back({x(random),y(random),100.-n});return p;}
LinearImage image(const std::vector<Point>& stars,int w=512,int h=384){LinearImage i{w,h,std::vector<float>(w*h*3,.01f)};for(auto p:stars)for(int y=std::max(0,int(p.y)-7);y<std::min(h,int(p.y)+8);++y)for(int x=std::max(0,int(p.x)-7);x<std::min(w,int(p.x)+8);++x)for(int c=0;c<3;++c)i.rgb[(y*w+x)*3+c]+=.3f*float(std::exp(-((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y))/3.5));return i;}
}
TEST(StackAlignment,FindsSubpixelTranslationRotationDespiteOutliers){
    auto ref=field(),cur=ref;double angle=.027;Transform t{cos(angle),-sin(angle),17.3,sin(angle),cos(angle),-9.6};for(auto& p:cur)p=t.apply(p);cur.erase(cur.begin(),cur.begin()+7);cur.push_back({300,20});cur.push_back({230,300});
    auto result=align(ref,cur,512,384,{});ASSERT_TRUE(result.accepted)<<result.reason;EXPECT_GE(result.matches,50);EXPECT_LT(result.rms,.01);EXPECT_NEAR(result.transform.tx,t.tx,.01);EXPECT_NEAR(result.transform.ty,t.ty,.01);EXPECT_NEAR(result.transform.b,t.b,1e-5);
}
TEST(StackAlignment,RejectsEmptyClusteredReflectedAndUnrelatedFields){
    EXPECT_FALSE(align({},field(),512,384,{}).accepted);auto cluster=field();for(auto& p:cluster){p.x=p.x*.01+200;p.y=p.y*.01+100;}EXPECT_FALSE(align(cluster,cluster,512,384,{}).accepted);
    auto ref=field(),other=ref;for(auto& p:other)p.x=512-p.x;EXPECT_FALSE(align(ref,other,512,384,{}).accepted);
    std::mt19937 r(7);for(auto& p:other){p.x=r()%512;p.y=r()%384;}EXPECT_FALSE(align(ref,other,512,384,{}).accepted);
    EXPECT_FALSE(plausible({1.2,0,0,0,1.2,0}));EXPECT_DOUBLE_EQ(overlap({1,0,900,0,1,0},512,384),0);
}
TEST(StackDetection,RejectsHotPixelsAndDetectsUnsaturatedStars){auto img=image(field());img.rgb[(20*512+20)*3]=.9;img.rgb[(20*512+20)*3+1]=.9;img.rgb[(20*512+20)*3+2]=.9;auto stars=detectStars(img,5);EXPECT_GE(stars.size(),40);EXPECT_FALSE(std::any_of(stars.begin(),stars.end(),[](auto p){return std::hypot(p.x-20,p.y-20)<3;}));}
TEST(StackAlignment,DetectsRegistersAndIntegratesNoisyPixelImages){
    auto reference=field(),moved=reference;Transform t{1,0,6.3,0,1,-4.2};for(auto& p:moved)p=t.apply(p);
    auto first=image(reference),next=image(moved);std::mt19937 random(51);std::normal_distribution<float> noise(0,.001f);
    for(auto& v:first.rgb)v+=noise(random);for(auto& v:next.rgb)v+=noise(random);
    auto match=align(detectStars(first,5),detectStars(next,5),first.width,first.height,{});
    ASSERT_TRUE(match.accepted)<<match.reason;EXPECT_NEAR(match.transform.tx,6.3,.2);EXPECT_NEAR(match.transform.ty,-4.2,.2);
    Accumulator sum;sum.add(first,{},{});sum.add(next,match.transform,{});
    double registeredError=0,unalignedError=0;
    for(int y=15;y<360;++y)for(int x=15;x<490;++x){auto p=std::size_t(y)*512+x;registeredError+=std::pow(sum.value(p,0)-first.rgb[p*3],2);unalignedError+=std::pow((next.rgb[p*3]-first.rgb[p*3])*.5,2);}
    EXPECT_LT(registeredError,unalignedError*.1);EXPECT_EQ(sum.frames,2);
}
TEST(StackIntegration,CoverageDoesNotDarkenEdgesAndClipsLateTransient){
    StackOptions options;Accumulator sum;LinearImage a{16,16,std::vector<float>(16*16*3,.2f)};for(int n=0;n<6;++n)sum.add(a,{},options);
    a.rgb[(8*16+8)*3]=.9f;sum.add(a,{1,0,1,0,1,0},options);EXPECT_NEAR(sum.value(8*16+7,0),.2,1e-6);EXPECT_GT(sum.clipped,0);EXPECT_EQ(sum.count[15],6);EXPECT_NEAR(sum.value(15,0),.2,1e-6);
}
TEST(StackIntegration,PreservesLinearFloatPrecisionAndDisplayDoesNotModifyData){Accumulator sum;LinearImage a{8,8,std::vector<float>(8*8*3,.10001f)};StackOptions o;o.rejection=false;sum.add(a,{},o);std::fill(a.rgb.begin(),a.rgb.end(),.10003f);sum.add(a,{},o);EXPECT_NEAR(sum.value(0,0),.10002,1e-7);auto p=render(sum,{0,.1f,.2f},1);EXPECT_GT(p.rgba[0],200);EXPECT_NEAR(sum.value(0,0),.10002,1e-7);}
TEST(StackFits,UsesBigEndianFloatRgbAndCoverageExtension){
    Accumulator sum;sum.add({8,8,std::vector<float>(8*8*3,.125f)},{},{});auto path=std::filesystem::temp_directory_path()/"astro-stack-unit.fits";writeFits(path,sum,2.5);std::ifstream f(path,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(f),{}};EXPECT_EQ(bytes.size()%2880,0);EXPECT_EQ(bytes.substr(0,8),"SIMPLE  ");EXPECT_NE(bytes.find("COVERAGE"),std::string::npos);EXPECT_EQ(static_cast<unsigned char>(bytes[2880]),0x3e);EXPECT_EQ(static_cast<unsigned char>(bytes[2881]),0);EXPECT_NE(bytes.find("2.500000"),std::string::npos);f.close();std::filesystem::remove(path);
}
TEST(StackRaw,DecodesFullSensorInsteadOfEmbeddedJpeg){const char* source=std::getenv("ASTRO_PREVIEW_CR2");if(!source)GTEST_SKIP()<<"Set ASTRO_PREVIEW_CR2 to real CR2";auto raw=decodeRaw(pathFromUtf8(source),2048);EXPECT_GE(raw.width,1000);EXPECT_GE(raw.height,1000);std::set<int> levels;for(std::size_t i=0;i<raw.rgb.size();i+=13)levels.insert(int(raw.rgb[i]*65535));EXPECT_GT(levels.size(),256u);EXPECT_EQ(raw.rgb.size(),std::size_t(raw.width)*raw.height*3);
EXPECT_THROW(decodeRaw(pathFromUtf8(source),1),std::runtime_error);}
TEST(StackProcessor,RealRawFinalizesAndCreatesReport){
    const char* source=std::getenv("ASTRO_PREVIEW_CR2");if(!source)GTEST_SKIP();auto root=std::filesystem::temp_directory_path()/("astro-stack-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);
    auto processor=makeStackProcessor();StackOptions o;o.alignment=false;o.checkpointFrames=1;processor->begin(root,o,"{}");
    auto wait=[&]{StackProcessResult r;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(90);do{r=processor->poll();if(r.ready)break;std::this_thread::sleep_for(std::chrono::milliseconds(20));}while(std::chrono::steady_clock::now()<end);return r;};
    auto initialized=wait();ASSERT_TRUE(initialized.prepared);processor->submit({{pathFromUtf8(source)},.5,1});
    auto first=wait();ASSERT_TRUE(first.ready);ASSERT_TRUE(first.accepted)<<first.detail;ASSERT_NE(first.preview,nullptr);EXPECT_TRUE(std::filesystem::exists(root/"stack-checkpoint.fits"));processor->submit({{pathFromUtf8(source)},0,2});auto replay=wait();ASSERT_TRUE(replay.accepted)<<replay.detail;EXPECT_GT(replay.exposureSeconds,0);processor->finish("unit validation");auto final=wait();ASSERT_TRUE(final.finalized)<<final.detail;EXPECT_FALSE(final.fatal);EXPECT_TRUE(std::filesystem::exists(root/"stack-linear.fits"));EXPECT_TRUE(std::filesystem::exists(root/"stack.json"));processor.reset();std::filesystem::remove_all(root);
}
TEST(StackIntegration,InvalidSampleCannotPartiallyModifyTheStack){Accumulator sum;LinearImage a{8,8,std::vector<float>(8*8*3,.2f)};sum.add(a,{},{});a.rgb.back()=NAN;
EXPECT_THROW(sum.add(a,{},{}),std::invalid_argument);EXPECT_EQ(sum.frames,1);for(auto count:sum.count)EXPECT_EQ(count,1);EXPECT_NEAR(sum.value(0,0),.2,1e-6);}
TEST(StackPreview,WhiteBalanceAndStretchNeverChangeSavedLinearSamples){Accumulator sum;LinearImage a{8,8,std::vector<float>(8*8*3,.1f)};a.previewGains={2,1,3};sum.add(a,{},{});auto p=render(sum,{0,.5f,1},7);EXPECT_EQ(p.rgba[0],51);EXPECT_EQ(p.rgba[1],25);EXPECT_EQ(p.rgba[2],76);EXPECT_NEAR(sum.value(0,0),.1,1e-6);EXPECT_EQ(p.revision,7);}
TEST(StackCosmetics,CorrectsIsolatedCfaImpulseWithoutErasingSmallStars){
    std::vector<std::uint16_t> raw(32*32,2048);raw[8*32+8]=16000;
    // A small optical PSF has support on neighbouring CFA pixels, unlike a hot pixel.
    raw[20*32+20]=12000;raw[20*32+19]=3500;raw[21*32+20]=4000;
    EXPECT_EQ(correctIsolatedBayerPixels(raw,32,32,32,2048,16383),1);
    EXPECT_EQ(raw[8*32+8],2048);EXPECT_EQ(raw[20*32+20],12000);EXPECT_EQ(raw[20*32+19],3500);
}
TEST(StackCosmetics,PreservesEdgesGradientsAndRowPadding){
    std::vector<std::uint16_t> raw(40*32,65535);for(int y=0;y<32;++y)for(int x=0;x<32;++x)raw[y*40+x]=2048+x*30+(x>16?3000:0);
    auto original=raw;EXPECT_EQ(correctIsolatedBayerPixels(raw,32,32,40,2048,16383),0);EXPECT_EQ(raw,original);

EXPECT_THROW(correctIsolatedBayerPixels(raw,40,40,40,2048,16383),std::invalid_argument);
}
TEST(StackRaw,FreezesFirstRawWhiteBalanceAndExportsColourPreview){
    const char* source=std::getenv("ASTRO_PREVIEW_CR2");if(!source)GTEST_SKIP();auto raw=decodeRaw(pathFromUtf8(source),2048);
    EXPECT_GT(raw.whiteBalance[0],0);EXPECT_GT(raw.whiteBalance[1],0);EXPECT_GT(raw.whiteBalance[2],0);
    auto again=decodeRaw(pathFromUtf8(source),2048,raw.whiteBalance);EXPECT_EQ(raw.rgb,again.rgb);
    Accumulator sum;sum.add(raw,{},{});auto preview=render(sum,{0,.25f,.5f},1);
    if(const char* target=std::getenv("ASTRO_STACK_PREVIEW_PPM")){std::ofstream out(pathFromUtf8(target),std::ios::binary);out<<"P6\n"<<preview.width<<' '<<preview.height<<"\n255\n";for(std::size_t i=0;i<preview.rgba.size();i+=4)out.write(reinterpret_cast<const char*>(preview.rgba.data()+i),3);}
    std::cout<<"RAW dimensions "<<raw.width<<'x'<<raw.height<<", WB "<<raw.whiteBalance[0]<<' '<<raw.whiteBalance[1]<<' '<<raw.whiteBalance[2]<<", corrected CFA pixels "<<raw.correctedPixels<<'\n';
}
TEST(StackBanding,ReducesRowOffsetsWhilePreservingGradientAndStarContrast){
    LinearImage image{128,128,std::vector<float>(128*128*3)};for(int y=0;y<128;++y)for(int x=0;x<128;++x)for(int c=0;c<3;++c)image.rgb[(y*128+x)*3+c]=.1f+y*.0002f+(y%2?.006f:-.006f);
    image.rgb[(60*128+64)*3]+=.3f;const float contrast=image.rgb[(60*128+64)*3]-image.rgb[(60*128+10)*3];
    StackOptions o;o.horizontalBanding=true;auto result=reduceHorizontalBanding(image,o);EXPECT_GT(result.rms,.004);EXPECT_EQ(result.limitedRows,0);
    for(int y=35;y<90;++y)EXPECT_NEAR(image.rgb[(y*128+10)*3],.1+y*.0002,.0006);
    EXPECT_NEAR(image.rgb[(60*128+64)*3]-image.rgb[(60*128+10)*3],contrast,1e-6);
}
TEST(StackBanding,DisabledIsExactAndStrengthLimitProtectsData){
    LinearImage image{64,64,std::vector<float>(64*64*3,.1f)};for(int x=0;x<64;++x)for(int c=0;c<3;++c)image.rgb[(32*64+x)*3+c]=.3f;
    auto original=image.rgb;EXPECT_EQ(reduceHorizontalBanding(image,{}).rms,0);EXPECT_EQ(original,image.rgb);
    StackOptions o;o.horizontalBanding=true;o.bandingMaxOffset=.01;o.bandingStrength=.5;auto result=reduceHorizontalBanding(image,o);EXPECT_EQ(result.limitedRows,1);EXPECT_NEAR(image.rgb[32*64*3],.295,1e-6);
    o.bandingRadius=0;
EXPECT_THROW(reduceHorizontalBanding(image,o),std::invalid_argument);
}
TEST(StackBanding,DoesNotFlattenSmoothVerticalGradients){
    LinearImage image{64,64,std::vector<float>(64*64*3)};for(int y=0;y<64;++y)for(int x=0;x<64;++x)for(int c=0;c<3;++c)image.rgb[(y*64+x)*3+c]=.03f+y*.002f;
    auto original=image.rgb;StackOptions o;o.horizontalBanding=true;reduceHorizontalBanding(image,o);for(std::size_t i=0;i<original.size();++i)EXPECT_NEAR(image.rgb[i],original[i],1e-7);
}
namespace {
SensorFrame sensor(float level){SensorFrame f;f.width=f.height=16;f.iso=800;f.exposure=1;f.model="test sensor";f.cfa={0,1,1,2};f.wb={1,1,1,1};f.matrix={{{1,0,0},{0,1,0},{0,0,1}}};f.pixels.assign(256,level);return f;}
}
TEST(StackCalibration,SubtractsMatchedDarkWithoutDoubleSubtractingBias){auto light=sensor(.3f),bias=sensor(.01f),dark=sensor(.03f);calibrateSensor(light,&bias,&dark,nullptr,nullptr);for(float v:light.pixels)EXPECT_NEAR(v,.27,1e-6);}
TEST(StackCalibration,FlatCorrectsVignettingPerCfaAndRetainsNegativeValues){auto light=sensor(.2f),bias=sensor(.01f),flat=sensor(.41f);for(int y=0;y<16;++y)for(int x=0;x<4;++x){light.pixels[y*16+x]=.105f;flat.pixels[y*16+x]=.21f;}calibrateSensor(light,&bias,nullptr,&flat,nullptr);for(float v:light.pixels)EXPECT_NEAR(v,.19,1e-6);auto negative=sensor(0);calibrateSensor(negative,&bias,nullptr,nullptr,nullptr);auto rgb=demosaicCalibrated(negative,{});for(float v:rgb.rgb)EXPECT_NEAR(v,-.01,1e-6);}
TEST(StackCalibration,DarkFlatIsNotSubtractedTwiceAndInvalidFlatDoesNotMutateLight){auto light=sensor(.21f),bias=sensor(.01f),flat=sensor(.4f),df=sensor(.04f);calibrateSensor(light,&bias,nullptr,&flat,&df);EXPECT_NEAR(light.pixels[0],.2,1e-6);flat.pixels.back()=0;auto before=light.pixels;
EXPECT_THROW(calibrateSensor(light,&bias,nullptr,&flat,&df),std::invalid_argument);EXPECT_EQ(light.pixels,before);}
TEST(StackCalibration,RejectsWrongIsoGeometryExposureAndSaturation){auto a=sensor(.2f),b=sensor(.02f);b.iso=1600;
EXPECT_THROW(validateSensorMatch(a,b,false),std::invalid_argument);b=sensor(.02f);b.exposure=2;
EXPECT_THROW(validateSensorMatch(a,b,true),std::invalid_argument);
EXPECT_NO_THROW(validateSensorMatch(a,b,false));b.cfa={2,1,1,0};
EXPECT_THROW(validateSensorMatch(a,b,false),std::invalid_argument);auto flat=sensor(1),bias=sensor(.01f);
EXPECT_THROW(calibrateSensor(a,&bias,nullptr,&flat,nullptr),std::invalid_argument);}
TEST(StackCalibration,MasterMeanIsDeterministicAndPreservesSignedNoise){auto a=sensor(-.001f),b=sensor(.003f);auto master=meanMaster({a,b});for(auto v:master.pixels)EXPECT_NEAR(v,.001,1e-7);
EXPECT_THROW(meanMaster({}),std::invalid_argument);}
TEST(StackCalibration,ReadsRealSensorCfaAndBuildsFiniteSignedRgb){const char* source=std::getenv("ASTRO_PREVIEW_CR2");if(!source)GTEST_SKIP();auto raw=readSensor(pathFromUtf8(source),2048);ASSERT_GT(raw.width,1000);ASSERT_GT(raw.iso,0);auto rgb=demosaicCalibrated(raw,{});EXPECT_EQ(rgb.rgb.size(),raw.pixels.size()*3);double sum=0;for(float v:rgb.rgb){ASSERT_TRUE(std::isfinite(v));sum+=std::abs(v);}EXPECT_GT(sum,1);}
TEST(StackBackground,MatchesAdditiveOffsetsAndPreservesStarContrast){auto a=image(field()),b=a;auto target=normalizeBackground(a,nullptr,false);for(auto& v:b.rgb)v+=.07f;normalizeBackground(b,&target,false);for(std::size_t i=0;i<a.rgb.size();++i)EXPECT_NEAR(a.rgb[i],b.rgb[i],1e-6);}
TEST(StackBackground,RemovesPlaneWithoutErasingCompactSources){auto a=image(field());for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x)for(int c=0;c<3;++c)a.rgb[(y*a.width+x)*3+c]+=.1f*x/a.width+.04f*y/a.height;normalizeBackground(a,nullptr,true);EXPECT_NEAR(a.rgb[(50*a.width+10)*3],a.rgb[(250*a.width+490)*3],.003);EXPECT_GT(*std::max_element(a.rgb.begin(),a.rgb.end()),.2);}
TEST(StackAlignment,AffineRefinementHandlesSmallShearAndRejectsDegeneracy){auto ref=field(),cur=ref;Transform t{1.003,.004,2.1,-.002,.998,-1.3};for(auto& p:cur)p=t.apply(p);StackOptions o;o.affineAlignment=true;auto result=align(ref,cur,512,384,o);ASSERT_TRUE(result.accepted)<<result.reason;EXPECT_LT(result.rms,.01);EXPECT_NEAR(result.transform.b,t.b,1e-6);std::vector<std::pair<Point,Point>> line(8,{{1,1},{2,2}});
EXPECT_THROW(fitAffine(line),std::invalid_argument);}
TEST(StackDrizzle,AreaDropsPreserveSurfaceBrightnessAndCoverageAtAllScales){
    LinearImage image{16,16,std::vector<float>(16*16*3,.125f)};
    for(double scale:{1.5,2.,3.}){auto d=drizzle(image,{},scale,1);EXPECT_EQ(d.image.width,int(16*scale));double area=0;for(std::size_t p=0;p<d.weights.size();++p){area+=d.weights[p];ASSERT_GT(d.weights[p],0);for(int c=0;c<3;++c)EXPECT_NEAR(d.image.rgb[p*3+c],.125,1e-6);}EXPECT_NEAR(area,256*scale*scale,.001);}
}
TEST(StackDrizzle,DitherFillsDropGapsAndCountsEachFrameOnce){
    LinearImage image{8,8,std::vector<float>(8*8*3,.2f)};StackOptions o;o.drizzleScale=3;o.drizzlePixfrac=.2;o.rejection=false;Accumulator a;a.add(image,{},o);auto first=std::count(a.count.begin(),a.count.end(),0u);a.add(image,{1,0,1./3,0,1,1./3},o);EXPECT_LT(std::count(a.count.begin(),a.count.end(),0u),first);for(auto n:a.count)EXPECT_LE(n,2u);for(std::size_t p=0;p<a.count.size();++p)if(a.count[p])EXPECT_NEAR(a.value(p,0),.2,1e-6);
}
TEST(StackExport,FloatTiffRetainsNegativeLinearSamples){
    Accumulator a;a.add({8,8,std::vector<float>(8*8*3,-.125f)},{},{});auto file=std::filesystem::temp_directory_path()/"astro-float-tiff-test.tiff";writeTiff(file,a);std::ifstream in(file,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(in),{}};ASSERT_EQ(bytes.substr(0,2),"II");ASSERT_EQ(bytes.size(),158+8*8*12);float value{};std::memcpy(&value,bytes.data()+158,4);EXPECT_FLOAT_EQ(value,-.125f);in.close();std::filesystem::remove(file);
}
TEST(StackRejection,WinsorLimitsTransientWithoutDroppingCoverage){Accumulator a;StackOptions o;o.winsorizedRejection=true;LinearImage image{8,8,std::vector<float>(8*8*3,.2f)};for(int i=0;i<6;++i)a.add(image,{},o);image.rgb[0]=.9f;a.add(image,{},o);EXPECT_EQ(a.count[0],7);EXPECT_GT(a.clipped,0);EXPECT_NEAR(a.value(0,0),.2,1e-4);}
TEST(StackAstrometry,RequiresCompleteFiniteWcsAndExplicitSolverSuccess){
    std::string ini="PLTSOLVD=T\nCRPIX1=100\nCRPIX2=100\nCRVAL1=154.63\nCRVAL2=22.04\nCD1_1=-0.0007\nCD1_2=0\nCD2_1=0\nCD2_2=0.0007\n";
    EXPECT_TRUE(parsePlateSolution(ini).solved);EXPECT_FALSE(parsePlateSolution("PLTSOLVD=T\n").solved);EXPECT_FALSE(parsePlateSolution("PLTSOLVD=F\n"+ini.substr(12)).solved);
    auto pos=ini.find("154.63");ini.replace(pos,6,"NaN");EXPECT_FALSE(parsePlateSolution(ini).solved);
}
TEST(StackMaster,SensorMeanAndFitsPreserveCfaAndFrameCount){
    auto a=sensor(.1f),b=sensor(.3f);SensorFrame mean;addToMaster(mean,a,1);addToMaster(mean,b,2);EXPECT_NEAR(mean.pixels[0],.2,1e-6);
    auto path=std::filesystem::temp_directory_path()/"astro-sensor-master-test.fits";writeSensorMaster(path,mean,2,"Bias");std::ifstream input(path,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(input),{}};EXPECT_NE(bytes.find("BAYERPAT"),std::string::npos);EXPECT_NE(bytes.find("'RGGB'"),std::string::npos);EXPECT_NE(bytes.find("NCOMBINE= 2"),std::string::npos);input.close();std::filesystem::remove(path);
}
TEST(StackCalibration,DarkMapRepairsHotColdAndColumnDefectsWithoutSelectingScienceStars){
    auto dark=sensor(.01f);dark.width=dark.height=32;dark.pixels.assign(32*32,.01f);for(int y=0;y<32;++y)dark.pixels[y*32+12]=.2f;dark.pixels[8*32+8]=-.05f;
    auto mask=calibrationDefects(dark);EXPECT_TRUE(mask[8*32+8]);EXPECT_TRUE(mask[16*32+12]);auto science=dark;std::fill(science.pixels.begin(),science.pixels.end(),.1f);science.pixels[16*32+12]=.9f;science.pixels[8*32+8]=-.3f;science.pixels[20*32+20]=.8f;
    EXPECT_GT(repairCalibrationDefects(science,mask),0);EXPECT_NEAR(science.pixels[16*32+12],.1,1e-6);EXPECT_NEAR(science.pixels[8*32+8],.1,1e-6);EXPECT_NEAR(science.pixels[20*32+20],.8,1e-6);
}
TEST(StackProcessor,MissingCalibrationFolderRetainsOriginalFailureAndWritesErrorReport){
    auto root=std::filesystem::temp_directory_path()/("astro-cal-error-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);auto processor=makeStackProcessor();StackOptions options;options.biasDirectory=(root/"missing-bias").string();processor->begin(root,options,"{}");
    auto wait=[&]{StackProcessResult r;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);do{r=processor->poll();if(r.ready)return r;std::this_thread::sleep_for(std::chrono::milliseconds(10));}while(std::chrono::steady_clock::now()<end);return r;};
    auto failure=wait();ASSERT_TRUE(failure.fatal);processor->finish(failure.detail,true);auto final=wait();ASSERT_TRUE(final.finalized);EXPECT_TRUE(final.fatal);EXPECT_EQ(final.detail,failure.detail);EXPECT_TRUE(std::filesystem::exists(root/"stack.json"));EXPECT_FALSE(std::filesystem::exists(root/"stack-linear.fits"));processor.reset();std::filesystem::remove_all(root);
}
TEST(StackPreview,SixteenBitAndAsinhRetainPrecisionWithoutChangingLinearData){
    Accumulator a;LinearImage image{8,8,std::vector<float>(8*8*3,.10001f)};a.add(image,{},{});auto mtf=render(a,{0,.25f,1,false},1),stretch=render(a,{0,.25f,1,true},2);EXPECT_EQ(stretch.rgba16.size(),stretch.rgba.size());EXPECT_EQ(stretch.rgba16[3],65535);EXPECT_NE(mtf.rgba16[0],stretch.rgba16[0]);EXPECT_NEAR(a.value(0,0),.10001f,1e-7);EXPECT_GT(stretch.rgba16[0],0);
}
TEST(StackExport,XisfStoresPlanarFloatSamplesAtDeclaredAttachmentOffset){
    Accumulator a;LinearImage image{8,8,std::vector<float>(8*8*3,.1f)};image.rgb[0]=-.125f;image.rgb[1]=.25f;image.rgb[2]=.5f;a.add(image,{},{});auto path=std::filesystem::temp_directory_path()/"astro-xisf-test.xisf";writeXisf(path,a,3.5);std::ifstream input(path,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(input),{}};ASSERT_EQ(bytes.substr(0,8),"XISF0100");ASSERT_EQ(bytes.size(),4096+8*8*12);std::uint32_t length{};std::memcpy(&length,bytes.data()+8,4);EXPECT_LT(length,4080);EXPECT_NE(bytes.substr(16,length).find("attachment:4096:768"),std::string::npos);for(auto [offset,expected]:std::vector<std::pair<int,float>>{{4096,-.125f},{4096+64*4,.25f},{4096+128*4,.5f}}){float actual{};std::memcpy(&actual,bytes.data()+offset,4);EXPECT_FLOAT_EQ(actual,expected);}input.close();std::filesystem::remove(path);
}
TEST(StackAstrometry,SolvesExistingSkyPhotographWithRealAstapAndCatalog){
    const auto* exe=std::getenv("ASTRO_ASTAP_EXE");const auto* source=std::getenv("ASTRO_ASTAP_SKY");const auto* database=std::getenv("ASTRO_ASTAP_DATABASE");if(!exe||!source||!database)GTEST_SKIP()<<"Opt-in real ASTAP/catalog/sky photograph";
    StackOptions options;options.solverExecutable=exe;options.solverDatabase=database;options.solverTimeoutSeconds=120;options.solverFov=72;if(auto fov=std::getenv("ASTRO_ASTAP_FOV"))options.solverFov=std::stod(fov);
    auto root=std::filesystem::temp_directory_path()/("astro-real-solve-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);auto image=root/(std::string("sky")+pathFromUtf8(source).extension().string());std::filesystem::copy_file(pathFromUtf8(source),image);
    auto result=solvePlate(image,options);std::cout<<"Actual ASTAP result: "<<result.detail<<"; diagnostics "<<root.string()<<'\n';EXPECT_TRUE(result.solved)<<result.detail;if(result.solved){EXPECT_EQ(result.wcs.size(),8);std::cout<<"RA "<<result.wcs.at("CRVAL1")<<" DEC "<<result.wcs.at("CRVAL2")<<'\n';}
}
TEST(StackAlignment,AffineAndQuadOptionsStillRejectUnrelatedFields){auto a=field(),b=field();std::mt19937 r(300);for(auto& p:b){p.x=20+r()%470;p.y=20+r()%340;}StackOptions o;o.affineAlignment=true;EXPECT_FALSE(align(a,b,512,384,o).accepted);}
