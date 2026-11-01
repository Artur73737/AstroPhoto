// SPDX-License-Identifier: MPL-2.0
// C++ adaptation of NINA.Plugin.Livestack AlignmentGeometry and
// ImageTransformer2.Alignment: geometry, spatial coverage, similarity least squares.
// Original copyright N.I.N.A. contributors. See licenses/nina-livestack/NOTICE.md.
#include <astro/StackMath.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::stacking {
bool plausible(const Transform& m){
    for(double v:{m.a,m.b,m.tx,m.c,m.d,m.ty})if(!std::isfinite(v))return false;
    const double xx=m.a*m.a+m.c*m.c,yy=m.b*m.b+m.d*m.d,xy=m.a*m.b+m.c*m.d;
    const double disc=std::sqrt((xx-yy)*(xx-yy)+4*xy*xy);
    const double largest=std::sqrt(std::max(0.,(xx+yy+disc)/2)),smallest=std::sqrt(std::max(0.,(xx+yy-disc)/2));
    return m.a*m.d-m.b*m.c>0&&smallest>=.98-1e-9&&largest<=1.02+1e-9&&largest<=smallest*1.02+1e-9;
}
double overlap(const Transform& m,int w,int h){
    const double det=m.a*m.d-m.b*m.c;if(!plausible(m)||w<=0||h<=0||std::abs(det)<1e-12)return 0;
    std::vector<Point> p;for(Point q:std::vector<Point>{{-.5,-.5},{w-.5,-.5},{w-.5,h-.5},{-.5,h-.5}})p.push_back({(m.d*(q.x-m.tx)-m.b*(q.y-m.ty))/det,(-m.c*(q.x-m.tx)+m.a*(q.y-m.ty))/det});
    auto clip=[&](bool x,double boundary,bool minimum){std::vector<Point> result;if(p.empty())return;auto prev=p.back();auto coord=[&](Point q){return x?q.x:q.y;};bool prevInside=minimum?coord(prev)>=boundary:coord(prev)<=boundary;
        for(auto cur:p){bool inside=minimum?coord(cur)>=boundary:coord(cur)<=boundary;if(inside!=prevInside){double f=(boundary-coord(prev))/(coord(cur)-coord(prev));result.push_back({prev.x+f*(cur.x-prev.x),prev.y+f*(cur.y-prev.y)});}if(inside)result.push_back(cur);prev=cur;prevInside=inside;}p=std::move(result);};
    clip(true,-.5,true);clip(true,w-.5,false);clip(false,-.5,true);clip(false,h-.5,false);double area=0;
    for(std::size_t i=0;i<p.size();++i){const auto q=p[(i+1)%p.size()];area+=p[i].x*q.y-q.x*p[i].y;}return std::clamp(std::abs(area)/(2.*w*h),0.,1.);
}
bool coverage(const std::vector<Point>& p,int w,int h){
    if(p.size()<8||w<=0||h<=0)return false;double mx=0,my=0;for(auto q:p){if(!std::isfinite(q.x)||!std::isfinite(q.y))return false;mx+=q.x/w;my+=q.y/h;}mx/=p.size();my/=p.size();double xx=0,yy=0,xy=0;
    for(auto q:p){double x=q.x/w-mx,y=q.y/h-my;xx+=x*x;yy+=y*y;xy+=x*y;}return (xx+yy-std::sqrt((xx-yy)*(xx-yy)+4*xy*xy))/(2*p.size())>=.0025;
}
Transform fitSimilarity(const std::vector<std::pair<Point,Point>>& pairs){
    if(pairs.size()<2)throw std::invalid_argument("Insufficient correspondences");Point r{},s{};for(auto [p,q]:pairs){r.x+=p.x;r.y+=p.y;s.x+=q.x;s.y+=q.y;}r.x/=pairs.size();r.y/=pairs.size();s.x/=pairs.size();s.y/=pairs.size();double den=0,re=0,im=0;
    for(auto [p,q]:pairs){double x=p.x-r.x,y=p.y-r.y,u=q.x-s.x,v=q.y-s.y;den+=x*x+y*y;re+=x*u+y*v;im+=x*v-y*u;}if(den<=1e-6)throw std::invalid_argument("Insufficient separation for similarity fit");double a=re/den,c=im/den;return {a,-c,s.x-a*r.x+c*r.y,c,a,s.y-c*r.x-a*r.y};
}
}
