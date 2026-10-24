#include "StarAnalysis.hpp"
#include <astro/HocusFocusMath.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
namespace astro {
namespace {
double median(std::vector<double> values){auto it=values.begin()+values.size()/2;std::nth_element(values.begin(),it,values.end());return *it;}
}
static StarField measureNativeStars(const PreviewPixels& p,double minimumSnr,std::span<const StarSample> tracked={},double matchRadius=8){
    if(p.width<=0||p.height<=0||p.rgba.size()!=static_cast<std::size_t>(p.width)*p.height*4||!std::isfinite(minimumSnr)||minimumSnr<4)throw std::invalid_argument("Invalid stellar analysis input");
    StarField out;out.width=p.width;out.height=p.height;const int w=p.width,h=p.height;
    if(w<35||h<35)return out;
    const auto count=static_cast<std::size_t>(w)*h,stride=static_cast<std::size_t>(w)+1;
    std::vector<float> luma(count),smooth(count);std::vector<double> integral(stride*(h+1)),square(integral.size());
    std::array<unsigned,256> histogram{};std::uint64_t clipped{};
    for(int y=0;y<h;++y){double sum{},sum2{};for(int x=0;x<w;++x){const auto i=static_cast<std::size_t>(y)*w+x;
        const float v=.2126f*p.rgba[i*4]+.7152f*p.rgba[i*4+1]+.0722f*p.rgba[i*4+2];luma[i]=v;++histogram[std::clamp(static_cast<int>(v),0,255)];
        if(std::max({p.rgba[i*4],p.rgba[i*4+1],p.rgba[i*4+2]})>=250)++clipped;
        sum+=v;sum2+=v*v;integral[(y+1)*stride+x+1]=integral[y*stride+x+1]+sum;square[(y+1)*stride+x+1]=square[y*stride+x+1]+sum2;
    }}
    std::uint64_t cumulative{};for(int i=0;i<256;++i){cumulative+=histogram[i];if(cumulative>=count/2){out.background=i;break;}}
    out.clippedFraction=static_cast<double>(clipped)/count;
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){const auto i=static_cast<std::size_t>(y)*w+x;smooth[i]=(luma[i-w-1]+2*luma[i-w]+luma[i-w+1]+2*luma[i-1]+4*luma[i]+2*luma[i+1]+luma[i+w-1]+2*luma[i+w]+luma[i+w+1])/16;}
    auto box=[&](const auto& t,int x,int y,int r){return t[(y+r+1)*stride+x+r+1]-t[(y-r)*stride+x+r+1]-t[(y+r+1)*stride+x-r]+t[(y-r)*stride+x-r];};
    struct Candidate{int x,y;float peak;bool tracked{};};std::vector<Candidate> candidates;
    for(int y=16;y<h-16;++y)for(int x=16;x<w-16;++x){const auto i=static_cast<std::size_t>(y)*w+x;const auto peak=smooth[i];
        bool maximum=true;for(int dy=-1;dy<=1&&maximum;++dy)for(int dx=-1;dx<=1;++dx){if(!dx&&!dy)continue;auto j=static_cast<std::size_t>(y+dy)*w+x+dx;if(smooth[j]>peak||(j<i&&smooth[j]==peak)){maximum=false;break;}}
        if(!maximum)continue;
        const double background=(box(integral,x,y,14)-box(integral,x,y,10))/400;
        const double noise=std::sqrt(std::max(0.,(box(square,x,y,14)-box(square,x,y,10))/400-background*background));
        if(peak-background<std::max(3.,minimumSnr*noise))continue;
        candidates.push_back({x,y,peak});
    }
    // Previously identified stars remain measurable as they become smaller than
    // the discovery morphology limits. Unknown single-pixel sources still fail
    // discovery; known sources still require SNR, flux, clipping and time checks.
    const int searchRadius=static_cast<int>(std::clamp(matchRadius,2.,30.));
    for(const auto& target:tracked){if(!std::isfinite(target.x)||!std::isfinite(target.y)||target.x<0||target.x>=w||target.y<0||target.y>=h)continue;
        int bx=-1,by=-1;float peak=-1;
        for(int y=std::max(16,static_cast<int>(target.y)-searchRadius);y<=std::min(h-17,static_cast<int>(target.y)+searchRadius);++y)
        for(int x=std::max(16,static_cast<int>(target.x)-searchRadius);x<=std::min(w-17,static_cast<int>(target.x)+searchRadius);++x){if(std::hypot(x-target.x,y-target.y)>matchRadius)continue;if(smooth[y*w+x]>peak){peak=smooth[y*w+x];bx=x;by=y;}}
        if(bx<0)continue;auto existing=std::find_if(candidates.begin(),candidates.end(),[&](auto c){return c.x==bx&&c.y==by;});
        if(existing!=candidates.end())existing->tracked=true;else candidates.push_back({bx,by,peak,true});
    }
    std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.tracked!=b.tracked?a.tracked>b.tracked:a.peak>b.peak;});
    // Bound worst-case processing on textured terrestrial images.
    if(candidates.size()>256)candidates.resize(256);
    std::vector<double> noises;
    for(auto candidate:candidates){const int x=candidate.x,y=candidate.y;
        if(std::any_of(out.stars.begin(),out.stars.end(),[&](auto s){return std::hypot(s.x-x,s.y-y)<(candidate.tracked?12:22);})){++out.rejected;continue;}
        std::vector<double> sky;std::vector<hocus::BackgroundPixel> annulus;
        for(int dy=-14;dy<=14;++dy)for(int dx=-14;dx<=14;++dx)if(dx*dx+dy*dy>=121&&dx*dx+dy*dy<=196){double v=luma[(y+dy)*w+x+dx];sky.push_back(v);annulus.push_back({double(dx),double(dy),v});}
        auto plane=hocus::fitBackground(annulus);
        if(!plane.valid)plane={true,median(sky),0,0,0};
        const double background=plane.offset;sky.clear();for(auto p:annulus)sky.push_back(std::abs(p.value-plane.at(p.x,p.y)));
        const double noise=std::max(.5,1.4826*median(sky));noises.push_back(noise);
        // Fit the photometric aperture to the source. A fixed 10 px aperture
        // mostly measures background for small EVF stars and used to reject
        // visible stars through an excessive integrated-noise threshold.
        int aperture=10,quiet=0;
        for(int r=2;r<=10;++r){double shell=0;int pixels=0;
            for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){const int rr=dx*dx+dy*dy;if(rr>(r-1)*(r-1)&&rr<=r*r){shell+=luma[(y+dy)*w+x+dx]-plane.at(dx,dy);++pixels;}}
            if(shell/std::max(1,pixels)<std::max(noise*.75,(candidate.peak-background)*.08))++quiet;else quiet=0;
            if(quiet>=2){aperture=std::max(4,r);break;}
        }
        double positive{},sx{},sy{},peak{},mass{};int support{},clippedCore{};bool blend=false;
        int aperturePixels=0;
        for(int dy=-aperture;dy<=aperture;++dy)for(int dx=-aperture;dx<=aperture;++dx){if(dx*dx+dy*dy>aperture*aperture)continue;++aperturePixels;const auto i=static_cast<std::size_t>(y+dy)*w+x+dx;
            const double v=luma[i]-plane.at(dx,dy);mass+=v;peak=std::max(peak,v);const double weight=std::max(0.,v-2*noise);positive+=weight;sx+=dx*weight;sy+=dy*weight;
            if(v>std::max(3*noise,(candidate.peak-background)*.2))++support;
            if(dx*dx+dy*dy<=25&&std::max({p.rgba[i*4],p.rgba[i*4+1],p.rgba[i*4+2]})>=250)++clippedCore;
            if(dx*dx+dy*dy>=9&&smooth[i]-plane.at(dx,dy)>(candidate.peak-background)*.65&&smooth[i]>smooth[i-1]&&smooth[i]>smooth[i+1]&&smooth[i]>smooth[i-w]&&smooth[i]>smooth[i+w])blend=true;
        }
        if(clippedCore){++out.saturated;continue;}
        if(blend||support<(candidate.tracked?1:4)||positive<=0||mass<minimumSnr*noise*std::sqrt(static_cast<double>(aperturePixels))||peak/noise<minimumSnr){++out.rejected;continue;}
        const double cx=sx/positive,cy=sy/positive;if(std::hypot(cx,cy)>2.5){++out.rejected;continue;}
        double xx{},yy{},xy{},outer{};std::vector<std::pair<double,double>> radial;radial.reserve(6000);double total{};
        for(int dy=-11;dy<=11;++dy)for(int dx=-11;dx<=11;++dx){
            const double v=luma[(y+dy)*w+x+dx]-plane.at(dx,dy),weight=std::max(0.,v-2*noise),rx=dx-cx,ry=dy-cy;
            if(rx*rx+ry*ry<=aperture*aperture){xx+=rx*rx*weight;yy+=ry*ry*weight;xy+=rx*ry*weight;if(rx*rx+ry*ry>=.64*aperture*aperture)outer+=v;}
            // Encircled, background-subtracted flux, including signed noise. A 4x4
            // area subdivision avoids integer-pixel quantization of the half-flux radius.
            for(int iy=0;iy<4;++iy)for(int ix=0;ix<4;++ix){double r=std::hypot(rx+(ix+.5)/4-.5,ry+(iy+.5)/4-.5);if(r<=aperture){radial.emplace_back(r,v/16);total+=v/16;}}
        }
        const double trace=(xx+yy)/positive,delta=std::sqrt((xx-yy)*(xx-yy)+4*xy*xy)/positive;
        const double minor=(trace-delta)*.5,major=(trace+delta)*.5;
        const bool undersampled=candidate.tracked&&trace<1.5;
        if((!undersampled&&(minor<.15||major>minor*(candidate.tracked?6.:2.5)))||outer/mass>.12||total<=0){++out.rejected;continue;}
        std::sort(radial.begin(),radial.end());double sum{},radius{};
        for(auto [r,flux]:radial){const double before=sum;sum+=flux;if(sum>=total*.5&&flux>0){radius=r;if(before<total*.5)break;}}
        const double hfd=2*radius;if(!std::isfinite(hfd)||hfd<(candidate.tracked?.5:1.2)||hfd>12){++out.rejected;continue;}
        out.stars.push_back({x+cx,y+cy,hfd,total,peak/noise,major>0?minor/major:1,peak+background,background});
    }
    if(!noises.empty())out.noise=median(noises);
    return out;
}
StarField measureStars(const PreviewPixels& p,double minimumSnr,std::span<const StarSample> tracked,double matchRadius){
    if(!std::isfinite(matchRadius)||matchRadius<2||matchRadius>30)throw std::invalid_argument("Invalid stellar matching radius");
    auto out=measureNativeStars(p,minimumSnr,tracked,matchRadius);out.acquisitionCandidates=static_cast<int>(out.stars.size());
    // Area binning exposes broad, defocused sources without interpreting image
    // edges as stars. Never mix these measurements into the native-resolution HFD.
    if(out.stars.size()<20)for(int scale:{2,4}){
        PreviewPixels small;small.width=p.width/scale;small.height=p.height/scale;
        if(small.width<35||small.height<35)continue;
        small.rgba.resize(static_cast<std::size_t>(small.width)*small.height*4,255);
        for(int y=0;y<small.height;++y)for(int x=0;x<small.width;++x)for(int channel=0;channel<3;++channel){
            unsigned sum{};for(int dy=0;dy<scale;++dy)for(int dx=0;dx<scale;++dx)sum+=p.rgba[(static_cast<std::size_t>(y*scale+dy)*p.width+x*scale+dx)*4+channel];
            small.rgba[(static_cast<std::size_t>(y)*small.width+x)*4+channel]=static_cast<std::uint8_t>(sum/(scale*scale));
        }
        auto broad=measureNativeStars(small,minimumSnr);
        out.acquisitionCandidates=std::max(out.acquisitionCandidates,static_cast<int>(broad.stars.size()));
    }
    return out;
}
}
