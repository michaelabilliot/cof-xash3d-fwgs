#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
// The installed x86 bridge hangs on mesh extensions. Keep mesh pNext null;
// animate immutable bone-local vertices through DrawInstance bone transforms.
class CofShadowMesh {
 friend struct CofShadowMeshTestAccess;
 remixapi_Interface api{};HMODULE module=nullptr;remixapi_MeshHandle mesh=nullptr;
 remixapi_MaterialHandle material=nullptr;ULONGLONG retryAt=0;bool configured=false,disabled=false,reported=false;uint64_t lifetime=1;
 std::vector<remixapi_HardcodedVertex> floorVertices;std::vector<uint32_t> floorBones,floorIndices;
 uint64_t meshHash=0;
public:
 explicit CofShadowMesh(uint64_t salt=0):lifetime(1+salt){}
 bool fail(const char* reason) {
  disabled=true;
  if(!reported){reported=true;if(gEngfuncs.Con_Printf)gEngfuncs.Con_Printf("D3D9: player shadow disabled: %s\n",reason);}
  return false;
 }
 bool ready() {
  HMODULE current=GetModuleHandleA("d3d9.dll");
  if(current!=module){api={};mesh=nullptr;material=nullptr;configured=false;disabled=false;reported=false;module=current;retryAt=0;}
  if(disabled)return false;
  if(api.CreateMesh&&api.DrawInstance&&api.DestroyMesh&&api.CreateMaterial&&api.DestroyMaterial&&api.SetConfigVariable)return true;
  if(!module||GetTickCount64()<retryAt)return false;retryAt=GetTickCount64()+1000;
  auto init=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary"));
  if(!init)return fail("Remix API unavailable");
  remixapi_InitializeLibraryInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
  info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
  const auto result=init(&info,&api);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS){api={};return result==REMIXAPI_ERROR_CODE_NOT_INITIALIZED?false:fail("incompatible Remix API");}
  if(!api.CreateMesh||!api.DrawInstance||!api.DestroyMesh||!api.CreateMaterial||!api.DestroyMaterial||!api.SetConfigVariable)
   return fail("required skinned-instance API unavailable");
  return true;
 }
 bool available()const{return mesh!=nullptr;}
 void reset() {
  if(module==GetModuleHandleA("d3d9.dll")) {
   if(mesh&&api.DestroyMesh)api.DestroyMesh(mesh);
   if(material&&api.DestroyMaterial)api.DestroyMaterial(material);
  }
  mesh=nullptr;material=nullptr;configured=false;floorVertices.clear();floorBones.clear();floorIndices.clear();meshHash=0;++lifetime;
 }
 bool create(const std::vector<remixapi_HardcodedVertex>& vertices,const std::vector<uint32_t>& indices,
             const std::vector<uint32_t>& boneIndices,unsigned boneCount,bool skinned=true) {
  if(!ready())return false;
  if(mesh)return true;
  if(vertices.empty()||vertices.size()>100000||indices.empty()||indices.size()>600000||indices.size()%3||
     boneIndices.size()!=vertices.size()||boneCount==0||boneCount>128)return fail("invalid shadow mesh");
  for(auto i:indices)if(i>=vertices.size())return fail("invalid shadow indices");
  for(auto bone:boneIndices)if(bone>=boneCount)return fail("invalid shadow bone");
  if(!configured&&api.SetConfigVariable) {
   const auto hide=api.SetConfigVariable("rtx.playerModel.enableInPrimarySpace","False");
   const auto shadows=api.SetConfigVariable("rtx.playerModel.enablePrimaryShadows","True");
   configured=hide==REMIXAPI_ERROR_CODE_SUCCESS&&shadows==REMIXAPI_ERROR_CODE_SUCCESS;
  }
  if(!configured)return fail("player visibility configuration rejected");
  if(!material&&api.CreateMaterial) {
   remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
   opaque.albedoConstant={.12f,.12f,.12f};opaque.opacityConstant=1.f;opaque.roughnessConstant=1.f;
   // Remix's explicit material alpha comparison is Vulkan-valued: zero means
   // NEVER, not disabled. The default-zero struct otherwise discards every ray hit.
   opaque.alphaTestType=7; // VK_COMPARE_OP_ALWAYS
   opaque.useDrawCallAlphaState=0;opaque.blendType_hasvalue=0;
   remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;info.hash=0x434f46524d000000ull+lifetime;
   if(api.CreateMaterial(&info,&material)!=REMIXAPI_ERROR_CODE_SUCCESS||!material)return fail("shadow material rejected");
  }
  remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=vertices.data();surface.vertices_count=vertices.size();
  surface.indices_values=indices.data();surface.indices_count=indices.size();surface.material=material;
  std::vector<float> weights(vertices.size(),1.f);
  surface.skinning_hasvalue=skinned?1:0;surface.skinning_value.bonesPerVertex=1;
  surface.skinning_value.blendWeights_values=weights.data();surface.skinning_value.blendWeights_count=static_cast<uint32_t>(weights.size());
  surface.skinning_value.blendIndices_values=boneIndices.data();surface.skinning_value.blendIndices_count=static_cast<uint32_t>(boneIndices.size());
  remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;
  info.hash=0x434f465242000000ull+lifetime;info.surfaces_values=&surface;info.surfaces_count=1;
  remixapi_MeshHandle next=nullptr;
  if(api.CreateMesh(&info,&next)!=REMIXAPI_ERROR_CODE_SUCCESS||!next)return fail("skinned shadow mesh rejected");
  mesh=next;meshHash=info.hash;floorVertices=vertices;floorBones=boneIndices;floorIndices=indices;return true;
 }
 void status()const {
  char path[MAX_PATH]{};if(module)GetModuleFileNameA(module,path,MAX_PATH);
  gEngfuncs.Con_Printf("SHADOW API module=%s mesh=%p material=%p hash=%08X%08X configured=%d disabled=%d vertices=%u indices=%u\n",path,mesh,material,unsigned(meshHash>>32),unsigned(meshHash),int(configured),int(disabled),unsigned(floorVertices.size()),unsigned(floorIndices.size()));
 }
 bool probe(CofShadowMesh& other,int kind,const remixapi_Transform* bones,unsigned count)const {
  if(other.available())return true;
  if(kind==1){
   std::vector<remixapi_HardcodedVertex> v(8);std::vector<uint32_t> bi(8,0);
   for(unsigned i=0;i<8;++i){v[i].position[0]=(i&1)?8.f:-8.f;v[i].position[1]=(i&2)?8.f:-8.f;v[i].position[2]=(i&4)?48.f:0.f;v[i].normal[2]=1;v[i].color=0xffffffff;}
   const std::vector<uint32_t> ix={0,1,3,0,3,2,4,6,7,4,7,5,0,4,5,0,5,1,2,3,7,2,7,6,0,2,6,0,6,4,1,5,7,1,7,3};
   return other.create(v,ix,bi,1,false);
  }
  if(kind==3)return other.create(floorVertices,floorIndices,floorBones,count);
  auto v=floorVertices;std::vector<uint32_t> bi(v.size(),0);
  for(size_t i=0;i<v.size();++i){if(floorBones[i]>=count)return false;const auto& m=bones[floorBones[i]].matrix;
   for(int r=0;r<3;++r){v[i].position[r]=m[r][3];v[i].normal[r]=0;for(int c=0;c<3;++c){v[i].position[r]+=m[r][c]*floorVertices[i].position[c];v[i].normal[r]+=m[r][c]*floorVertices[i].normal[c];}}
   float n=std::sqrt(v[i].normal[0]*v[i].normal[0]+v[i].normal[1]*v[i].normal[1]+v[i].normal[2]*v[i].normal[2]);if(n>0)for(float& x:v[i].normal)x/=n;
  }
  return other.create(v,floorIndices,bi,1,false);
 }
 bool bounds(const D3DMATRIX& world,const remixapi_Transform* bones,unsigned count,float* lo,float* hi)const {
  for(int r=0;r<3;++r){lo[r]=1e9f;hi[r]=-1e9f;}
  for(size_t i=0;i<floorVertices.size();++i){if(floorBones[i]>=count)return false;const auto& b=bones[floorBones[i]].matrix;float p[3]{};
   for(int r=0;r<3;++r){p[r]=b[r][3];for(int c=0;c<3;++c)p[r]+=b[r][c]*floorVertices[i].position[c];}
   for(int r=0;r<3;++r){float v=world.m[3][r];for(int c=0;c<3;++c)v+=world.m[c][r]*p[c];if(!std::isfinite(v))return false;lo[r]=std::min(lo[r],v);hi[r]=std::max(hi[r],v);}
  }return !floorVertices.empty();
 }
 // Donor weapon/locomotion sequences use different vertical origins. Anchor
 // the posed sole, not the unanimated MDL bounds, to the collision-hull bottom.
 bool floor(const remixapi_Transform* bones,unsigned count,float& z)const {
  if(!bones||floorVertices.empty()||floorVertices.size()!=floorBones.size())return false;
  z=1e9f;
  for(size_t i=0;i<floorVertices.size();++i) {
   if(floorBones[i]>=count)return false;
   const auto& m=bones[floorBones[i]].matrix;const auto& p=floorVertices[i].position;
   const float v=m[2][0]*p[0]+m[2][1]*p[1]+m[2][2]*p[2]+m[2][3];
   if(!std::isfinite(v))return false;z=std::min(z,v);
  }
  return std::isfinite(z)&&std::fabs(z)<512.f;
 }
 bool draw(const D3DMATRIX& world,const remixapi_Transform* bones,unsigned boneCount,bool visible=false,bool skinned=true) {
  if(!ready()||!mesh||!configured)return false;
  if(skinned&&(!bones||!boneCount||boneCount>128))return fail("invalid shadow pose");
  remixapi_InstanceInfoBoneTransformsEXT pose{};pose.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;
  pose.boneTransforms_values=bones;pose.boneTransforms_count=boneCount;
  remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.mesh=mesh;instance.pNext=skinned?&pose:nullptr;
  instance.categoryFlags=visible?0:REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_MODEL|REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_BODY;
  instance.doubleSided=1;
  for(int row=0;row<3;++row)for(int col=0;col<4;++col)instance.transform.matrix[row][col]=world.m[col][row];
  if(api.DrawInstance(&instance)!=REMIXAPI_ERROR_CODE_SUCCESS)return fail("skinned shadow instance rejected");
  return true;
 }
};
