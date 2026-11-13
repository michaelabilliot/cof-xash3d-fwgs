// Legacy graphical HUD reconstruction: verified cofr13_client 539453f1bb2de2a1f8daa52934fcb638.
// Layout constants from its HUD paint methods; installed original TGA assets retained.
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include "../../../xash3d-fwgs/ref/d3d9/dx_cof_light.h"
#include "../../../xash3d-fwgs/ref/d3d9/dx_cof_native_light.h"
#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "HUDControl_guess.h"
#include "RECBInventory_guess.h"
#include "Padlock_guess.h"
#include "com_model.h"
#include "ammohistory.h"
#include "parsemsg.h"
#include "cofr_legacy.h"
#include "cofr_ammo_policy.hpp"
#include <VGUI_Panel.h>
#include <VGUI_Font.h>
#include <VGUI_SurfaceBase.h>
#include <windows.h>
#undef min
#undef max
#include "cofr_hud_layout.hpp"
extern WeaponsResource gWR;
extern int g_iDualWeapon1_guess,g_iDualWeapon2_guess;
extern int iBoat_guess,iWheelchair_guess;
extern cl_entity_t* pAttachCamEnt_guess;
extern bool bAttachCam_guess;
extern vec3_t vecBoatOrigin_guess,vecWheelchairOrigin_guess,vecCameraAttachment_guess,vecCameraAngles_guess;
extern void VectorAngles(const float*,float*);
namespace {
static int magazineCapacity[256]={};
static auto& engine=gEngfuncs;
static int maxHealth=100,hudCap=0,currentHealth=100;static bool healthKnown=false;static unsigned paintFrames=0;
static bool assetsReady=false;
static void log(const char* text,const char* value){engine.Con_Printf(const_cast<char*>(text),value);engine.Con_Printf("\n");}
template<class T>T renderer(const char* name){auto module=GetModuleHandleA("ref_d3d9.dll");return module?reinterpret_cast<T>(GetProcAddress(module,name)):nullptr;}
class HudPanel:public vgui::Panel {
 float scale=1;std::map<int,vgui::Font*> fonts;
 struct Texture{int id=0,width=0,height=0;};std::map<std::string,Texture> textures;
 int px(int n)const{return int(std::lround(n*scale));}
 void color(int r,int g,int b,int alpha=0){drawSetColor(r,g,b,alpha);}
 void rect(int x,int y,int w,int h){drawFilledRect(px(x),px(y),px(x+w),px(y+h));}
    Texture texture(const std::string& path) {
        auto found = textures.find(path); if (found != textures.end()) return found->second;
        Texture result;
        if (path.empty()) return result;
        int size = 0; auto bytes = engine.COM_LoadFile(const_cast<char*>(path.c_str()), 5, &size);
        if (!bytes || size < 18) { if (bytes) engine.COM_FreeFile(bytes); textures[path] = result; return result; }
        // True-colour TGA, including the stock RLE images. Decode user's assets at runtime.
        const unsigned char* p = bytes; int type = p[2], bpp = p[16] / 8;
        int w = p[12] | p[13] << 8, h = p[14] | p[15] << 8;
        size_t count = size_t(w) * h, pos = 18 + p[0];
        if (p[1] || (type != 2 && type != 10) || (bpp != 3 && bpp != 4) || w < 1 || h < 1 || w > 4096 || h > 4096 || (p[17] & 0x10)) {
            engine.COM_FreeFile(bytes); textures[path] = result; return result;
        }
        std::vector<unsigned char> rgba(count * 4); size_t out = 0; bool good = true;
        auto pixel = [&](const unsigned char* value) {
            size_t x = out % w, y = out / w;
            if (!(p[17] & 0x20)) y = h - 1 - y;
            size_t dest = (y * w + x) * 4;
            rgba[dest] = value[2]; rgba[dest + 1] = value[1]; rgba[dest + 2] = value[0]; rgba[dest + 3] = bpp == 4 ? value[3] : 255;
            ++out;
        };
        while (out < count && good) {
            int run = 1; bool repeat = false;
            if (type == 10) { if (pos >= size_t(size)) { good = false; break; } int code = p[pos++]; run = (code & 127) + 1; repeat = (code & 128) != 0; }
            if (out + run > count || pos + size_t(bpp) * (repeat ? 1 : run) > size_t(size)) { good = false; break; }
            for (int i = 0; i < run; ++i) { pixel(p + pos); if (!repeat) pos += bpp; }
            if (repeat) pos += bpp;
        }
        if (good) {
            result = {getSurfaceBase()->createNewTextureID(), w, h};
            drawSetTextureRGBA(result.id, reinterpret_cast<const char*>(rgba.data()), w, h);
        } else log("CLIENT: malformed TGA rejected: %s", path.c_str());
        engine.COM_FreeFile(bytes); textures[path] = result; return result;
    }
    void picture(const std::string& path, int x, int y, int w, int h, int alpha = 0) {
        auto t = texture(path); if (!t.id) return;
        float ratio = std::min(float(w) / t.width, float(h) / t.height);
        int iw = int(t.width * ratio), ih = int(t.height * ratio);
        x += (w - iw) / 2; y += (h - ih) / 2;
        color(255, 255, 255, alpha); drawSetTexture(t.id); drawTexturedRect(px(x), px(y), px(x + iw), px(y + ih));
    }

 void icon(const char* name,int x,int y,int w,int h,int a=0){picture(std::string("gfx/vgui/cof8_hud/")+name+".tga",x,y,w,h,a);}
 int capacity(int id){
  if(id>0&&id<256&&magazineCapacity[id]>0)return magazineCapacity[id];
  // Native WeaponList has reserve maxima, not magazine capacities. WpnInfo supplies these elsewhere.
  switch(id){case 3:return 15;case 4:return 5;case 7:return 12;case 8:return 1;case 9:return 30;case 11:return 5;case 13:return 1;case 14:return 30;case 19:return 18;case 20:return 5;case 21:return 6;case 26:return 10;case 27:return 30;case 31:return 1;default:return 0;}
 }
 void paint() override {
  if(!CoFR_CustomHudActive()||!gViewPort||!gViewPort->m_pHUDControl_guess)return;
  if(gViewPort->m_pRECBInventory_guess&&gViewPort->m_pRECBInventory_guess->isVisible())return;
  int hp=healthKnown?currentHealth:gHUD.m_Health.m_iHealth;if(hp<=0||engine.IsSpectateOnly()||(gHUD.m_iHideHUDDisplay&12))return;
  auto hud=gViewPort->m_pHUDControl_guess;cof::hud::View v;
  v.time=engine.GetClientTime();v.health=hp*100/std::max(1,maxHealth);v.stamina=hud->m_iStamina_guess;
  v.healthVisible=v.staminaVisible=true;v.cap=hudCap;
  auto ammo=[&](int id,int clip){cof::hud::Ammo a;if(id<1||id>=MAX_WEAPONS||cof::hideWeaponAmmo(id))return a;
   auto& w=gWR.rgWeapons[id];a.id=id;a.clip=clip;a.capacity=capacity(id);a.type=w.iAmmoType;
   a.reserve=w.iAmmoType>=0&&w.iAmmoType<MAX_AMMO_TYPES?gWR.CountAmmo(w.iAmmoType):-1;return a;};
  auto weapon=gHUD.m_Ammo.m_pWeapon;
  if(weapon&&!(gHUD.m_iHideHUDDisplay&1)){
   if(weapon->iId==15){v.primary=ammo(g_iDualWeapon1_guess,gHUD.m_Ammo.m_iDualAmmoLeft_guess);v.secondary=ammo(g_iDualWeapon2_guess,gHUD.m_Ammo.m_iDualAmmoRight_guess);}
   else v.primary=ammo(weapon->iId,weapon->iClip);
  }
  int w,h;getSize(w,h);cof::hud::paint(*this,v,w,h);++paintFrames;
 }
public:
 HudPanel():vgui::Panel(0,0,1,1){setPaintBackgroundEnabled(false);setPaintBorderEnabled(false);setAutoFocusNavEnabled(false);setCursor(vgui::Scheme::scu_none);}
 bool isWithin(int,int)override{return false;}
 vgui::Panel* isWithinTraverse(int,int)override{return nullptr;}
 void resize(int w,int h){scale=cof::hud::scaleFor(w,h);setBounds(0,0,w,h);}
 void videoReset(){textures.clear();}
 void box(int x,int y,int w,int h,int r,int g,int b,int opacity){color(r,g,b,255-opacity);rect(x,y,w,h);}
 void outline(int x,int y,int w,int h,int r,int g,int b,int opacity){color(r,g,b,255-opacity);drawOutlinedRect(px(x),px(y),px(x+w),px(y+h));}
 void text(int x,int y,const std::string& value,int size,int shade){int pixels=std::max(8,px(size));auto& f=fonts[pixels];if(!f)f=new vgui::Font("Arial",pixels,0,0,400,false,false,false,false);drawSetTextFont(f);drawSetTextColor(shade,shade,shade,0);drawSetTextPos(px(x),px(y));drawPrintText(value.c_str(),int(value.size()));}
 void icon(const char* name,int x,int y,int w,int h,int opacity,int shade){auto t=texture(std::string("gfx/vgui/cof8_hud/")+name+".tga");if(!t.id)return;color(shade,shade,shade,255-opacity);drawSetTexture(t.id);drawTexturedRect(px(x),px(y),px(x+w),px(y+h));}
 bool ready(){for(const char* n:{"heart","runner","bullet","magazine","grime"})if(!texture(std::string("gfx/vgui/cof8_hud/")+n+".tga").id)return false;return true;}
};
static HudPanel* panel=nullptr;static vgui::Panel* parent=nullptr;
static int maxHP(const char*,int size,void* data){if(size==4){BEGIN_READ(data,size);maxHealth=std::max(1,READ_SHORT());currentHealth=std::max(0,READ_SHORT());healthKnown=true;}return 1;}
static void dump(){auto w=gHUD.m_Ammo.m_pWeapon;engine.Con_Printf("COFR HUD paint frames=%u\n",paintFrames);engine.Con_Printf("COFR HUD: assets=%d attached=%d health=%d cap=%d stamina=%d id=%d clip=%d dual=%d/%d\n",assetsReady,panel!=nullptr,healthKnown?currentHealth:gHUD.m_Health.m_iHealth,maxHealth,gViewPort&&gViewPort->m_pHUDControl_guess?gViewPort->m_pHUDControl_guess->m_iStamina_guess:-1,w?w->iId:0,w?w->iClip:0,gHUD.m_Ammo.m_iDualAmmoLeft_guess,gHUD.m_Ammo.m_iDualAmmoRight_guess);}
}
bool CoFR_CustomHudActive(){return assetsReady&&engine.pfnGetCvarFloat("cof8_hud")!=0&&engine.pfnGetCvarFloat("hud_draw")!=0;}
void CoFR_LegacyInit(){engine.pfnRegisterVariable("cof8_hud","1",FCVAR_ARCHIVE);engine.pfnHookUserMsg("Cof8MaxHP",maxHP);engine.pfnAddCommand("cof8_hud_dump",dump);}
void CoFR_LegacyFrame(){
 if(!gViewPort||!gViewPort->m_pHUDControl_guess)return;auto p=gViewPort->m_pHUDControl_guess;
 if(gViewPort->m_pPadlock_guess&&gViewPort->m_pPadlock_guess->isVisible()){
  auto lock=gViewPort->m_pPadlock_guess;if(lock->m_pButtonsImage_guess)lock->m_pButtonsImage_guess->setColor(vgui::Color(255,255,255,1));gViewPort->setVisible(true);lock->setPaintEnabled(true);lock->repaint();
  if(lock->m_pButtons_guess){lock->m_pButtons_guess->setVisible(true);lock->m_pButtons_guess->setPaintEnabled(true);lock->m_pButtons_guess->setPaintBackgroundEnabled(true);lock->m_pButtons_guess->repaint();}
 }
 if(parent!=p){parent=p;panel=new HudPanel;panel->setParent(p);assetsReady=panel->ready();engine.Con_Printf("COFR HUD: source panel attached; graphical assets=%d\n",assetsReady);}
 SCREENINFO info={};info.iSize=sizeof(info);if(engine.pfnGetScreenInfo(&info))panel->resize(info.iWidth,info.iHeight);panel->setVisible(CoFR_CustomHudActive());panel->repaint();
 if(CoFR_CustomHudActive()){p->m_pHealthBar_guess->setVisible(false);p->m_pStaminaBar_guess->setVisible(false);}
}
void CoFR_LegacyShutdown(){panel=nullptr;parent=nullptr;assetsReady=false;maxHealth=100;healthKnown=false;}
bool CoFR_BlurBackground(){auto draw=renderer<int(__cdecl*)()>("DX_CofPauseBackgroundV1");if(!draw||!draw())return false;SCREENINFO info={};info.iSize=sizeof(info);if(engine.pfnGetScreenInfo(&info))engine.pfnFillRGBA(0,0,info.iWidth,info.iHeight,0,0,0,115);return true;}
void CoFR_CameraFlash(){auto flash=renderer<int(__cdecl*)(float)>("DX_CofCameraFlashV1");if(flash)flash(engine.GetClientTime());}
void CoFR_RefreshCamera(){
 auto query=renderer<int(__cdecl*)(cl_entity_t*,float,float*,int)>("DX_StudioCameraAttachmentsV3");if(!query)return;
 float points[12];float now=engine.GetClientTime();
 if(iBoat_guess&&query(reinterpret_cast<cl_entity_t*>(iBoat_guess),now,points,1))VectorCopy(points,vecBoatOrigin_guess);
 if(iWheelchair_guess&&query(reinterpret_cast<cl_entity_t*>(iWheelchair_guess),now,points,1))VectorCopy(points,vecWheelchairOrigin_guess);
 if(pAttachCamEnt_guess){bAttachCam_guess=false;if(query(pAttachCamEnt_guess,now,points,4)){
  VectorCopy(points,vecCameraAttachment_guess);Vector direction(points[3]-points[0],points[4]-points[1],points[5]-points[2]);VectorAngles(direction,vecCameraAngles_guess);vecCameraAngles_guess[0]=-vecCameraAngles_guess[0];
  Vector roll(points[9]-points[0],points[10]-points[1],points[11]-points[2]);vec3_t angles;VectorAngles(roll,angles);vecCameraAngles_guess[2]=angles[0];bAttachCam_guess=true;
 }}
}

void CoFR_MagazineInfo(int id,int cap){if(id>0&&id<256&&cap>0&&cap<=128)magazineCapacity[id]=cap;}

extern char g_szDualModel_guess[64];
extern int g_iDualEntIndex_guess,g_iDualBody_guess,g_iHeadShieldType_guess;
void CoFR_PublishView(const float* origin,const float* angles){
 auto publish=renderer<int(__cdecl*)(const dx_cof_light_state_v2*)>("DX_CofLightStateV2");
 dx_cof_light_state_v2 packet;packet.time=engine.GetClientTime();packet.rawFlags=gHUD.m_iFlashFlags_guess;
 auto local=engine.GetLocalPlayer();if(!local||(healthKnown?currentHealth:gHUD.m_Health.m_iHealth)<=0)packet.reasons|=DX_COF_NO_PLAYER;
 if(gHUD.m_iViewEntity_guess&&local&&gHUD.m_iViewEntity_guess!=local->index)packet.reasons|=DX_COF_OTHER_VIEW;
 if(iBoat_guess)packet.reasons|=DX_COF_BOAT;if(iWheelchair_guess)packet.reasons|=DX_COF_WHEELCHAIR;if(pAttachCamEnt_guess)packet.reasons|=DX_COF_ATTACHED;
 if(gViewPort&&gViewPort->m_pPadlock_guess&&gViewPort->m_pPadlock_guess->isVisible())packet.reasons|=DX_COF_PUZZLE;
 auto view=engine.GetViewModel();if(!view||!view->model)packet.reasons|=DX_COF_NO_VIEWMODEL;
 for(int i=0;i<3;++i)packet.origin[i]=origin[i];engine.pfnAngleVectors(angles,packet.direction,nullptr,nullptr);packet.eligibleFlags=DX_CofEligible(packet.rawFlags,packet.reasons);if(publish)publish(&packet);
 const bool hideWeapons = (packet.reasons & (DX_COF_BOAT | DX_COF_ATTACHED |
     DX_COF_WHEELCHAIR | DX_COF_PUZZLE | DX_COF_OTHER_VIEW)) ||
     (gHUD.m_iHideHUDDisplay & HIDEHUD_WEAPONS);
 auto hidden=renderer<void(__cdecl*)(int)>("DX_SceneViewModelHiddenV1");
 if(hidden)hidden(hideWeapons ? 1 : 0);
 auto players=renderer<int(__cdecl*)(unsigned,float)>("DX_CofPlayerVisibilityV1");if(players)players((IsViewEntityActive_guess()?1u:0u)|(pAttachCamEnt_guess?2u:0u),packet.time);
 auto dual=renderer<int(__cdecl*)(const cl_entity_t*,int,float)>("DX_CofDualViewModelV1");
 if(!dual)return;
 if(hideWeapons||g_iHeadShieldType_guess!=3||g_iDualEntIndex_guess<1||!g_szDualModel_guess[0]||!strcmp(g_szDualModel_guess,"models/null.mdl")){dual(nullptr,0,packet.time);return;}
 auto source=engine.GetViewModel();if(!source){dual(nullptr,0,packet.time);return;}
 extern cl_entity_t g_HeadShieldEnt;cl_entity_t copy=*source;copy.player=0;copy.curstate.sequence=g_HeadShieldEnt.curstate.sequence;copy.curstate.animtime=g_HeadShieldEnt.curstate.animtime;copy.curstate.frame=g_HeadShieldEnt.curstate.frame;copy.curstate.framerate=1;copy.curstate.effects&=~EF_NODRAW;int index=0;copy.model=engine.CL_LoadModel(g_szDualModel_guess,&index);copy.curstate.body=g_iDualBody_guess;
 if(!copy.model){dual(nullptr,0,packet.time);return;}int active=dual(&copy,1,packet.time); // Native secondary transform reflects its right axis.
 extern int iGoro_guess;auto hands=renderer<void(__cdecl*)(int,int)>("DX_CofDualHandsV1");
 if(hands){bool filter=active&&!iGoro_guess&&gHUD.m_Ammo.m_pWeapon&&gHUD.m_Ammo.m_pWeapon->iId==15;hands(filter?(g_iDualWeapon2_guess==13?2:1):0,filter?(g_iDualWeapon1_guess==13?2:1):0);}
}
void CoFR_NativeEntityLight(cl_entity_t* entity){
 if(!entity||entity->curstate.renderfx!=71)return;auto publish=renderer<int(__cdecl*)(const dx_cof_native_light*)>("DX_CofNativeLightV1");if(!publish)return;
 dx_cof_native_light p;p.key=entity->index;p.time=engine.GetClientTime();p.range=entity->curstate.renderamt*8.f;p.cone=entity->curstate.scale;
 for(int i=0;i<3;++i){p.origin[i]=entity->origin[i];p.angles[i]=entity->curstate.angles[i];}
 p.color[0]=entity->curstate.rendercolor.r/128.f;p.color[1]=entity->curstate.rendercolor.g/128.f;p.color[2]=entity->curstate.rendercolor.b/128.f;publish(&p);
}

void CoFR_HudCap(int value){hudCap=value;}
void CoFR_LegacyVidInit(){if(panel){panel->videoReset();assetsReady=panel->ready();}maxHealth=100;hudCap=0;healthKnown=false;}

#include "usercmd.h"
#include "ref_params.h"
#include "in_buttons.h"
void CoFR_PublishCrouch(const ref_params_s* p){auto fn=renderer<int(__cdecl*)(int,float,float)>("DX_CofCrouchStateV1");if(!fn)return;if(!p||!p->cmd||p->spectator||p->intermission||p->viewentity!=p->playernum+1){fn(-1,0,p?p->time:0);return;}fn((p->cmd->buttons&IN_DUCK)?1:0,p->viewheight[2],p->time);}
bool CoFR_CustomDecal(int size,void* data){auto fn=renderer<int(__cdecl*)(const void*,int)>("DX_CofCustomDecalV1");return fn&&fn(data,size)!=0;}
