#define NOMINMAX
#include <windows.h>
#include <astro/StackMath.hpp>
#include <astro/Calibration.hpp>
#include <bit>
#include <fstream>
#include <stdexcept>
namespace astro::stacking {
void writeSensorMaster(const std::filesystem::path& path,const SensorFrame& f,int frames,const std::string& type){
    auto tmp=path;tmp+=".tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);std::string text;auto card=[&](std::string key,std::string value){key.resize(8,' ');auto s=key+"= "+value;s.resize(80,' ');text+=s;};card("SIMPLE","T");card("BITPIX","-32");card("NAXIS","2");card("NAXIS1",std::to_string(f.width));card("NAXIS2",std::to_string(f.height));card("NCOMBINE",std::to_string(frames));card("EXPTIME",std::to_string(f.exposure));card("ISO",std::to_string(f.iso));card("IMAGETYP","'"+type+"'");std::string pattern;for(int c:f.cfa)pattern+="RGB"[c];card("BAYERPAT","'"+pattern+"'");card("BUNIT","'black-subtracted normalized ADU'");std::string end="END";end.resize(80,' ');text+=end;text.resize(((text.size()+2879)/2880)*2880,' ');out.write(text.data(),text.size());for(float v:f.pixels){auto bits=std::bit_cast<std::uint32_t>(v);for(int s=24;s>=0;s-=8)out.put(char(bits>>s));}std::string pad((2880-(f.pixels.size()*4)%2880)%2880,0);out.write(pad.data(),pad.size());out.flush();if(!out)throw std::runtime_error("Cannot save sensor master");out.close();if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit sensor master");
}
void writeTiff(const std::filesystem::path& path,const Accumulator& a){
    const auto bytes=std::uint64_t(a.width)*a.height*12;if(!a.frames||bytes>0xffffffffu-4096)throw std::runtime_error("Classic TIFF requires a nonempty image smaller than 4 GiB; use FITS");
    auto tmp=path;tmp+=".tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);auto u16=[&](std::uint16_t v){out.put(char(v));out.put(char(v>>8));};auto u32=[&](std::uint32_t v){for(int i=0;i<4;++i)out.put(char(v>>(i*8)));};
    constexpr std::uint32_t extras=8+2+11*12+4,data=extras+12;
    out.write("II",2);u16(42);u32(8);u16(11);auto tag=[&](int id,int type,int count,std::uint32_t v){u16(std::uint16_t(id));u16(std::uint16_t(type));u32(count);u32(v);};
    tag(256,4,1,a.width);tag(257,4,1,a.height);tag(258,3,3,extras);tag(259,3,1,1);tag(262,3,1,2);tag(273,4,1,data);tag(277,3,1,3);tag(278,4,1,a.height);tag(279,4,1,std::uint32_t(bytes));tag(284,3,1,1);tag(339,3,3,extras+6);u32(0);
    for(int i=0;i<3;++i)u16(32);for(int i=0;i<3;++i)u16(3);
    for(std::size_t p=0;p<a.count.size();++p)for(int c=0;c<3;++c)u32(std::bit_cast<std::uint32_t>(a.value(p,c)));out.flush();if(!out)throw std::runtime_error("TIFF write failed");out.close();
    if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit TIFF");
}
}
