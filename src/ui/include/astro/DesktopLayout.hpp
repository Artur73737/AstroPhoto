#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace astro::ui {
struct DesktopArea {float x{},y{},width{},height{};};
// Toolbar, connection, camera, image, sequence, output, log. All dimensions are
// physical pixels inside the monitor work area (taskbar excluded).
inline std::array<DesktopArea,7> desktopLayout(DesktopArea a,float font) {
    const float gap=std::min(8.f,std::min(a.width,a.height)*.01f);
    const float bar=std::min(std::ceil(font*4.4f),a.height*.18f);
    const float w=a.width-2*gap,h=a.height-bar-3*gap;
    const float left=std::floor((w-2*gap)*.23f),right=std::floor((w-2*gap)*.25f),middle=w-2*gap-left-right;
    const float x=a.x+gap,y=a.y+bar+2*gap;
    const float bottom=std::floor(h*.24f),top=h-bottom-gap,connection=std::floor((top-gap)*.25f);
    return {{{x,a.y+gap,w,bar},{x,y,left,connection},{x,y+connection+gap,left,top-connection-gap},
             {x+left+gap,y,middle,top},{x+left+middle+2*gap,y,right,h},
             {x,y+top+gap,left,bottom},{x+left+gap,y+top+gap,middle,bottom}}};
}
}
