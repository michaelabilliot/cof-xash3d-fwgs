// Included inside the production renderer fixture, after the original cases.
{
 using namespace cof_shadow_motion;
 shadowVmModel=&scope;viewmodel.model=&scope;
 auto motion=symbol<State*>("g_cofShadowMotion");
 auto motionRig=symbol<Rig*>("g_cofShadowMotionRig");
 auto viewAngles=symbol<float*>("g_vAngles");float savedAngles[3];memcpy(savedAngles,viewAngles,sizeof savedAngles);
 std::ofstream capture;if(argc>=5){capture.open(argv[4]);require(bool(capture),"pose capture output");}
 bool firstCapture=true;
 if(capture){capture<<"{\"indices\":[";for(size_t i=0;i<shadowSurfaceIndices.size();++i){if(i)capture<<',';capture<<shadowSurfaceIndices[i];}capture<<"],\"frames\":[";}
 const auto validate=[&](){
  for(const auto& bone:shadowPose){
   for(int i=0;i<3;++i)for(int j=0;j<4;++j)require(std::isfinite(bone.matrix[i][j]),"expanded pose remains finite");
   for(int i=0;i<3;++i)for(int j=0;j<3;++j){float dot=0;for(int k=0;k<3;++k)dot+=bone.matrix[k][i]*bone.matrix[k][j];require(std::fabs(dot-(i==j?1.f:0.f))<.003f,"local quaternion blending preserves rigid bones");}
  }
  require(shadowCreates==1,"all expanded poses retain one immutable mesh");
 };
 const auto record=[&](const char* label){
  validate();if(!capture)return;
  if(!firstCapture)capture<<',';firstCapture=false;
  capture<<"{\"label\":\""<<label<<"\",\"blend\":"<<motion->transition<<",\"hips\":"<<motion->hipYaw<<",\"reverse\":"<<motion->reverse<<",\"traversal\":"<<motion->traversal<<",\"positions\":[";
  for(size_t i=0;i<shadowSkinnedVertices.size();++i){if(i)capture<<',';const auto& p=shadowSkinnedVertices[i].position;capture<<'['<<p[0]<<','<<p[1]<<','<<p[2]<<']';}
  capture<<"]}";
 };
 const auto step=[&](float vx=0,float vy=0,float vz=0,bool crouch=false){
  client.time+=.05;client.simorg[0]+=vx*.05f;client.simorg[1]+=vy*.05f;client.simorg[2]+=vz*.05f;
  player.curstate.usehull=crouch?1:0;
  for(int i=0;i<3;++i)cameraOrigin[i]=client.simorg[i];cameraOrigin[2]+=crouch?12:28;
  shadow(&player,&police,&donorModel,&viewmodel);validate();
 };
 const auto ticks=[&](int n,float vx=0,float vy=0,float vz=0,bool crouch=false){for(int i=0;i<n;++i)step(vx,vy,vz,crouch);};
 const auto height=[&](){float low=1e9f,high=-1e9f;for(const auto& v:shadowSkinnedVertices){low=std::min(low,v.position[2]);high=std::max(high,v.position[2]);}return high-low;};
 const auto delta=[](const std::vector<remixapi_HardcodedVertex>& a,const std::vector<remixapi_HardcodedVertex>& b){float largest=0;require(a.size()==b.size(),"immutable pose topology");for(size_t i=0;i<a.size();++i)for(int j=0;j<3;++j)largest=std::max(largest,std::fabs(a[i].position[j]-b[i].position[j]));return largest;};
 viewAngles[0]=viewAngles[1]=viewAngles[2]=0;player.curstate.movetype=MOVETYPE_WALK;player.curstate.onground=0;
 loadVm("models/weapons/famas/v_famas.mdl","idle");ticks(16);record("standing");
 const float standingHeight=height();
 step(0,0,0,true);const float firstDuckHeight=height();require(motion->transition>0&&motion->transition<1&&motion->duration==.28f,"crouch begins with an intermediate blend");record("crouch-050ms");
 for(int i=0;i<5;++i){step(0,0,0,true);record(i==0?"crouch-100ms":i==1?"crouch-150ms":i==2?"crouch-200ms":i==3?"crouch-250ms":"crouch-300ms");}
 const float crouchHeight=height();require(standingHeight>firstDuckHeight&&firstDuckHeight>crouchHeight+1&&crouchHeight<standingHeight-8,"Simon actually lowers through intermediate crouch heights");
 {float low=1e9f,high=-1e9f;for(int i=0;i<160;++i){step(0,0,0,true);const float z=shadowPose[1].matrix[2][3]+shadowInstance.transform.matrix[2][3];low=std::min(low,z);high=std::max(high,z);}require(high-low<.01f,"settled crouch holds its pelvis height across multiple idle cycles");
 low=1e9f;high=-1e9f;for(int i=0;i<120;++i){viewAngles[1]=std::sin(i*.1f)*90;step(0,0,0,true);const float z=shadowPose[1].matrix[2][3]+shadowInstance.transform.matrix[2][3];low=std::min(low,z);high=std::max(high,z);}
 fprintf(stdout,"PASS: crouched turning pelvis excursion %.6f units (old renderer: 4.62885); planted feet prevent hip pumping.\n",high-low);
 require(high-low<.01f,"crouched camera turns cannot trigger standing foot shuffles or hip pumping");viewAngles[1]=0;ticks(16,0,0,0,true);}
 step();require(motion->transition>0&&motion->transition<1&&height()>crouchHeight&&height()<standingHeight,"uncrouching also passes through intermediate geometry");record("stand-up-050ms");ticks(8);record("stand-up-complete");
 step(0,0,0,true);step(0,0,0,true);auto interrupted=shadowSkinnedVertices;step();require(delta(interrupted,shadowSkinnedVertices)<10,"reversing crouch mid-transition stays continuous");ticks(10);
 {
  using Publish=int(__cdecl*)(int,float,float);auto publish=reinterpret_cast<Publish>(GetProcAddress(dll,"DX_CofCrouchStateV1"));require(publish!=nullptr,"native crouch publication export");
  void(*publishFrame)(const ref_params_t*)=nullptr;
  if(argc==6){
   const DWORD64 rendererBase=moduleBase;const std::string folder=std::string(argv[2])+"/..";SetDllDirectoryA(folder.c_str());auto clientModule=LoadLibraryA(argv[5]);SetDllDirectoryA(nullptr);require(clientModule!=nullptr,"candidate client crouch publisher loads without game startup");
   const std::string file=argv[5];SymSetSearchPath(process,file.substr(0,file.find_last_of("/\\")).c_str());moduleBase=SymLoadModuleEx(process,nullptr,argv[5],nullptr,reinterpret_cast<DWORD64>(clientModule),0,nullptr,0);require(moduleBase!=0,"client publisher PDB");
   publishFrame=symbol<void(*)(const ref_params_t*)>("client!publishCrouchSignal");moduleBase=rendererBase;
  }
  const auto nativeInput=[&](bool held,float height){
   if(publishFrame){ref_params_t params{};usercmd_t command{};command.buttons=held?IN_DUCK:0;params.cmd=&command;params.time=float(client.time+.05);params.viewheight[2]=height;params.viewentity=1;
    const auto before=params;const auto original=command;publishFrame(&params);require(!memcmp(&before,&params,sizeof params)&&!memcmp(&original,&command,sizeof command),"compiled client publication does not alter movement or view parameters");}
   else require(publish(held?1:0,height,float(client.time+.05))==1,"raw crouch input published");
  };
  const float cameraHeights[]={28,24,17,12,28,25,12,28,12,12,12,12};
  bool oldState=false;int oldReversals=0;float lastBlend=0;
  for(float eyeHeight:cameraHeights){
   const bool next=crouchPose(0,eyeHeight,oldState);if(oldState&&!next)++oldReversals;oldState=next;
   nativeInput(true,eyeHeight);step();
   require(motion->crouched&&shadowEntity->curstate.gaitsequence==choose(donorHdr,"crouch_idle"),"held crouch never selects standing when the camera briefly rises");
   require(motion->transition+.0001f>=lastBlend,"held crouch transition never restarts or reverses during camera jitter");lastBlend=motion->transition;
  }
  require(oldReversals>=2,"regression input reproduces the old crouch-stand-crouch detector failure");
  nativeInput(false,12);step();require(motion->crouched,"released input retains a physically low/blocked crouch");
  nativeInput(false,28);step();require(!motion->crouched&&motion->transition<1,"standing begins only after actual release and standing eye height");
  for(int i=0;i<8;++i){nativeInput(false,28);step();}require(motion->transition==1,"release completes one stable stand-up transition");
  bool held=false;float eyeHeight=28;auto sample=symbol<bool(*)(float,bool&,float&)>("R_CofShadowCrouchInput");
  require(!sample(float(client.time+.5),held,eyeHeight),"stale crouch publication expires");require(!publish(1,NAN,float(client.time)),"invalid native crouch height rejected");publish(-1,0,float(client.time));
  if(publishFrame){publishFrame(nullptr);require(!sample(float(client.time),held,eyeHeight),"null view clears published crouch");}
  puts("PASS: raw crouch command/unbobbed height prevents crouch-stand-crouch reversal across camera spikes; release/blocked duck/stale/invalid/reset paths pass; native params preserved.");
 }
 ticks(16,0,100);require(motion->hipYaw>65,"left strafe steers articulated lower body");record("strafe-left");
 ticks(20,0,-100);require(motion->hipYaw<-65,"right strafe steers articulated lower body");record("strafe-right");
 ticks(16,-100);require(motion->reverse==-1&&shadowEntity->curstate.gaitsequence==choose(donorHdr,"walk"),"backward walk reverses gait cycle");record("backward-walk");
 ticks(16,-200);require(motion->reverse==-1&&shadowEntity->curstate.gaitsequence==choose(donorHdr,"run"),"backward run reverses gait cycle");record("backward-run");
 ticks(16,0,60,0,true);record("crouch-strafe-left");require(motion->hipYaw>65,"crouch left strafe");
 ticks(20,0,-60,0,true);record("crouch-strafe-right");require(motion->hipYaw<-65&&height()<standingHeight-12,"crouch right strafe retains a low torso");
 ticks(16,-60,0,0,true);record("crouch-backward");require(motion->reverse==-1,"crouch backward cycle");ticks(12);
 viewAngles[1]=90;step();require(std::fabs(angle(motion->bodyYaw-90))>30&&std::fabs(motion->turnSpeed)>10,"turning body follows view over time instead of snapping");ticks(8);record("turn-in-place");
 viewAngles[1]=0;ticks(20);viewAngles[0]=-60;ticks(16);require(motion->aimPitch<-55,"aim up follows camera pitch");record("aim-up");auto aimingUp=shadowSkinnedVertices;
 const float upHandHeight=shadowPose[hand].matrix[2][3]-shadowPose[spine].matrix[2][3];
 viewAngles[0]=60;ticks(20);require(motion->aimPitch>55&&delta(aimingUp,shadowSkinnedVertices)>8,"aim down produces distinct articulated geometry");
 record("aim-down");require(upHandHeight>shadowPose[hand].matrix[2][3]-shadowPose[spine].matrix[2][3]+10,"upward aim raises the hand; downward aim lowers it in the actual rig");viewAngles[0]=0;ticks(20);
 struct ProfileCase {const char* model;const char* clip;cof_shadow_animation::Profile profile;const char* label;};
 const ProfileCase profiles[]={
  {"models/weapons/famas/v_famas.mdl","idle",cof_shadow_animation::Profile::Famas,"FAMAS"},
  {"models/weapons/ooi_ak47/v_ooi_ak47.mdl","idle_single",cof_shadow_animation::Profile::AK47,"AK47"},
  {"models/weapons/mp5/v_mp5.mdl","idle",cof_shadow_animation::Profile::MP5,"MP5"},
  {"models/weapons/m76/v_m76.mdl","idle",cof_shadow_animation::Profile::M76,"M76"},
  {"models/weapons/g43/v_g43.mdl","idle",cof_shadow_animation::Profile::G43,"G43"},
  {"models/weapons/browning/v_wheelchair_browning.mdl","idle",cof_shadow_animation::Profile::Browning,"Browning"},
  {"models/weapons/axe/v_axe.mdl","idle",cof_shadow_animation::Profile::Axe,"axe"},
  {"models/weapons/sledgeshovel/v_sledgeshovel.mdl","idle",cof_shadow_animation::Profile::Shovel,"sledge-shovel"},
  {"models/weapons/mobile/v_mobile.mdl","idle_flash",cof_shadow_animation::Profile::Phone,"phone"},
  {"models/weapons/flashlight/v_flashlight.mdl","idle",cof_shadow_animation::Profile::Flashlight,"flashlight"},
  {"models/weapons/lantern/v_lantern.mdl","idle",cof_shadow_animation::Profile::Lantern,"lantern"},
  {"models/weapons/camera/v_camera.mdl","idle",cof_shadow_animation::Profile::Camera,"camera"}
 };
 std::vector<remixapi_HardcodedVertex> famasPose;
 for(const auto& profile:profiles){loadVm(profile.model,profile.clip);ticks(18);require(cof_shadow_animation::select(profile.model,profile.clip).profile==profile.profile,"actual VM gets distinct weapon/portable profile");record(profile.label);
  if(profile.profile==cof_shadow_animation::Profile::Famas)famasPose=shadowSkinnedVertices;
  if(profile.profile==cof_shadow_animation::Profile::AK47)require(delta(famasPose,shadowSkinnedVertices)>.25f,"AK and FAMAS have distinct procedural arm poses");}
 actionCase("models/weapons/ooi_ak47/v_ooi_ak47.mdl","shoot_single","m16_shoot");ticks(8);
 actionCase("models/weapons/mp5/v_mp5.mdl","reload_regular","tmp_reload");ticks(8);
 actionCase("models/weapons/m76/v_m76.mdl","shoot","tmp_shoot");ticks(8);
 actionCase("models/weapons/g43/v_g43.mdl","fire1","rifle_shoot");ticks(8);
 actionCase("models/weapons/browning/v_wheelchair_browning.mdl","shoot","pistol_shoot");ticks(8);
 actionCase("models/weapons/axe/v_axe.mdl","attack","sledgehammer_attack");ticks(8);
 actionCase("models/weapons/sledgeshovel/v_sledgeshovel.mdl","attack","sledgehammer_attack");ticks(8);
 for(const auto& item:std::array<std::pair<const char*,const char*>,3>{{
  {"models/weapons/mobile/v_mobile.mdl","flash_looksms"},
  {"models/weapons/flashlight/v_flashlight.mdl","switch"},
  {"models/weapons/camera/v_camera.mdl","shoot"}}}){
  loadVm(item.first,item.second);ticks(8);require(motion->gesture==cof_shadow_animation::Gesture::Use,"actual portable-item use clip drives gesture");}
 loadVm("models/weapons/famas/v_famas.mdl","draw");viewmodel.curstate.frame=0;step();require(motion->gesture==cof_shadow_animation::Gesture::Draw&&motion->transition<1,"draw enters blended gesture");ticks(8);record("draw-start");viewmodel.curstate.frame=255;ticks(8);record("draw-finish");
 loadVm("models/weapons/famas/v_famas.mdl","holster");viewmodel.curstate.frame=0;ticks(8);auto holsterStart=shadowSkinnedVertices;viewmodel.curstate.frame=255;ticks(8);require(delta(holsterStart,shadowSkinnedVertices)>4,"holster lowers arms through actual VM timing");record("holster-finish");
 loadVm("models/weapons/flare/v_flare.mdl","flare_idle");ticks(18);record("flare-hold");
 loadVm("models/weapons/flare/v_flare.mdl","flare_ignite");for(int i=0;i<5;++i){viewmodel.curstate.frame=std::min(255.f,i*64.f);ticks(7);record(i==0?"flare-ignite-start":i==2?"flare-ignite-middle":"flare-ignite");}
 require(motion->gesture==cof_shadow_animation::Gesture::Ignite,"real flare ignite label animates ignition gesture");
 loadVm("models/weapons/flare/v_flare.mdl","flare_throw");viewmodel.curstate.frame=0;ticks(7);auto throwStart=shadowSkinnedVertices;record("flare-throw-start");viewmodel.curstate.frame=128;ticks(7);auto throwWindup=shadowSkinnedVertices;record("flare-throw-middle");viewmodel.curstate.frame=255;ticks(7);record("flare-throw-finish");require(delta(throwStart,throwWindup)>10&&delta(throwWindup,shadowSkinnedVertices)>12,"flare throw has distinct windup and follow-through geometry");
 loadVm("models/weapons/glock/v_glock.mdl","idle1");ticks(18);
 using DualPublish=int(__cdecl*)(const cl_entity_t*,int,float);auto publishDual=reinterpret_cast<DualPublish>(GetProcAddress(dll,"DX_CofDualViewModelV1"));require(publishDual!=nullptr,"dual viewmodel publication available");
 cl_entity_t second{};secondaryVmModel.type=mod_studio;second.model=&secondaryVmModel;second.curstate.framerate=0;second.curstate.frame=64;
 const auto dual=[&](const char* model,bool crouch,const char* label){
  strcpy_s(secondaryVmModel.name,model);std::ifstream file(std::string(argv[2])+"/"+model,std::ios::binary);secondaryVmBytes.assign(std::istreambuf_iterator<char>(file),{});require(secondaryVmBytes.size()>sizeof(studiohdr_t),"real secondary VM loaded");
  for(int i=0;i<18;++i){require(publishDual(&second,1,float(client.time+.05))==1,"secondary VM published");step(0,0,0,crouch);}
  require(motion->secondary!=0,"secondary hand profile active");record(label);
  Matrix donorWorld[Limit]{};for(int i=0;i<motionRig->count;++i){Matrix local;matrix(motion->displayed[i],local);const int parent=motionRig->parents[i];if(parent>=0)multiply(donorWorld[parent],local,donorWorld[i]);else memcpy(donorWorld[i],local,sizeof(Matrix));}
  float forward[3];anatomicalAxis(*motionRig,donorWorld,true,0,forward);float ahead=0;for(int i=0;i<3;++i)ahead+=(donorWorld[motionRig->left[3]][i][3]-donorWorld[motionRig->neck][i][3])*forward[i];
  require(ahead>2,"mirrored secondary hand stays in front of torso, not behind the back");
  for(int jointIndex=1;jointIndex<4;++jointIndex){const int bone=motionRig->left[jointIndex],parent=motionRig->parents[bone];const auto& a=motion->displayed[bone].p;const auto& b=motion->displayed[parent].p;(void)b;const float length=std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);require(length<20&&std::isfinite(length),"dual arm remains attached with plausible segment lengths");}
 };
 dual("models/weapons/glock/v_glock.mdl",false,"dual-pistols");
 dual("models/weapons/mobile/v_mobile.mdl",false,"gun-phone");dual("models/weapons/mobile/v_mobile.mdl",true,"gun-phone-crouched");
 dual("models/weapons/flashlight/v_flashlight.mdl",false,"gun-flashlight");
 publishDual(nullptr,0,float(client.time));ticks(12);require(motion->secondary==0,"stowing secondary blends back to ordinary pose");
 player.curstate.movetype=MOVETYPE_FLY;ticks(18,0,0,65);require(motion->traversal==1,"native ladder movement selects climbing pose");record("ladder-up");auto climb=shadowSkinnedVertices;ticks(5,0,0,65);require(delta(climb,shadowSkinnedVertices)>1,"ladder arms and legs actually alternate");
 ticks(10,0,0,-65);record("ladder-down");player.curstate.movetype=MOVETYPE_WALK;player.curstate.onground=-1;testWater=3;ticks(18,70);require(motion->traversal==2,"native water level selects swim");record("swimming");ticks(18);record("treading-water");
 testWater=0;player.curstate.onground=0;ticks(20);record("returned-to-standing");
 require(motion->traversal==0&&motion->transition==1,"traversal exit settles cleanly");
 if(capture){capture<<"]}";capture.close();}
 memcpy(viewAngles,savedAngles,sizeof savedAngles);secondaryVmBytes.clear();shadowVmModel=nullptr;shadowVmBytes.clear();player.curstate.usehull=0;
 puts("PASS: production private-body motion: intermediate crouch/stand and reversal, directional walk/run/crouch, gradual turning, up/down aim, distinct weapon/portable profiles, draw/holster, flare gestures, independent dual arms including crouch, ladder/swim/tread, finite rigid bones and one immutable mesh. Offline recording only; no game/device.");
}
