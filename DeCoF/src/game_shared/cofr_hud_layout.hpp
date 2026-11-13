// Recovered original custom-inventory source, Git blob e51762a6f2584f41b9fb71621d4b364ffb9a4149.
#pragma once
#include "cofr_hud_state.hpp"
namespace cof::hud {
template<class Sink> void label(Sink& s,int x,int y,const std::string& value,int size,int shade) {
    s.text(x+1,y+1,value,size,0);s.text(x,y,value,size,shade);
}
template<class Sink> void glyph(Sink& s,const char* name,int x,int y,int w,int h,int opacity) {
    s.icon(name,x+1,y+1,w,h,opacity*3/4,0);s.icon(name,x,y,w,h,opacity,255);
}
// Coordinates are logical 1080p pixels. Sink alpha uses conventional opacity.
template<class Sink> void paint(Sink& s,const View& v,int width,int height) {
    const float scale=scaleFor(width,height);
    if(!std::isfinite(scale)||scale<=0)return;
    const int right=int(std::lround(float(width)/scale))-40;
    const int bottom=int(std::lround(float(height)/scale))-40;
    const int y=bottom-60;
    if(v.healthVisible) {
        glyph(s,"heart",50,y-1,24,28,235);
        s.box(96,y,320,26,15,15,17,215);
        s.outline(96,y,320,26,91,88,86,200);
        const int fill=std::clamp(v.health,0,100)*314/100;
        const float pulse=v.health>0&&v.health<25&&std::isfinite(v.time)?.5f+.5f*std::sin(v.time*4.2f):0.f;
        if(fill)s.box(99,y+3,fill,20,139+int(pulse*20),68,70,235);
        // Flat ink with dirt across the track; no highlight or bevel.
        s.icon("grime",98,y+2,316,22,210,255);
        if(v.cap)label(s,96,y-18,"CAPPED",10,160);
    }
    if(v.staminaVisible) {
        glyph(s,"runner",50,y+31,24,28,235);
        s.box(96,y+40,320,10,15,15,17,215);
        s.outline(96,y+40,320,10,81,86,91,195);
        const int fill=std::clamp(v.stamina,0,100)*316/100;
        if(fill)s.box(98,y+42,fill,6,100,128,154,235);
        s.icon("grime",98,y+42,316,6,205,255);
    }
    // Syringes and flares have a native reserve count but no magazine.
    const auto uses = [](const Ammo& ammo) { return (ammo.id == 8 || ammo.id == 13) && ammo.reserve >= 0; };
    const bool a=firearm(v.primary)||uses(v.primary),b=firearm(v.secondary)||uses(v.secondary);
    if(!a&&!b)return;
    const bool dual=a&&b;
    const bool shared=dual&&v.primary.type==v.secondary.type;
    const auto slots=[](const Ammo& ammo){return std::clamp(ammo.capacity>0?ammo.capacity:std::max(1,ammo.clip),1,128);};
    const auto rows=[&](const Ammo& ammo){return (slots(ammo)+39)/40;};
    const auto groupHeight = [&](const Ammo& ammo, bool magazines) {
        return uses(ammo) ? 31 : (magazines ? 52 : 20) + rows(ammo) * 27;
    };
    const int primaryHeight = groupHeight(a ? v.primary : v.secondary, true);
    const int secondaryHeight = dual ? groupHeight(v.secondary, !shared) : 0;
    int top=bottom-primaryHeight-secondaryHeight;
    const auto group=[&](const Ammo& ammo,int groupTop,bool magazines) {
        if (uses(ammo)) {
            label(s, right - 155, groupTop + 6,
                  std::string(ammo.id == 8 ? "SYRINGES  " : "FLARES  ") + std::to_string(ammo.reserve), 12, 205);
            return;
        }
        const int count=slots(ammo);
        const int reserveSlots=ammo.capacity>0?std::min((ammo.reserve+ammo.capacity-1)/ammo.capacity,11):0;
        const int wide=std::max(180,std::max(std::min(count,40)*11,48+reserveSlots*21));
        const int left=right-wide;
        int bulletsTop=groupTop+20;
        if(magazines) {
            label(s,left,groupTop+8,"MAGS",12,205);
            if(ammo.capacity>0) {
                const int full=ammo.reserve/ammo.capacity;
                const bool partial=ammo.reserve%ammo.capacity!=0;
                const int total=full+(partial?1:0);
                // Reserve is loose rounds; a dim final icon is a partial equivalent.
                for(int i=0;i<std::min(total,11);++i)
                    glyph(s,"magazine",left+48+i*21,groupTop,16,28,i<full?235:110);
                if(total>11)label(s,right-12,groupTop+8,"+",12,205);
            }
            bulletsTop=groupTop+52;
        }
        label(s,left,bulletsTop-16,"MAG SIZE",10,170);
        for(int i=0;i<count;++i)
            glyph(s,"bullet",left+(i%40)*11,bulletsTop+(i/40)*27,8,22,i<ammo.clip?235:45);
    };
    group(a?v.primary:v.secondary,top,true);
    if(dual)group(v.secondary,top+primaryHeight,!shared);
}
}
