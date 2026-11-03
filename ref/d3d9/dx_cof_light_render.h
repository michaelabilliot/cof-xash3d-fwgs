#pragma once
#include "dx_cof_light.h"
#include <d3d9.h>
inline bool DX_CofBuildLight(const dx_cof_light_state_v2& p,unsigned flags,float intensity,D3DLIGHT9& light,float handheldBrightness=1.f,float originAdvance=4.f) {
 if(!DX_CofValid(p)||!(flags&DX_COF_SUPPORTED_FLAGS)||!std::isfinite(intensity)||intensity<=0||!std::isfinite(originAdvance))return false;
 float length=std::sqrt(p.direction[0]*p.direction[0]+p.direction[1]*p.direction[1]+p.direction[2]*p.direction[2]);
 light={};light.Type=D3DLIGHT_SPOT;
 light.Direction={p.direction[0]/length,p.direction[1]/length,p.direction[2]/length};
 light.Position={p.origin[0]+originAdvance*light.Direction.x,p.origin[1]+originAdvance*light.Direction.y,p.origin[2]+originAdvance*light.Direction.z};
 const auto profile=DX_CofLightProfile(flags);
 if(!std::isfinite(handheldBrightness))handheldBrightness=.25f;
 const float factor=(flags&DX_COF_HANDHELD)?std::fmax(0.f,std::fmin(4.f,handheldBrightness)):1.f;
 const float scale=profile.intensity*factor;
 if(scale<=0.f)return false;
 light.Diffuse={intensity*scale,intensity*scale,intensity*scale,1};
 light.Range=700;light.Attenuation0=1;light.Attenuation2=3.f/(700.f*700.f);
 constexpr float radians=.0174532925199433f;
 light.Theta=profile.innerDegrees*radians;light.Phi=profile.outerDegrees*radians;light.Falloff=1;
 return true;
}
