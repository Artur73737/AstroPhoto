#include <astro/StackMath.hpp>
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
TEST(StackRaw,DecodesFullSensorInsteadOfEmbeddedJpeg){const char* source=std::getenv("ASTRO_PREVIEW_CR2");if(!source)GTEST_SKIP()<<"Set ASTRO_PREVIEW_CR2 to real CR2";auto raw=decodeRaw(pathFromUtf8(source),2048);EXPECT_GE(raw.width,1000);EXPECT_GE(raw.height,1000);std::set<int> levels;for(std::size_t i=0;i<raw.rgb.size();i+=13)levels.insert(int(raw.rgb[i]*65535));EXPECT_GT(levels.size(),256u);EXPECT_EQ(raw.rgb.size(),std::size_t(raw.width)*raw.height*3);EXPECT_THROW(decodeRaw(pathFromUtf8(source),1),std::runtime_error);}
TEST(StackProcessor,RealRawFinalizesAndCreatesReport){
    const char* source=std::getenv("ASTRO_PREVIEW_CR2");if(!source)GTEST_SKIP();auto root=std::filesystem::temp_directory_path()/("astro-stack-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);
    auto processor=makeStackProcessor();StackOptions o;o.alignment=false;processor->begin(root,o,"{}");
    auto wait=[&]{StackProcessResult r;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(90);do{r=processor->poll();if(r.ready)break;std::this_thread::sleep_for(std::chrono::milliseconds(20));}while(std::chrono::steady_clock::now()<end);return r;};
    auto initialized=wait();ASSERT_TRUE(initialized.prepared);processor->submit({{pathFromUtf8(source)},.5,1});
    auto first=wait();ASSERT_TRUE(first.ready);ASSERT_TRUE(first.accepted)<<first.detail;ASSERT_NE(first.preview,nullptr);processor->finish("unit validation");auto final=wait();ASSERT_TRUE(final.finalized)<<final.detail;EXPECT_FALSE(final.fatal);EXPECT_TRUE(std::filesystem::exists(root/"stack-linear.fits"));EXPECT_TRUE(std::filesystem::exists(root/"stack.json"));processor.reset();std::filesystem::remove_all(root);
}
TEST(StackIntegration,InvalidSampleCannotPartiallyModifyTheStack){Accumulator sum;LinearImage a{8,8,std::vector<float>(8*8*3,.2f)};sum.add(a,{},{});a.rgb.back()=NAN;EXPECT_THROW(sum.add(a,{},{}),std::invalid_argument);EXPECT_EQ(sum.frames,1);for(auto count:sum.count)EXPECT_EQ(count,1);EXPECT_NEAR(sum.value(0,0),.2,1e-6);}
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
    o.bandingRadius=0;EXPECT_THROW(reduceHorizontalBanding(image,o),std::invalid_argument);
}
TEST(StackBanding,DoesNotFlattenSmoothVerticalGradients){
    LinearImage image{64,64,std::vector<float>(64*64*3)};for(int y=0;y<64;++y)for(int x=0;x<64;++x)for(int c=0;c<3;++c)image.rgb[(y*64+x)*3+c]=.03f+y*.002f;
    auto original=image.rgb;StackOptions o;o.horizontalBanding=true;reduceHorizontalBanding(image,o);for(std::size_t i=0;i<original.size();++i)EXPECT_NEAR(image.rgb[i],original[i],1e-7);
}
