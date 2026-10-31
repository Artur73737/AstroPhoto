#define NOMINMAX
#include <windows.h>
#include <astro/RemoteGallery.hpp>
#include <astro/RemoteAuth.hpp>
#include <astro/RemoteMedia.hpp>
#include "PhotoPreview.hpp"
#include "ControlImage.hpp"
#include <algorithm>
namespace astro::remote {
namespace {
bool within(const std::filesystem::path& root,const std::filesystem::path& path){auto rel=path.lexically_relative(root);return !rel.empty()&&!rel.is_absolute()&&*rel.begin()!=L"..";}
std::string utf8(const std::filesystem::path& p){auto u=p.u8string();return {u.begin(),u.end()};}
}
nlohmann::json Gallery::list(const std::filesystem::path& root){
    files_.clear();root_=std::filesystem::weakly_canonical(root);nlohmann::json result=nlohmann::json::array();
    if(!std::filesystem::exists(root_))return {{"items",result},{"truncated",false}};
    std::vector<std::filesystem::directory_entry> entries;std::size_t visited{};
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root_,std::filesystem::directory_options::skip_permission_denied)){
        if(++visited>10000)break;if(!entry.is_regular_file())continue;auto path=std::filesystem::weakly_canonical(entry.path());if(!within(root_,path))continue;
        auto ext=path.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(ext==".jpg"||ext==".jpeg"||ext==".cr2"||ext==".png"||ext==".tif"||ext==".tiff")entries.push_back(entry);
    }
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.last_write_time()>b.last_write_time();});const bool truncated=visited>10000||entries.size()>200;if(entries.size()>200)entries.resize(200);
    for(const auto& entry:entries){const auto path=std::filesystem::weakly_canonical(entry.path());auto id=hmac("AstroPhoto gallery identifier",utf8(path));files_[id]=path;result.push_back({{"id",id},{"name",utf8(path.filename())},{"relative_path",utf8(path.lexically_relative(root_))},{"bytes",entry.file_size()}});}
    return {{"items",result},{"truncated",truncated}};
}
std::string Gallery::image(const std::filesystem::path& root,const std::string& id){
    if(std::filesystem::weakly_canonical(root)!=root_||!files_.contains(id))throw std::runtime_error("Refresh the PC gallery before opening this image");
    const auto path=std::filesystem::canonical(files_.at(id));if(!within(root_,path))throw std::runtime_error("Image is outside the output folder");
    const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(hr))throw std::runtime_error("Cannot initialize image decoder");struct Com{~Com(){CoUninitialize();}} com;
    auto pixels=decodePreview(path);
    if(pixels.width>1920||pixels.height>1920){const double scale=1920./std::max(pixels.width,pixels.height);const int w=std::max(1,int(pixels.width*scale)),h=std::max(1,int(pixels.height*scale));std::vector<std::uint8_t> resized(std::size_t(w)*h*4);for(int y=0;y<h;++y)for(int x=0;x<w;++x)std::copy_n(pixels.rgba.data()+(std::size_t(int(y/scale))*pixels.width+int(x/scale))*4,4,resized.data()+(std::size_t(y)*w+x)*4);pixels.rgba=std::move(resized);pixels.width=w;pixels.height=h;}
    return previewJpeg(std::move(pixels));
}
}
