#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include "dx_camera_socket.h"
#include "dx_map_light_power.h"
#include "dx_light_lifetime.h"
namespace camera_area {
inline float shaping(float angleDegrees){
 const float c=std::cos(angleDegrees*.0174532925199433f);
 const float edge=.5f,soft=.14278760968653936f; // outer60 / inner50 degrees.
 const float t=std::clamp((c-edge)/soft,0.f,1.f);return t*t*(3.f-2.f*t);
}
// Unit-radiance irradiance on a parallel, centred reference wall. Four-point
// Gauss quadrature preserves the current central brightness at128 units.
inline double irradiance(double width,double height,double distance){
 constexpr double nodes[]={-.8611363115940526,-.3399810435848563,.3399810435848563,.8611363115940526};
 constexpr double weights[]={.3478548451374538,.6521451548625461,.6521451548625461,.3478548451374538};
 double result=0,z2=distance*distance;
 for(int x=0;x<4;++x)for(int y=0;y<4;++y){double px=width*.5*nodes[x],py=height*.5*nodes[y],r2=z2+px*px+py*py;result+=weights[x]*weights[y]*z2/(r2*r2);}
 return result*width*height*.25;
}
inline float radiance(const camera_socket::Area& area,float brightness,float conversionFactor,float legacyRadius){
 if(!std::isfinite(brightness)||brightness<=0||!std::isfinite(conversionFactor)||conversionFactor<=0||!std::isfinite(legacyRadius)||legacyRadius<0||area.width<=0||area.height<=0)return 0;
 const float radius=std::max(.001f,legacyRadius);
 const float old=dx_map_light::radiance(brightness,700.f,1.f,0.f,3.f/(700.f*700.f),radius)*conversionFactor;
 const double reference=double(old)*3.14159265358979323846*radius*radius/(128.*128.);
 const double response=irradiance(area.width,area.height,128.);
 const double value=response>0?reference/response:0;return value>0&&value<1e15&&std::isfinite(value)?float(value):0;
}
}
class CameraFlashAreaLight {
 friend struct CameraFlashAreaLightTestAccess;
 remixapi_Interface api{};HMODULE module=nullptr;ULONGLONG retryAt=0;
 remixapi_LightHandle handle=nullptr;bool updateUnavailable=false;
 uint64_t currentHash=0;
 dx_map_light::LifetimeIds lifetimes;
 remixapi_LightInfoRectEXT previous{};float previousRadiance=-1;
public:
 bool ready(){
  const auto loaded=GetModuleHandleA("d3d9.dll");
  if(loaded!=module){api={};handle=nullptr;module=loaded;retryAt=0;updateUnavailable=false;previousRadiance=-1;}
  if(api.CreateLight&&api.DrawLightInstance&&api.DestroyLight)return true;
  if(!module||GetTickCount64()<retryAt)return false;retryAt=GetTickCount64()+100;
  auto init=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary"));if(!init)return false;
  remixapi_InitializeLibraryInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
  if(init(&info,&api)!=REMIXAPI_ERROR_CODE_SUCCESS){api={};return false;}
  return api.CreateLight&&api.DrawLightInstance&&api.DestroyLight;
 }
 void retire(){
  if(handle&&module==GetModuleHandleA("d3d9.dll")&&api.DestroyLight)api.DestroyLight(handle);
  handle=nullptr;currentHash=0;previousRadiance=-1;
 }
 bool configure(const camera_socket::Area& area,float output){
  if(!std::isfinite(output)||output<=0||!ready()){retire();return false;}
  remixapi_LightInfoRectEXT rect{};rect.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT;
  rect.position={area.source[0],area.source[1],area.source[2]};rect.xAxis={area.xAxis[0],area.xAxis[1],area.xAxis[2]};rect.yAxis={area.yAxis[0],area.yAxis[1],area.yAxis[2]};
  rect.xSize=area.width;rect.ySize=area.height;rect.direction={area.normal[0],area.normal[1],area.normal[2]};rect.shaping_hasvalue=1;
  rect.shaping_value.direction=rect.direction;rect.shaping_value.coneAngleDegrees=60.f;rect.shaping_value.coneSoftness=.14278760968653936f;rect.shaping_value.focusExponent=0;
  const bool changed=!handle||output!=previousRadiance||memcmp(&rect,&previous,sizeof rect);
  if(changed){
   remixapi_LightInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;info.pNext=&rect;info.radiance={output,output,output};info.isDynamic=1;info.ignoreViewModel=1;
   remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_SUCCESS;
   if(handle&&api.UpdateLightDefinition&&!updateUnavailable){
    info.hash=currentHash;result=api.UpdateLightDefinition(handle,&info);
    if(result!=REMIXAPI_ERROR_CODE_SUCCESS){updateUnavailable=true;retire();}
   }else retire();
   if(!handle){
    info.hash=lifetimes.next(0x434f4643414dull);if(!info.hash)return false;
    result=api.CreateLight(&info,&handle);if(result==REMIXAPI_ERROR_CODE_SUCCESS&&handle)currentHash=info.hash;
   }
   if(result!=REMIXAPI_ERROR_CODE_SUCCESS||!handle){R_CofLightDebug("camera area definition failed code=%u",unsigned(result));retire();return false;}
   previous=rect;previousRadiance=output;
  }return true;
 }
 bool draw(){
  if(!handle||!api.DrawLightInstance)return false;
  const auto result=api.DrawLightInstance(handle);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS){R_CofLightDebug("camera area draw failed code=%u",unsigned(result));retire();return false;}
  R_CofLightDebug("camera area draw source=(%.3f %.3f %.3f) size=(%.3f %.3f) radiance=%.3f ignoreViewModel=1",previous.position.x,previous.position.y,previous.position.z,previous.xSize,previous.ySize,previousRadiance);return true;
 }
};
