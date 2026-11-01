#pragma once
#include <algorithm>
namespace astro::ui {
enum class WorkspaceLayout { Compact, Medium, Wide };
// Breakpoints follow font size, so high DPI and explicit UI scaling get the
// same usable layout as a smaller window. Dock nodes handle resizing within it.
inline WorkspaceLayout workspaceLayout(float width,float height,float font) {
    font=std::max(9.f,font);
    if(width<font*80||height<font*32)return WorkspaceLayout::Compact;
    if(width<font*125||height<font*48)return WorkspaceLayout::Medium;
    return WorkspaceLayout::Wide;
}
}
