#include "StarAnalysis.hpp"
#include <iostream>
// Offline inspection of recorded frames with the production detector. No camera
// connection, motor movement, exposure changes, or generated image fixtures.
int main(int argc,char** argv){
    try{for(int i=1;i<argc;++i){auto f=astro::measureStars(astro::decodePreview(astro::pathFromUtf8(argv[i])));
        std::cout<<"FILE "<<argv[i]<<" stars "<<f.stars.size()<<" background "<<f.background<<'\n';
        for(auto s:f.stars)std::cout<<s.x<<','<<s.y<<','<<s.hfd<<','<<s.flux<<','<<s.snr<<'\n';}
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
