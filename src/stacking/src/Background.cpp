#include <astro/StackMath.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::stacking {
std::array<float,3> normalizeBackground(LinearImage& image,const std::array<float,3>* reference,bool removeGradient){
    const int w=image.width,h=image.height;if(w<8||h<8||image.rgb.size()!=std::size_t(w)*h*3)throw std::invalid_argument("Invalid background image");
    struct Tile{double x,y;std::array<double,3> v;};std::vector<Tile> tiles;
    for(int by=0;by<8;++by)for(int bx=0;bx<8;++bx){Tile t{(bx+.5)/8-.5,(by+.5)/8-.5,{}};
        for(int c=0;c<3;++c){std::vector<float> samples;const int x0=bx*w/8,x1=(bx+1)*w/8,y0=by*h/8,y1=(by+1)*h/8;
            for(int y=y0;y<y1;y+=std::max(1,(y1-y0)/32))for(int x=x0;x<x1;x+=std::max(1,(x1-x0)/32)){float v=image.rgb[(std::size_t(y)*w+x)*3+c];if(!std::isfinite(v))throw std::invalid_argument("Non-finite background sample");if(v<.98f)samples.push_back(v);}
            if(samples.empty())throw std::invalid_argument("No unsaturated background samples");auto q=samples.begin()+samples.size()/5;std::nth_element(samples.begin(),q,samples.end());t.v[c]=*q;
        }tiles.push_back(t);
    }
    std::array<float,3> background{};std::array<double,3> sx{},sy{};
    for(int c=0;c<3;++c){std::vector<double> levels;for(auto t:tiles)levels.push_back(t.v[c]);std::sort(levels.begin(),levels.end());background[c]=float((levels[31]+levels[32])/2);
        if(removeGradient){double intercept=background[c];for(int pass=0;pass<4;++pass){double a[3][4]{};std::vector<double> residual;for(auto t:tiles)residual.push_back(std::abs(t.v[c]-intercept-sx[c]*t.x-sy[c]*t.y));std::sort(residual.begin(),residual.end());double scale=std::max(1e-6,1.4826*residual[32]);for(auto t:tiles){double r=std::abs(t.v[c]-intercept-sx[c]*t.x-sy[c]*t.y);double weight=std::min(1.,1.5*scale/std::max(1e-12,r)),p[]{1,t.x,t.y};for(int i=0;i<3;++i){for(int j=0;j<3;++j)a[i][j]+=weight*p[i]*p[j];a[i][3]+=weight*p[i]*t.v[c];}}for(int k=0;k<3;++k){double v=a[k][k];if(std::abs(v)<1e-10)throw std::invalid_argument("Degenerate background fit");for(int j=k;j<4;++j)a[k][j]/=v;for(int r=0;r<3;++r)if(r!=k){double f=a[r][k];for(int j=k;j<4;++j)a[r][j]-=f*a[k][j];}}intercept=a[0][3];sx[c]=a[1][3];sy[c]=a[2][3];}}
    }
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c){double correction=sx[c]*((x+.5)/w-.5)+sy[c]*((y+.5)/h-.5);if(reference)correction+=background[c]-(*reference)[c];image.rgb[(std::size_t(y)*w+x)*3+c]-=float(correction);}
    return background;
}
}
