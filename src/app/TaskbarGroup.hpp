#pragma once
#include <windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <stdexcept>
// Floating ImGui panels remain native top-level windows. Only the toolbar is
// an application window; the other panels are owned tool windows.
class TaskbarGroup {
public:
    TaskbarGroup() {
        auto& platform=ImGui::GetPlatformIO();create_=platform.Platform_CreateWindow;current_=this;
        platform.Platform_CreateWindow=[](ImGuiViewport* viewport){current_->create_(viewport);current_->apply(viewport);};
    }
    ~TaskbarGroup() {
        // GLFW must destroy its own HWNDs: detach before the owner's destruction
        // would otherwise cause Windows to destroy its owned windows implicitly.
        for(auto* viewport:ImGui::GetPlatformIO().Viewports) {
            const auto hwnd=static_cast<HWND>(viewport->PlatformHandleRaw);
            if(hwnd&&IsWindow(hwnd)&&GetWindow(hwnd,GW_OWNER)==owner_)SetWindowLongPtrW(hwnd,GWLP_HWNDPARENT,0);
        }
        ImGui::GetPlatformIO().Platform_CreateWindow=create_;current_=nullptr;
    }
    HWND owner() const {return owner_;}
    void apply(ImGuiViewport* viewport) {
        if(viewport==ImGui::GetMainViewport()||!viewport->PlatformHandleRaw)return;
        const auto hwnd=static_cast<HWND>(viewport->PlatformHandleRaw);
        const auto* toolbar=ImGui::FindWindowByName("AstroCanon###Toolbar");
        const bool root=toolbar&&toolbar->Viewport==viewport;
        if(root)owner_=hwnd;
        const auto old=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        const auto style=root?((old|WS_EX_APPWINDOW)&~WS_EX_TOOLWINDOW):((old|WS_EX_TOOLWINDOW)&~WS_EX_APPWINDOW);
        if(old!=style) {
            const bool visible=IsWindowVisible(hwnd)!=FALSE;if(visible)ShowWindow(hwnd,SW_HIDE);
            set(hwnd,GWL_EXSTYLE,style);
            if(!SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE))throw std::runtime_error("Taskbar window style update failed");
            if(visible)ShowWindow(hwnd,SW_SHOWNA);
        }
        const auto expected=root?nullptr:owner_;
        if(GetWindow(hwnd,GW_OWNER)!=expected)set(hwnd,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(expected));
    }
private:
    static void set(HWND hwnd,int index,LONG_PTR value){SetLastError(0);if(!SetWindowLongPtrW(hwnd,index,value)&&GetLastError()!=0)throw std::runtime_error("Taskbar window ownership update failed");}
    inline static TaskbarGroup* current_{};
    void (*create_)(ImGuiViewport*){};HWND owner_{};
};
