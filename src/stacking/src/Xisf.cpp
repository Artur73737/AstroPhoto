#define NOMINMAX
#include <windows.h>
#include <astro/StackMath.hpp>
#include <bit>
#include <fstream>
#include <stdexcept>
namespace astro::stacking {
void writeXisf(const std::filesystem::path& path,const Accumulator& a,double seconds){
    if(!a.frames)throw std::invalid_argument("Cannot export empty XISF");
    const auto size=std::uint64_t(a.width)*a.height*12;
    const std::string xml="<?xml version=\"1.0\" encoding=\"UTF-8\"?><xisf version=\"1.0\" xmlns=\"http://www.pixinsight.com/xisf\"><Image id=\"integration\" geometry=\""+std::to_string(a.width)+":"+std::to_string(a.height)+":3\" sampleFormat=\"Float32\" colorSpace=\"RGB\" pixelStorage=\"Planar\" byteOrder=\"little\" location=\"attachment:4096:"+std::to_string(size)+"\"><FITSKeyword name=\"NCOMBINE\" value=\""+std::to_string(a.frames)+"\" comment=\"Accepted frames\"/><FITSKeyword name=\"EXPTIME\" value=\""+std::to_string(seconds)+"\" comment=\"Integrated exposure seconds\"/><FITSKeyword name=\"COLORSPC\" value=\"'LINEAR SRGB'\" comment=\"Unstretched linear samples\"/></Image></xisf>";
    if(xml.size()+16>4096)throw std::runtime_error("XISF header exceeds attachment offset");
    auto tmp=path;tmp+=".tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);auto word=[&](std::uint32_t v){for(int shift=0;shift<32;shift+=8)out.put(char(v>>shift));};
    out.write("XISF0100",8);word(std::uint32_t(xml.size()));word(0);out.write(xml.data(),xml.size());std::string pad(4096-16-xml.size(),0);out.write(pad.data(),pad.size());
    std::array<char,65536> buffer{};std::size_t used=0;for(int c=0;c<3;++c)for(std::size_t p=0;p<a.count.size();++p){auto bits=std::bit_cast<std::uint32_t>(a.value(p,c));for(int shift=0;shift<32;shift+=8)buffer[used++]=char(bits>>shift);if(used==buffer.size()){out.write(buffer.data(),used);used=0;}}
    if(used)out.write(buffer.data(),used);out.flush();if(!out)throw std::runtime_error("XISF write failed");out.close();if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit XISF");
}
}
