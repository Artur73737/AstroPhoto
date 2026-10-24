#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "ControlImage.hpp"
namespace astro {
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("PNG encoding failed: "+std::to_string(hr));}
std::string base64Image(const std::vector<std::uint8_t>& bytes){
    DWORD size{};const auto flags=CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF;
    if(bytes.size()>UINT32_MAX||!CryptBinaryToStringA(bytes.data(),static_cast<DWORD>(bytes.size()),flags,nullptr,&size))throw std::runtime_error("Image encoding failed");
    std::string result(size,'\0');if(!CryptBinaryToStringA(bytes.data(),static_cast<DWORD>(bytes.size()),flags,result.data(),&size))throw std::runtime_error("Image encoding failed");
    result.resize(size);return result;
}
nlohmann::json pngImage(std::vector<std::uint8_t> rgba,int width,int height,bool flip,void* nativeWindow){
    if(width<1||height<1||static_cast<std::uint64_t>(width)*height>16000000||rgba.size()!=static_cast<std::size_t>(width)*height*4)throw std::invalid_argument("Image is absent or exceeds 16 megapixels");
    const auto stride=static_cast<std::size_t>(width)*4;
    if(flip)for(int y=0;y<height/2;++y)std::swap_ranges(rgba.begin()+y*stride,rgba.begin()+(y+1)*stride,rgba.begin()+(height-y-1)*stride);
    if(nativeWindow){auto region=CreateRectRgn(0,0,0,0);if(GetWindowRgn(static_cast<HWND>(nativeWindow),region)!=ERROR)for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(!PtInRegion(region,x,y))rgba[static_cast<std::size_t>(y)*stride+x*4+3]=0;DeleteObject(region);}
    for(std::size_t i=0;i<rgba.size();i+=4)std::swap(rgba[i],rgba[i+2]);
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IStream> stream;check(CreateStreamOnHGlobal(nullptr,TRUE,&stream));ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));check(frame->SetSize(width,height));auto format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format));if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("PNG format unsupported");
    check(frame->WritePixels(height,static_cast<UINT>(stride),static_cast<UINT>(rgba.size()),rgba.data()));check(frame->Commit());check(encoder->Commit());
    STATSTG info{};check(stream->Stat(&info,STATFLAG_NONAME));if(info.cbSize.QuadPart>48*1024*1024)throw std::runtime_error("PNG response too large");
    LARGE_INTEGER zero{};check(stream->Seek(zero,STREAM_SEEK_SET,nullptr));std::vector<std::uint8_t> bytes(static_cast<std::size_t>(info.cbSize.QuadPart));ULONG read{};check(stream->Read(bytes.data(),static_cast<ULONG>(bytes.size()),&read));if(read!=bytes.size())throw std::runtime_error("Incomplete PNG stream");
    return {{"ok",true},{"mime_type","image/png"},{"width",width},{"height",height},{"data",base64Image(bytes)}};
}
}
