#pragma once
#include <string>
#include <string_view>
namespace cof_shadow_animation {
enum class Profile { Generic,Famas,AK47,MP5,M76,G43,Browning,Axe,Shovel,Phone,Flashlight,Lantern,Camera,Flare };
enum class Gesture { None,Draw,Holster,Use,Ignite,Throw };
struct Selection {
 std::string stance="idle",action;
 bool sprint=false,jump=false;
 Profile profile=Profile::Generic;
 Gesture gesture=Gesture::None;
};
inline std::string lower(std::string_view text){std::string result(text);for(char& c:result)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return result;}
inline bool has(const std::string& text,const char* part){return text.find(part)!=std::string::npos;}
// VM clip labels are the timing source. Aliases reuse existing police poses;
// they do not add animations or touch the native player/co-op model.
inline Selection select(std::string_view model,std::string_view clip){
 const auto path=lower(model),label=lower(clip);Selection out;std::string family;
 if(has(path,"/famas/"))out.profile=Profile::Famas;
 else if(has(path,"/ooi_ak47/")||has(path,"/ak47/"))out.profile=Profile::AK47;
 else if(has(path,"/mp5/"))out.profile=Profile::MP5;
 else if(has(path,"/m76/"))out.profile=Profile::M76;
 else if(has(path,"/g43/"))out.profile=Profile::G43;
 else if(has(path,"/browning/"))out.profile=Profile::Browning;
 else if(has(path,"/axe/"))out.profile=Profile::Axe;
 else if(has(path,"/sledgeshovel/"))out.profile=Profile::Shovel;
 else if(has(path,"/mobile/")||has(path,"/radio/"))out.profile=Profile::Phone;
 else if(has(path,"/flashlight/"))out.profile=Profile::Flashlight;
 else if(has(path,"/lantern/"))out.profile=Profile::Lantern;
 else if(has(path,"/camera/"))out.profile=Profile::Camera;
 else if(has(path,"/flare/"))out.profile=Profile::Flare;
 if(has(label,"holster")||has(label,"putaway"))out.gesture=Gesture::Holster;
 else if(has(label,"draw")||has(label,"deploy")||has(label,"equip"))out.gesture=Gesture::Draw;
 else if(out.profile==Profile::Flare&&has(label,"ignite"))out.gesture=Gesture::Ignite;
 else if(out.profile==Profile::Flare&&(has(label,"throw")||label=="discard"))out.gesture=Gesture::Throw;
 else if(label=="use"||has(label,"photo")||has(label,"press")||
  (out.profile==Profile::Camera&&has(label,"shoot"))||
  (out.profile==Profile::Phone&&(has(label,"looksms")||has(label,"raise")||has(label,"lower")))||
  (out.profile==Profile::Flashlight&&label=="switch"))out.gesture=Gesture::Use;
 if(has(path,"/famas/")||has(path,"/m16/")||has(path,"/ooi_ak47/")||has(path,"/ak47/"))family="m16";
 else if(has(path,"/tmp/")||has(path,"/mp5/")||has(path,"/m76/"))family="tmp";
 else if(has(path,"/rifle/")||has(path,"/g43/"))family="rifle";
 else if(has(path,"/shotgun/"))family="shotgun";
 else if(has(path,"/vp70/"))family="vp70";
 else if(has(path,"/glock/")||has(path,"/p345/")||has(path,"/revolver/")||has(path,"/browning/"))family="pistol";
 else if(has(path,"/switchblade/"))family="switchblade";
 else if(has(path,"/nightstick/"))family="nightstick";
 else if(has(path,"/branch/"))family="branch";
 else if(has(path,"/sledgehammer/")||has(path,"/sledgeshovel/")||has(path,"/axe/"))family="sledgehammer";
 else if(has(path,"/syringe/"))family="syringe";
 else if(has(path,"/radio/")||has(path,"/mobile/")||has(path,"/flashlight/")||has(path,"/lantern/")||has(path,"/camera/")||has(path,"/flare/"))family="radio";
 if(!family.empty())out.stance=family+"_idle";
 out.sprint=has(label,"sprint")&&!has(label,"from")&&!has(label,"end");
 out.jump=has(label,"jump")&&!has(label,"from")&&!has(label,"land");
 if(family.empty())return out;
 const bool empty=has(label,"empty")||has(label,"noshot");
 const bool reload=has(label,"reload")||label.rfind("rel_",0)==0;
 if(reload){
  std::string prefix=has(path,"/p345/")?"p345":has(path,"/revolver/")?"revolver":family;
  if(has(label,"start"))out.action=prefix+"_reload_start";
  else if(has(label,"insert"))out.action=prefix+"_reload_insert";
  else if(has(label,"end"))out.action=prefix+"_reload_end"+(empty&&family=="shotgun"?"empty":"");
  else if(prefix=="rifle")out.action="m16_reload"; // G43's magazine reload has no tube-loading phases.
  else out.action=prefix+"_reload"+(empty?(family=="vp70"?"_empty":"empty"):"");
 }else if(family=="syringe"){
  if(label=="use")out.action="syringe_use";
  else if(label=="stab")out.action="syringe_heal_player_hit";
 }else if(has(label,"melee")||has(label,"punch")||has(label,"shove")){
  out.action=family+(family=="nightstick"||family=="sledgehammer"?"_attack":"_melee");
 }else if(has(label,"attack")){
  if(family=="switchblade")out.action=has(label,"attack2")||has(label,"attack3")?"switchblade_attack2":"switchblade_attack1";
  else out.action=family+"_attack";
 }else if((has(label,"shoot")||has(label,"fire"))&&!has(label,"noshot")&&!has(label,"fire_empty")){
  out.action=family+"_shoot";
 }
 return out;
}
}
