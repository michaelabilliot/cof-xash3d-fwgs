#pragma once
#include "dx_camera_area_light.h"
#include <map>
#include <set>
namespace area_record {
struct Definition {remixapi_LightInfo info{};remixapi_LightInfoRectEXT rect{};};
static std::map<remixapi_LightHandle,Definition> live;
static std::set<uint64_t> hashes;
static unsigned created=0,updated=0,destroyed=0,drawn=0,serial=0;
static bool failCreate=false,failUpdate=false,failDraw=false;
static void(*observer)(const Definition*)=nullptr;
static void validate(const remixapi_LightInfo* info){
 check(info&&info->sType==REMIXAPI_STRUCT_TYPE_LIGHT_INFO&&info->isDynamic&&info->ignoreViewModel,"camera-only dynamic emitter excludes viewmodels");
 const auto rect=static_cast<const remixapi_LightInfoRectEXT*>(info->pNext);
 check(rect&&rect->sType==REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT&&rect->xSize>0&&rect->ySize>0,"small rectangular emitter, never a sphere");
 check(rect->shaping_hasvalue&&rect->shaping_value.coneAngleDegrees==60&&rect->shaping_value.focusExponent==0,"120-degree broad forward emission, flat centre");
 check(fabsf(rect->shaping_value.coneSoftness-.1427876097f)<1e-6f,"100-degree plateau and soft edge");
 check(std::isfinite(info->radiance.x)&&info->radiance.x>0&&info->radiance.x==info->radiance.y&&info->radiance.x==info->radiance.z,"finite neutral area radiance");
}
static remixapi_ErrorCode REMIXAPI_CALL create(const remixapi_LightInfo* info,remixapi_LightHandle* out){
 validate(info);if(failCreate){*out=nullptr;return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;}
 check(live.empty(),"a replacement retires the preceding emitter before creation");check(hashes.insert(info->hash).second,"deferred-destroy hashes never reused");
 *out=reinterpret_cast<remixapi_LightHandle>(uintptr_t(++serial));live[*out]={*info,*static_cast<const remixapi_LightInfoRectEXT*>(info->pNext)};++created;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL update(remixapi_LightHandle handle,const remixapi_LightInfo* info){
 validate(info);check(live.count(handle)==1,"update live camera handle");if(failUpdate)return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
 check(info->hash==live[handle].info.hash,"update retains creation hash");live[handle]={*info,*static_cast<const remixapi_LightInfoRectEXT*>(info->pNext)};++updated;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL destroy(remixapi_LightHandle handle){check(live.erase(handle)==1,"camera area retired once");++destroyed;if(observer)observer(nullptr);return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL draw(remixapi_LightHandle handle){
 check(live.count(handle)==1,"draw live camera area");if(failDraw)return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
 ++drawn;if(observer)observer(&live[handle]);return REMIXAPI_ERROR_CODE_SUCCESS;
}
}
struct CameraFlashAreaLightTestAccess {
 static void bind(CameraFlashAreaLight& light,bool updates=true){
  light.api={};light.module=GetModuleHandleA("d3d9.dll");light.api.CreateLight=area_record::create;light.api.UpdateLightDefinition=updates?area_record::update:nullptr;light.api.DestroyLight=area_record::destroy;light.api.DrawLightInstance=area_record::draw;light.updateUnavailable=false;
 }
};
