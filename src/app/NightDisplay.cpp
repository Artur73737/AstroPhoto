#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <GLFW/glfw3.h>
#include "NightDisplay.hpp"
#include <stdexcept>
#include <string>
namespace astro {
namespace {
template<class F> F load(const char* name){auto p=glfwGetProcAddress(name);if(!p)throw std::runtime_error(std::string("OpenGL entry point missing: ")+name);return reinterpret_cast<F>(p);}
}
struct NightDisplay::Impl {
    using CreateShader=GLuint(APIENTRY*)(GLenum);
    using ShaderSource=void(APIENTRY*)(GLuint,GLsizei,const char* const*,const GLint*);
    using ObjectAction=void(APIENTRY*)(GLuint);
    using GetStatus=void(APIENTRY*)(GLuint,GLenum,GLint*);
    using GetLog=void(APIENTRY*)(GLuint,GLsizei,GLsizei*,char*);
    using CreateProgram=GLuint(APIENTRY*)();
    using Attach=void(APIENTRY*)(GLuint,GLuint);
    using UniformLocation=GLint(APIENTRY*)(GLuint,const char*);
    using UniformInt=void(APIENTRY*)(GLint,GLint);
    using ActiveTexture=void(APIENTRY*)(GLenum);
    CreateShader createShader=load<CreateShader>("glCreateShader");
    ShaderSource shaderSource=load<ShaderSource>("glShaderSource");
    ObjectAction compile=load<ObjectAction>("glCompileShader"),deleteShader=load<ObjectAction>("glDeleteShader"),link=load<ObjectAction>("glLinkProgram"),use=load<ObjectAction>("glUseProgram"),deleteProgram=load<ObjectAction>("glDeleteProgram");
    GetStatus shaderStatus=load<GetStatus>("glGetShaderiv"),programStatus=load<GetStatus>("glGetProgramiv");
    GetLog shaderLog=load<GetLog>("glGetShaderInfoLog"),programLog=load<GetLog>("glGetProgramInfoLog");
    CreateProgram createProgram=load<CreateProgram>("glCreateProgram");
    Attach attach=load<Attach>("glAttachShader");
    UniformLocation location=load<UniformLocation>("glGetUniformLocation");
    UniformInt uniform=load<UniformInt>("glUniform1i");
    ActiveTexture activeTexture=load<ActiveTexture>("glActiveTexture");
    GLuint program{},texture{};int width{},height{},lastTheme{-1};
    GLuint shader(GLenum kind,const char* source){auto id=createShader(kind);shaderSource(id,1,&source,nullptr);compile(id);GLint ok{};shaderStatus(id,0x8B81,&ok);if(!ok){char log[1024]{};shaderLog(id,sizeof(log),nullptr,log);deleteShader(id);throw std::runtime_error(std::string("Night display shader: ")+log);}return id;}
    Impl(){
        // The app requests OpenGL 3.0 compatibility, also used by the ImGui backend.
        auto vertex=shader(0x8B31,"#version 120\nvarying vec2 uv;void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;}");
        GLuint fragment{};
        try{fragment=shader(0x8B30,"#version 120\nuniform sampler2D scene;varying vec2 uv;void main(){vec3 rgb=texture2D(scene,uv).rgb;float y=dot(rgb,vec3(0.299,0.587,0.114));gl_FragColor=vec4(y*0.8,0.0,0.0,1.0);}");program=createProgram();attach(program,vertex);attach(program,fragment);link(program);GLint ok{};programStatus(program,0x8B82,&ok);if(!ok){char log[1024]{};programLog(program,sizeof(log),nullptr,log);throw std::runtime_error(std::string("Night display link: ")+log);}}
        catch(...){deleteShader(vertex);if(fragment)deleteShader(fragment);if(program)deleteProgram(program);throw;}
        deleteShader(vertex);deleteShader(fragment);
    }
    ~Impl(){if(texture)glDeleteTextures(1,&texture);if(program)deleteProgram(program);}
};
NightDisplay::NightDisplay():impl_(std::make_unique<Impl>()){}
NightDisplay::~NightDisplay()=default;
void NightDisplay::render(int width,int height,bool enabled){
    if(!enabled||width<1||height<1)return;auto& p=*impl_;
    GLint oldProgram{},oldActive{};glGetIntegerv(0x8B8D,&oldProgram);glGetIntegerv(0x84E0,&oldActive);
    glPushAttrib(GL_ALL_ATTRIB_BITS);p.activeTexture(0x84C0);
    if(!p.texture)glGenTextures(1,&p.texture);glBindTexture(GL_TEXTURE_2D,p.texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
    if(p.width!=width||p.height!=height){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);p.width=width;p.height=height;}
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width,height);
    glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,width,height);
    p.use(p.program);p.uniform(p.location(p.program,"scene"),0);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);glEnd();
    p.use(oldProgram);glPopAttrib();p.activeTexture(oldActive);
}
void NightDisplay::caption(void* window,bool light,bool night){
    auto& p=*impl_;const int theme=night?2:light?1:0;if(theme==p.lastTheme)return;p.lastTheme=theme;
    const auto hwnd=static_cast<HWND>(window);BOOL dark=!light;
    // Older Windows releases may not support custom caption colours; client rendering is unaffected.
    (void)DwmSetWindowAttribute(hwnd,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    COLORREF background=night?RGB(20,0,0):DWMWA_COLOR_DEFAULT,text=night?RGB(190,0,0):DWMWA_COLOR_DEFAULT;
    (void)DwmSetWindowAttribute(hwnd,DWMWA_CAPTION_COLOR,&background,sizeof(background));
    (void)DwmSetWindowAttribute(hwnd,DWMWA_TEXT_COLOR,&text,sizeof(text));
}
}
