#include <astro/Translation.hpp>
#include <astro/Localization.hpp>
#include <map>
namespace astro::ui {
namespace {
thread_local bool english{};
thread_local std::map<std::pair<bool,std::string>,std::string> messages;
thread_local std::map<std::string,std::string,std::less<>> labels;
}
void setLanguage(std::string_view language){english=language=="en";}
std::string translatedMessage(const std::string& text){
    const auto key=std::make_pair(english,text);
    if(auto it=messages.find(key);it!=messages.end())return it->second;
    if(messages.size()>=4096)messages.clear();
    return messages.emplace(key,localizeMessage(text,english?"en":"it")).first->second;
}
const char* tr(const char* text){
    if(!english||!text)return text;
    if(auto it=labels.find(text);it!=labels.end())return it->second.c_str();
    auto translated=localizeMessage(text,"en");
    // Property labels use a bounded set of native JPEG quality names.
    if(auto normal=translated.find("Normale");normal!=std::string::npos)translated.replace(normal,7,"Normal");
    if(translated==text)return text;
    return labels.emplace(text,std::move(translated)).first->second.c_str();
}
}
