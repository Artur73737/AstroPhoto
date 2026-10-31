#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/RemoteMedia.hpp>
#include "LiveViewPreview.hpp"
#include <algorithm>
#include <stdexcept>
namespace astro::remote {
using Microsoft::WRL::ComPtr;
namespace { void check(HRESULT result){if(FAILED(result))throw std::runtime_error("Remote JPEG encoding failed");} }
std::string previewJpeg(PreviewPixels pixels){
    if(pixels.width<1||pixels.height<1||pixels.rgba.size()!=std::size_t(pixels.width)*pixels.height*4)throw std::runtime_error("Invalid preview pixels");
    check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));struct Com{~Com(){CoUninitialize();}} com;
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmap> bitmap;check(factory->CreateBitmapFromMemory(pixels.width,pixels.height,GUID_WICPixelFormat32bppRGBA,pixels.width*4,static_cast<UINT>(pixels.rgba.size()),pixels.rgba.data(),&bitmap));
    const double scale=std::min(1.,1024./std::max(pixels.width,pixels.height));const UINT width=std::max(1,static_cast<int>(pixels.width*scale)),height=std::max(1,static_cast<int>(pixels.height*scale));
    ComPtr<IWICBitmapScaler> scaler;check(factory->CreateBitmapScaler(&scaler));check(scaler->Initialize(bitmap.Get(),width,height,WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> rgb;check(factory->CreateFormatConverter(&rgb));check(rgb->Initialize(scaler.Get(),GUID_WICPixelFormat24bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    ComPtr<IStream> stream;check(CreateStreamOnHGlobal(nullptr,TRUE,&stream));
    ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatJpeg,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;check(encoder->CreateNewFrame(&frame,&options));
    PROPBAG2 property{};property.pstrName=const_cast<LPOLESTR>(L"ImageQuality");VARIANT quality{};quality.vt=VT_R4;quality.fltVal=.78f;check(options->Write(1,&property,&quality));check(frame->Initialize(options.Get()));check(frame->SetSize(width,height));WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;check(frame->SetPixelFormat(&format));check(frame->WriteSource(rgb.Get(),nullptr));check(frame->Commit());check(encoder->Commit());
    STATSTG stat{};check(stream->Stat(&stat,STATFLAG_NONAME));if(stat.cbSize.QuadPart>4*1024*1024)throw std::runtime_error("Encoded preview exceeds 4 MiB");
    LARGE_INTEGER zero{};check(stream->Seek(zero,STREAM_SEEK_SET,nullptr));std::string result(static_cast<std::size_t>(stat.cbSize.QuadPart),'\0');ULONG read{};check(stream->Read(result.data(),static_cast<ULONG>(result.size()),&read));if(read!=result.size())throw std::runtime_error("Incomplete JPEG stream");return result;
}
std::string cameraJpeg(const std::string& jpeg,const LiveFrame& frame){
    if(jpeg.empty()||jpeg.size()>8*1024*1024)throw std::runtime_error("No valid Live View JPEG available");
    check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));struct Com{~Com(){CoUninitialize();}} com;
    std::vector<std::uint8_t> bytes(jpeg.begin(),jpeg.end());return previewJpeg(prepareLiveViewPreview(decodeImage(bytes),frame));
}
}
