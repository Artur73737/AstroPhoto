/* Copyright (c) 2021-2026 George Hilios <ghilios+NINA@googlemail.com>
 * C++ adaptation for AstroPhoto, 2026.
 * This Source Code Form is subject to the Mozilla Public License, v. 2.0.
 * See licenses/hocus-focus/LICENSE.txt or https://mozilla.org/MPL/2.0/.
 * Upstream: ghilios/hocus-focus, bced3c30d8a86114e2578039149667242737b942.
 * Adapted from WeightRegularization.cs, HyperbolicFittingAlglib.cs,
 * AlglibHyperbolicFitting.cs and StarDetector.ComputeGradientContamination.
 */
#include <astro/HocusFocusMath.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
namespace astro::hocus {
namespace {
double median(std::vector<double> values){
    if(values.empty())return 0;
    std::sort(values.begin(),values.end());const auto n=values.size();
    return n%2?values[n/2]:(values[n/2-1]+values[n/2])*.5;
}
template<std::size_t N> bool solve(std::array<std::array<double,N>,N> a,std::array<double,N> b,std::array<double,N>& result){
    double scale=0;for(auto row:a)for(double v:row)scale=std::max(scale,std::abs(v));
    if(!std::isfinite(scale)||scale==0)return false;
    for(std::size_t col=0;col<N;++col){
        std::size_t pivot=col;for(std::size_t r=col+1;r<N;++r)if(std::abs(a[r][col])>std::abs(a[pivot][col]))pivot=r;
        if(std::abs(a[pivot][col])<=scale*1e-12)return false;
        std::swap(a[col],a[pivot]);std::swap(b[col],b[pivot]);
        for(std::size_t r=0;r<N;++r)if(r!=col){const double f=a[r][col]/a[col][col];for(std::size_t c=col;c<N;++c)a[r][c]-=f*a[col][c];b[r]-=f*b[col];}
    }
    for(std::size_t i=0;i<N;++i){result[i]=b[i]/a[i][i];if(!std::isfinite(result[i]))return false;}return true;
}
using Parameters=std::array<double,4>;
double model(const Parameters& p,double x){return p[2]/p[3]*std::hypot(x-p[0],p[3])+p[1];}
Parameters gradient(const Parameters& p,double x){
    const double u=x-p[0],r=std::hypot(u,p[3]);
    return {-p[2]*u/(p[3]*r),1,r/p[3],-p[2]*u*u/(p[3]*p[3]*r)};
}
double cost(const Parameters& p,const std::vector<Point>& points,const std::vector<double>& weights){
    double sum=0;for(std::size_t i=0;i<points.size();++i){double r=weights[i]*(model(p,points[i].x)-points[i].y);sum+=r*r;}return sum;
}
Parameters bounded(Parameters p,double minimumY){
    p[0]=std::clamp(p[0],-1.,1.);p[1]=std::clamp(p[1],-minimumY,minimumY);
    p[2]=std::clamp(p[2],.001,2*minimumY);p[3]=std::clamp(p[3],.001,4.);return p;
}
Parameters optimize(Parameters p,const std::vector<Point>& points,const std::vector<double>& weights,double minimumY){
    p=bounded(p,minimumY);double lambda=.001,error=cost(p,points,weights);
    for(int iteration=0;iteration<200;++iteration){
        std::array<std::array<double,4>,4> a{};Parameters b{},delta{};
        for(std::size_t i=0;i<points.size();++i){auto g=gradient(p,points[i].x);double w2=weights[i]*weights[i],r=model(p,points[i].x)-points[i].y;
            for(int row=0;row<4;++row){b[row]-=w2*g[row]*r;for(int col=0;col<4;++col)a[row][col]+=w2*g[row]*g[col];}}
        for(int row=0;row<4;++row)a[row][row]+=lambda*std::max(a[row][row],1e-6);
        if(!solve(a,b,delta))break;Parameters trial=p;for(int i=0;i<4;++i)trial[i]+=delta[i];trial=bounded(trial,minimumY);
        double next=cost(trial,points,weights);
        if(std::isfinite(next)&&next<error){double improvement=error-next;p=trial;error=next;lambda=std::max(1e-12,lambda*.3);if(improvement<1e-10*std::max(1.,error))break;}
        else{lambda*=10;if(lambda>1e12)break;}
    }
    return p;
}
Fit fitOne(const std::vector<Point>& raw){
    Fit result;if(raw.size()<5)return result;
    auto points=regularizeWeights(raw);std::sort(points.begin(),points.end(),[](auto a,auto b){return a.x<b.x;});
    for(std::size_t i=0;i<points.size();++i)if(!std::isfinite(points[i].x)||!std::isfinite(points[i].y)||points[i].y<=.1||(i&&points[i].x<=points[i-1].x))return result;
    const double center=(points.front().x+points.back().x)*.5,scale=(points.back().x-points.front().x)*.5;
    if(!std::isfinite(scale)||scale<=0)return result;
    const auto minimum=std::min_element(points.begin(),points.end(),[](auto a,auto b){return a.y<b.y;});
    double minY=minimum->y,maxY=0;const double seedX=(minimum->x-center)/scale;
    std::vector<double> weights;for(auto& p:points){p.x=(p.x-center)/scale;weights.push_back(1/p.sigma);maxY=std::max(maxY,p.y);}
    if(maxY-minY<1e-8*maxY)return result;
    Parameters best{};double bestCost=std::numeric_limits<double>::infinity();
    // Multiple bounded seeds avoid pinning the nonlinear solve to a noisy low point.
    for(double x:{seedX,0.,-.5,.5})for(double b:{.1,.5,1.,2.,4.}){
        auto candidate=optimize({x,0,minY,b},points,weights,minY);const double error=cost(candidate,points,weights);
        if(error<bestCost){bestCost=error;best=candidate;}
    }
    if(!std::isfinite(bestCost))return result;
    auto effective=weights;double previousSum=std::numeric_limits<double>::infinity();
    for(int iteration=0;iteration<10;++iteration){
        best=optimize(best,points,effective,minY);std::vector<double> residuals;double sum=0;
        for(auto p:points){double r=model(best,p.x)-p.y;residuals.push_back(r);sum+=std::abs(r);}
        const double location=median(residuals);std::vector<double> deviations;for(double r:residuals)deviations.push_back(std::abs(r-location));
        const double mad=1.483*median(deviations);if(mad<=1e-12)break;
        for(std::size_t i=0;i<points.size();++i){const double r=std::abs(residuals[i]-location);effective[i]=weights[i]*(r<=1.5*mad?1:1.5*mad/r);}
        if(std::abs(previousSum-sum)<=1e-6*std::max(1.,sum))break;previousSum=sum;
    }
    double sumWeights=0,mean=0;for(std::size_t i=0;i<points.size();++i){double w2=weights[i]*weights[i];sumWeights+=w2;mean+=w2*points[i].y;}mean/=sumWeights;
    double total=0;for(std::size_t i=0;i<points.size();++i)total+=weights[i]*weights[i]*std::pow(points[i].y-mean,2);
    const double rss=cost(best,points,weights);result.rSquared=total>0?1-rss/total:0;result.reducedChiSquared=rss/(points.size()-4);
    result.minimum=center+best[0]*scale;result.value=model(best,best[0]);
    result.parameters={result.minimum,best[1],best[2],best[3]*scale};
    std::array<std::array<double,4>,4> matrix{};for(std::size_t i=0;i<points.size();++i){auto g=gradient(best,points[i].x);for(int r=0;r<4;++r)for(int c=0;c<4;++c)matrix[r][c]+=weights[i]*weights[i]*g[r]*g[c];}
    Parameters covarianceColumn{};if(solve(matrix,Parameters{1,0,0,0},covarianceColumn)){
        double variance=covarianceColumn[0]*result.reducedChiSquared;
        if(variance>=0&&std::isfinite(variance)){double sigma=std::sqrt(variance)*scale;if(sigma<=2*scale)result.positionSigma=sigma;}
    }
    result.valid=std::isfinite(result.minimum)&&std::isfinite(result.value)&&result.value>0&&best[0]>-.999&&best[0]<.999;
    return result;
}
}
std::vector<Point> regularizeWeights(std::span<const Point> points){
    std::vector<Point> result(points.begin(),points.end());std::vector<double> sigmas;
    for(auto p:points)if(std::isfinite(p.sigma)&&p.sigma>0)sigmas.push_back(p.sigma);
    const double middle=sigmas.empty()?1:median(sigmas);
    for(auto& p:result)p.sigma=std::isfinite(p.sigma)&&p.sigma>0?std::max(p.sigma,.2*middle):middle;
    return result;
}
double evaluate(const Fit& fit,double x){return model(fit.parameters,x);}
Fit fitHyperbola(std::span<const Point> input,bool leaveOneOut){
    std::vector<Point> points(input.begin(),input.end());auto result=fitOne(points);
    if(!result.valid||!leaveOneOut||points.size()<6)return result;
    std::vector<double> minima;for(std::size_t skip=0;skip<points.size();++skip){auto subset=points;subset.erase(subset.begin()+skip);auto fit=fitOne(subset);if(fit.valid)minima.push_back(fit.minimum);}
    result.leaveOneOutSolved=static_cast<int>(minima.size());
    if(minima.size()>=2){double mean=std::accumulate(minima.begin(),minima.end(),0.)/minima.size(),sum=0;for(double x:minima)sum+=(x-mean)*(x-mean);result.leaveOneOutSigma=std::sqrt(sum/(minima.size()-1));}
    return result;
}
BackgroundPlane fitBackground(std::span<const BackgroundPixel> pixels){
    BackgroundPlane result;if(pixels.size()<12)return result;
    std::vector<double> weights(pixels.size(),1),deviations;
    std::array<double,3> p{};
    for(int iteration=0;iteration<4;++iteration){
        std::array<std::array<double,3>,3> a{};std::array<double,3> b{};
        for(std::size_t i=0;i<pixels.size();++i){const auto v=pixels[i];if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.value))return result;std::array<double,3> g{1,v.x,v.y};
            for(int r=0;r<3;++r){b[r]+=weights[i]*g[r]*v.value;for(int c=0;c<3;++c)a[r][c]+=weights[i]*g[r]*g[c];}}
        if(!solve(a,b,p))return result;
        deviations.clear();for(auto v:pixels)deviations.push_back(std::abs(v.value-(p[0]+p[1]*v.x+p[2]*v.y)));
        double sigma=1.4826*median(deviations);if(sigma<=1e-12)break;
        for(std::size_t i=0;i<pixels.size();++i){double z=deviations[i]/sigma;weights[i]=z<=1.345?1:1.345/z;}
    }
    deviations.clear();for(auto v:pixels)deviations.push_back(std::abs(v.value-(p[0]+p[1]*v.x+p[2]*v.y)));
    result={true,p[0],p[1],p[2],1.4826*median(deviations)};return result;
}
}
