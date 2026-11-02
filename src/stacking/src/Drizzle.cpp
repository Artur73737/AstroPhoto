#include <astro/StackMath.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::stacking {
namespace {
// Clip a projected drop to an output pixel; the contribution is its exact area.
double area(std::vector<Point> p,double x,double y){
    auto clip=[&](bool axis,double edge,bool minimum){std::vector<Point> q;if(p.empty())return;auto at=[&](Point v){return axis?v.x:v.y;};auto prev=p.back();bool pi=minimum?at(prev)>=edge:at(prev)<=edge;for(auto v:p){bool vi=minimum?at(v)>=edge:at(v)<=edge;if(pi!=vi){double t=(edge-at(prev))/(at(v)-at(prev));q.push_back({prev.x+t*(v.x-prev.x),prev.y+t*(v.y-prev.y)});}if(vi)q.push_back(v);prev=v;pi=vi;}p=std::move(q);};
    clip(true,x,true);clip(true,x+1,false);clip(false,y,true);clip(false,y+1,false);double a=0;for(std::size_t i=0;i<p.size();++i){auto q=p[(i+1)%p.size()];a+=p[i].x*q.y-p[i].y*q.x;}return std::abs(a)*.5;
}
}
DrizzleFrame drizzle(const LinearImage& input,const Transform& t,double scale,double pixfrac){
    if(!plausible(t)||input.width<8||input.height<8||input.rgb.size()!=std::size_t(input.width)*input.height*3||!std::isfinite(scale)||scale<1||scale>3||!std::isfinite(pixfrac)||pixfrac<.1||pixfrac>1)throw std::invalid_argument("Invalid drizzle input");
    DrizzleFrame out;out.image.width=int(std::ceil(input.width*scale));out.image.height=int(std::ceil(input.height*scale));out.image.previewGains=input.previewGains;out.image.whiteBalance=input.whiteBalance;
    const auto n=std::size_t(out.image.width)*out.image.height;out.weights.resize(n);out.image.rgb.resize(n*3);
    const double det=t.a*t.d-t.b*t.c;auto project=[&](double x,double y){return Point{((t.d*(x-t.tx)-t.b*(y-t.ty))/det+.5)*scale,((-t.c*(x-t.tx)+t.a*(y-t.ty))/det+.5)*scale};};
    for(int y=0;y<input.height;++y)for(int x=0;x<input.width;++x){double half=pixfrac*.5;std::vector<Point> poly{project(x-half,y-half),project(x+half,y-half),project(x+half,y+half),project(x-half,y+half)};double minx=poly[0].x,maxx=minx,miny=poly[0].y,maxy=miny;for(auto p:poly){minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);miny=std::min(miny,p.y);maxy=std::max(maxy,p.y);}const auto source=(std::size_t(y)*input.width+x)*3;
        for(int yy=std::max(0,int(std::floor(miny)));yy<std::min(out.image.height,int(std::ceil(maxy)));++yy)for(int xx=std::max(0,int(std::floor(minx)));xx<std::min(out.image.width,int(std::ceil(maxx)));++xx){double weight=area(poly,xx,yy);if(weight<=1e-12)continue;auto p=std::size_t(yy)*out.image.width+xx;out.weights[p]+=float(weight);for(int c=0;c<3;++c)out.image.rgb[p*3+c]+=float(input.rgb[source+c]*weight);}}
    for(std::size_t p=0;p<n;++p)if(out.weights[p]>0)for(int c=0;c<3;++c)out.image.rgb[p*3+c]/=out.weights[p];return out;
}
}
