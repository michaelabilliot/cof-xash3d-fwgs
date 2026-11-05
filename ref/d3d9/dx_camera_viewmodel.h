#pragma once
// Mirror Remix's viewmodel instance correction without changing its settings.
// Row-vector equivalent of rtx_instance_manager.cpp's perspectiveCorrection.
struct dx_camera_viewmodel_settings {bool enabled=false,correction=true;float scale=1,conversionFactor=1,legacyRadius=4;};
static dx_camera_viewmodel_settings g_cameraViewModelSettings;
static bool g_cameraViewModelSettingsLoaded=false;
void R_RefreshCofCameraViewModelSettings(){
 g_cameraViewModelSettings={};g_cameraViewModelSettingsLoaded=true;
 wchar_t path[MAX_PATH]{};const auto module=GetModuleHandleA("ref_d3d9.dll");
 if(!module||!GetModuleFileNameW(module,path,MAX_PATH))return;
 std::wstring directory(path);const auto slash=directory.find_last_of(L"\\/");if(slash==std::wstring::npos)return;directory.resize(slash+1);
 for(const auto name:{L"rtx.conf",L"user.conf"}){
  FILE* file=nullptr;if(_wfopen_s(&file,(directory+name).c_str(),L"rb")||!file)continue;
  char line[4096];unsigned lines=0;
  while(lines++<8192&&fgets(line,sizeof line,file)){
   std::string text(line);const auto equals=text.find('=');if(equals==std::string::npos)continue;
   auto key=text.substr(0,equals),value=text.substr(equals+1);
   const auto trim=[](std::string& s){const auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");s=a==std::string::npos?std::string():s.substr(a,b-a+1);};trim(key);trim(value);
   if(key=="rtx.viewModel.enable")g_cameraViewModelSettings.enabled=!Q_stricmp(value.c_str(),"True");
   if(key=="rtx.viewModel.perspectiveCorrection")g_cameraViewModelSettings.correction=!Q_stricmp(value.c_str(),"True");
   if(key=="rtx.lightConversionIntensityFactor"||key=="rtx.lightConversionSphereLightFixedRadius"){char* end=nullptr;const float number=strtof(value.c_str(),&end);if(end!=value.c_str()&&std::isfinite(number)&&number>=0){if(key=="rtx.lightConversionIntensityFactor")g_cameraViewModelSettings.conversionFactor=number;else g_cameraViewModelSettings.legacyRadius=number;}}
   if(key=="rtx.viewModel.scale"){char* end=nullptr;const float scale=strtof(value.c_str(),&end);if(end!=value.c_str()&&std::isfinite(scale)&&scale>0&&scale<=100)g_cameraViewModelSettings.scale=scale;}
  }fclose(file);
 }
 R_CofLightDebug("camera Remix viewmodel settings enabled=%d perspective=%d scale=%.6f",int(g_cameraViewModelSettings.enabled),int(g_cameraViewModelSettings.correction),g_cameraViewModelSettings.scale);
}
float R_CofCameraConversionFactor(){return g_cameraViewModelSettings.conversionFactor;}
float R_CofCameraLegacyRadius(){return g_cameraViewModelSettings.legacyRadius;}
bool R_CofCameraViewModelCorrection(const float* origin,const float* angles,D3DMATRIX& output){
 if(!g_cameraViewModelSettingsLoaded)R_RefreshCofCameraViewModelSettings();
 D3DXMATRIX result;D3DXMatrixIdentity(&result);
 if(!g_cameraViewModelSettings.enabled){output=result;return true;}
 if(!g_sceneViewportValid||!g_sceneViewport.Height||!g_sceneViewport.Width)return false;
 if(!origin||!angles)return false;
 vec3_t forward,up;AngleVectors(angles,forward,nullptr,up);
 const D3DXVECTOR3 eye(origin[0],origin[1],origin[2]),at(origin[0]+forward[0],origin[1]+forward[1],origin[2]+forward[2]),vertical(up[0],up[1],up[2]);
 D3DXMATRIX view,fix,invView,invProjection;D3DXMatrixLookAtRH(&view,&eye,&at,&vertical);D3DXMatrixIdentity(&fix);fix._33=-1;view*=fix;
 if(!D3DXMatrixInverse(&invView,nullptr,&view)||!D3DXMatrixInverse(&invProjection,nullptr,&g_sceneProjection))return false;
 D3DXMATRIX weapon;
 D3DXMatrixPerspectiveFovLH(&weapon,D3DXToRadian(std::clamp(r_viewmodel_fov.value,10.f,170.f)),
  float(g_sceneViewport.Width)/float(g_sceneViewport.Height),std::clamp(r_viewmodel_near.value,.01f,16.f),8192);
 // Remix retains world near/far coefficients; only weapon XY/FOV changes.
 weapon._33=g_sceneProjection._33;weapon._34=g_sceneProjection._34;weapon._43=g_sceneProjection._43;
 D3DXMATRIX scale;D3DXMatrixScaling(&scale,g_cameraViewModelSettings.scale,g_cameraViewModelSettings.scale,g_cameraViewModelSettings.scale);
 if(!g_cameraViewModelSettings.correction)weapon=g_sceneProjection;
 result=view*scale*weapon*invProjection*invView;
 for(int row=0;row<4;++row)for(int col=0;col<4;++col)if(!std::isfinite(result.m[row][col]))return false;
 output=result;return true;
}
