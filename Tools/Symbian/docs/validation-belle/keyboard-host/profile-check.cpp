#include <services/window/keyboard.h>
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <iostream>
using namespace eka2l1::epoc;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static key_map load(const char* path){
    auto yaml=YAML::LoadFile(path);require(yaml.IsSequence(),"profile must be sequence");
    key_map result;
    for (const auto& entry:yaml){
        require(entry["source"]["type"].as<std::string>()=="key","expected keyboard binding");
        auto code=entry["source"]["data"]["keycode"].as<std::uint32_t>();
        require(result.emplace(code,entry["target"].as<std::uint32_t>()).second,"duplicate source binding");
    }
    return result;
}
static std::optional<event> send(key_event_translator& translator,key_map& map,std::uint32_t host,bool pressed){
    auto scan=map_key_to_inputcode(map,host);require(scan.has_value(),"host key not mapped");
    event raw{};raw.type=pressed?event_code::key_down:event_code::key_up;
    raw.key_evt_={0,*scan,0,0};return translator.translate(raw);
}
int main(int argc,char**argv){
    require(argc==3,"default and opt-in paths needed");
    auto base=load(argv[1]),profile=load(argv[2]);
    require(!map_key_to_inputcode(base,16777249),"default Ctrl unexpectedly changed");
    require(!map_key_to_inputcode(base,'V'),"default V unexpectedly changed");
    require(profile.size()==base.size()+28,"incorrect additional bindings count");
    for(const auto& [source,target]:base)require(profile.at(source)==target,"existing mapping overridden");
    key_event_translator translator;
    require(!send(translator,profile,16777249,true),"Ctrl must not produce key");
    auto v=send(translator,profile,'V',true);
    require(v && v->key_evt_.code==22 && v->key_evt_.scancode=='V',"CtrlV mismatch");
    require(v->key_evt_.modifiers==(event_modifier_left_ctrl|event_modifier_ctrl|event_modifier_repeatable),"CtrlV flags mismatch");
    require(!send(translator,profile,'V',false),"V up must not produce key");
    require(!send(translator,profile,16777249,false),"Ctrl up must not produce key");
    require(translator.modifiers()==0,"Ctrl remained set");
    require(send(translator,profile,'V',true)->key_evt_.code=='v',"normal letter mismatch");
    require(!send(translator,profile,'V',false),"letter up");
    require(!send(translator,profile,16777251,true),"Alt must not produce key");
    require(translator.modifiers()==(event_modifier_left_alt|event_modifier_alt),"Alt flags mismatch");
    require(!send(translator,profile,16777251,false),"Alt up");
    require(translator.modifiers()==0,"Alt remained set");
    std::cout<<"PASS: actual yaml-cpp parse, all original bindings, 28 opt-in mappings, real map_key_to_inputcode and translator CtrlV/Alt chain\n";
}
