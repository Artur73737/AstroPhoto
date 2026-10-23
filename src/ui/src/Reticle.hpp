#pragma once
#include <imgui.h>
#include <algorithm>
#include <string_view>
namespace astro::ui {
inline void drawReticle(ImDrawList& draw,ImVec2 origin,ImVec2 size,std::string_view shape) {
    const ImVec2 end{origin.x+size.x,origin.y+size.y},center{origin.x+size.x/2,origin.y+size.y/2};
    const auto color=IM_COL32(255,32,32,230);
    const float radius=std::min(size.x,size.y)*0.14f,thickness=std::max(1.0f,ImGui::GetFontSize()/13.0f);
    draw.PushClipRect(origin,end,true);
    auto line=[&](ImVec2 a,ImVec2 b){draw.AddLine(a,b,color,thickness);};
    if(shape=="thirds") {
        for(int i=1;i<=2;++i){const float x=origin.x+size.x*i/3,y=origin.y+size.y*i/3;line({x,origin.y},{x,end.y});line({origin.x,y},{end.x,y});}
    } else if(shape=="circle")draw.AddCircle(center,radius,color,64,thickness);
    else if(shape=="target") {
        draw.AddCircle(center,radius,color,64,thickness);draw.AddCircle(center,radius*2,color,96,thickness);
        line({center.x-radius/3,center.y},{center.x+radius/3,center.y});line({center.x,center.y-radius/3},{center.x,center.y+radius/3});
    } else if(shape=="gap_cross") {
        const float gap=std::min(size.x,size.y)*0.025f;
        line({center.x,origin.y},{center.x,center.y-gap});line({center.x,center.y+gap},{center.x,end.y});
        line({origin.x,center.y},{center.x-gap,center.y});line({center.x+gap,center.y},{end.x,center.y});
    } else {
        line({center.x,origin.y},{center.x,end.y});line({origin.x,center.y},{end.x,center.y});
    }
    draw.PopClipRect();
}
}
