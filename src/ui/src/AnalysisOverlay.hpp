#pragma once
#include "Widgets.hpp"
#include <astro/PreviewAnalysis.hpp>
#include <cmath>
namespace astro::ui {
inline std::string focusMeasurementText(const FocusReading& f,bool peaking) {
    char sharpness[128];std::snprintf(sharpness,sizeof(sharpness),tr("Nitidezza %.2f | picco confermato %.2f"),f.score,f.best);
    std::string coverage;
    if(!peaking)coverage=tr("Peaking: disattivato");
    else if(!f.peakingMeasured)coverage=tr("Peaking: in attesa del frame");
    else if(f.peakingPixels==0)coverage=tr("Copertura ROI: 0 pixel - nessun segnale");
    else {char text[128];std::snprintf(text,sizeof(text),tr("Copertura ROI: %.2f%% (%d px)"),100*f.peakingFraction,f.peakingPixels);coverage=text;}
    return std::string(sharpness)+'\n'+coverage;
}
inline ImVec2 histogramSize(ImVec2 image) {return {std::min(widgets::em(19),image.x*.44f),std::min(widgets::em(9),image.y*.38f)};}
inline void drawHistogram(ImDrawList& draw,ImVec2 origin,ImVec2 size,const Histogram& histogram) {
    if(size.x<70||size.y<45||!histogram.pixels)return;
    draw.AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(8,12,18,210),5);
    const float margin=5,top=ImGui::GetFontSize()+6;
    draw.AddText({origin.x+margin,origin.y+3},IM_COL32(225,230,235,255),"RGB / Y  (log)");
    const ImVec2 lo{origin.x+margin,origin.y+top},hi{origin.x+size.x-margin,origin.y+size.y-margin};
    draw.AddLine({lo.x,hi.y},hi,IM_COL32(140,145,150,200));
    const ImU32 colors[]{IM_COL32(255,75,75,220),IM_COL32(80,230,100,220),IM_COL32(90,150,255,230),IM_COL32(240,240,240,160)};
    std::uint64_t maximum=1;for(const auto& channel:histogram.bins)for(auto value:channel)maximum=std::max(maximum,value);
    const double denominator=std::log1p(static_cast<double>(maximum));
    for(int c=0;c<4;++c) {
        ImVec2 previous{};
        for(int bin=0;bin<256;++bin) {
            const ImVec2 point{lo.x+(hi.x-lo.x)*bin/255.f,hi.y-static_cast<float>(std::log1p(static_cast<double>(histogram.bins[c][bin]))/denominator)*(hi.y-lo.y)};
            if(bin)draw.AddLine(previous,point,colors[c]);previous=point;
        }
    }
}
inline void drawFocusReading(ImDrawList& draw,ImVec2 origin,ImVec2 imageSize,int width,int height,const FocusReading& f,FocusTarget target,bool peaking) {
    const auto color=f.nearBest?IM_COL32(80,245,130,255):IM_COL32(255,195,65,255);
    if(f.width>0&&f.height>0) {
        const ImVec2 a{origin.x+imageSize.x*f.x/width,origin.y+imageSize.y*f.y/height};
        const ImVec2 b{a.x+imageSize.x*f.width/width,a.y+imageSize.y*f.height/height};
        draw.AddRect(a,b,color,2,0,1.5f);
    }
    const ImVec2 point{origin.x+imageSize.x*target.u,origin.y+imageSize.y*target.v};
    draw.AddCircle(point,4,color);draw.AddLine({point.x-7,point.y},{point.x+7,point.y},color);draw.AddLine({point.x,point.y-7},{point.x,point.y+7},color);
    const char* state=!f.valid?tr("Poco dettaglio / saturazione"):f.nearBest?tr("OK: vicino al miglior fuoco misurato"):f.bracketed?tr("Torna verso il picco misurato"):tr("Ruota la ghiera oltre il picco, poi torna");
    const auto measurement=focusMeasurementText(f,peaking);const char* values=measurement.c_str();
    const float wrap=std::max(30.f,std::min(widgets::em(36),imageSize.x-16));
    const auto a=ImGui::CalcTextSize(state,nullptr,false,wrap),b=ImGui::CalcTextSize(values,nullptr,false,wrap);
    const float h=a.y+b.y+13;
    const ImVec2 pos{origin.x+4,origin.y+std::max(0.f,imageSize.y-h-4)};
    draw.AddRectFilled(pos,{pos.x+wrap+8,pos.y+h},IM_COL32(8,12,18,215),5);
    draw.AddText(ImGui::GetFont(),ImGui::GetFontSize(),{pos.x+4,pos.y+4},color,state,nullptr,wrap);
    draw.AddText(ImGui::GetFont(),ImGui::GetFontSize(),{pos.x+4,pos.y+a.y+8},IM_COL32(225,230,235,255),values,nullptr,wrap);
}
}
