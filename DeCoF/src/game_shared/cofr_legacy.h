#pragma once
struct cl_entity_s;
void CoFR_LegacyInit();
void CoFR_LegacyFrame();
void CoFR_LegacyShutdown();
void CoFR_RefreshCamera();
bool CoFR_CustomHudActive();
bool CoFR_BlurBackground();
void CoFR_CameraFlash();
void CoFR_MagazineInfo(int id,int capacity);
void CoFR_PublishView(const float* origin,const float* angles);
void CoFR_NativeEntityLight(cl_entity_s* entity);

void CoFR_HudCap(int);
void CoFR_LegacyVidInit();

struct ref_params_s;
void CoFR_PublishCrouch(const ref_params_s*);
bool CoFR_CustomDecal(int,void*);
