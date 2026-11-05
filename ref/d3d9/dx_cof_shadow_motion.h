#pragma once
#include "dx_cof_shadow_animation.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace cof_shadow_motion {
constexpr int Limit=128;
constexpr float Pi=3.14159265358979323846f;
using Matrix=float[3][4];
inline float radians(float x){return x*Pi/180.f;}
inline float angle(float x){return std::remainder(x,360.f);}
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline void multiply(const Matrix& a,const Matrix& b,Matrix& out){
 Matrix temp{};for(int r=0;r<3;++r)for(int c=0;c<4;++c){temp[r][c]=c==3?a[r][3]:0;for(int k=0;k<3;++k)temp[r][c]+=a[r][k]*b[k][c];}std::memcpy(out,temp,sizeof(Matrix));
}
inline void inverse(const Matrix& in,Matrix& out){
 Matrix temp{};for(int r=0;r<3;++r){for(int c=0;c<3;++c)temp[r][c]=in[c][r];for(int k=0;k<3;++k)temp[r][3]-=temp[r][k]*in[k][3];}std::memcpy(out,temp,sizeof(Matrix));
}
struct Joint {std::array<float,4> q{0,0,0,1};std::array<float,3> p{};};
inline Joint joint(const Matrix& m){
 Joint j;for(int i=0;i<3;++i)j.p[i]=m[i][3];
 const float trace=m[0][0]+m[1][1]+m[2][2];
 if(trace>0){const float s=2*std::sqrt(std::max(0.f,trace+1));j.q={(m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,s*.25f};}
 else {int i=0;if(m[1][1]>m[0][0])i=1;if(m[2][2]>m[i][i])i=2;const int k=(i+1)%3,l=(i+2)%3;
  const float s=2*std::sqrt(std::max(.000001f,1+m[i][i]-m[k][k]-m[l][l]));
  j.q[i]=s*.25f;j.q[k]=(m[k][i]+m[i][k])/s;j.q[l]=(m[l][i]+m[i][l])/s;j.q[3]=(m[l][k]-m[k][l])/s;
 }
 float length=0;for(float x:j.q)length+=x*x;length=std::sqrt(length);for(auto& x:j.q)x/=std::max(length,.000001f);return j;
}
inline void matrix(const Joint& j,Matrix& m){
 const float x=j.q[0],y=j.q[1],z=j.q[2],w=j.q[3];
 m[0][0]=1-2*(y*y+z*z);m[0][1]=2*(x*y-z*w);m[0][2]=2*(x*z+y*w);
 m[1][0]=2*(x*y+z*w);m[1][1]=1-2*(x*x+z*z);m[1][2]=2*(y*z-x*w);
 m[2][0]=2*(x*z-y*w);m[2][1]=2*(y*z+x*w);m[2][2]=1-2*(x*x+y*y);
 for(int i=0;i<3;++i)m[i][3]=j.p[i];
}
inline Joint blend(const Joint& a,const Joint& b,float t){
 Joint out;float dot=0;auto q=b.q;for(int i=0;i<4;++i)dot+=a.q[i]*q[i];if(dot<0){dot=-dot;for(auto& v:q)v=-v;}
 float left=1-t,right=t;if(dot<.9995f){const float theta=std::acos(std::clamp(dot,0.f,1.f));const float divisor=std::sin(theta);left=std::sin((1-t)*theta)/divisor;right=std::sin(t*theta)/divisor;}
 float length=0;for(int i=0;i<4;++i){out.q[i]=a.q[i]*left+q[i]*right;length+=out.q[i]*out.q[i];}
 for(auto& v:out.q)v/=std::sqrt(std::max(length,.000001f));for(int i=0;i<3;++i)out.p[i]=a.p[i]+t*(b.p[i]-a.p[i]);return out;
}
struct Rig {
 int count=0;std::array<int,Limit> parents{};
 int root=-1,pelvis=-1,neck=-1,head=-1;
 Matrix torsoInverseBind{{1,0,0,0},{0,1,0,0},{0,0,1,0}};
 std::array<int,4> spine{{-1,-1,-1,-1}},leftLeg{{-1,-1,-1,-1}},rightLeg{{-1,-1,-1,-1}};
 std::array<int,8> left{{-1,-1,-1,-1,-1,-1,-1,-1}},right{{-1,-1,-1,-1,-1,-1,-1,-1}};
 bool descendant(int bone,int ancestor)const{if(ancestor<0)return false;for(int n=0;n<count&&bone>=0;++n){if(bone==ancestor)return true;bone=parents[bone];}return false;}
 bool valid()const{if(count<=0||count>Limit)return false;for(int i=0;i<count;++i)if(parents[i]<-1||parents[i]>=i)return false;return true;}
};
// Rotate a complete articulated branch about its joint in model space. Child
// lengths remain rigid; later interpolation operates on local quaternions.
inline void rotateAxis(const Rig& rig,Matrix* pose,int bone,const float* axis,float degrees){
 if(bone<0||bone>=rig.count||!std::isfinite(degrees))return;
 const float length=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);if(!std::isfinite(length)||length<.00001f)return;
 float v[3]={axis[0]/length,axis[1]/length,axis[2]/length};
 const float a=radians(degrees),c=std::cos(a),s=std::sin(a);Matrix rotation{};
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)rotation[i][j]=(i==j?c:0)+(1-c)*v[i]*v[j];
 rotation[0][1]-=s*v[2];rotation[0][2]+=s*v[1];rotation[1][0]+=s*v[2];rotation[1][2]-=s*v[0];rotation[2][0]-=s*v[1];rotation[2][1]+=s*v[0];
 float pivot[3]={pose[bone][0][3],pose[bone][1][3],pose[bone][2][3]};
 for(int i=0;i<3;++i){rotation[i][3]=pivot[i];for(int j=0;j<3;++j)rotation[i][3]-=rotation[i][j]*pivot[j];}
 for(int i=0;i<rig.count;++i)if(rig.descendant(i,bone))multiply(rotation,pose[i],pose[i]);
}
inline void rotate(const Rig& rig,Matrix* pose,int bone,int axis,float degrees){
 if(axis<0||axis>2)return;float v[3]{};v[axis]=1;rotateAxis(rig,pose,bone,v,degrees);
}
inline void anatomicalAxis(const Rig& rig,const Matrix* pose,bool upper,int axis,float* out){
 const int l=upper?rig.left[1]:rig.leftLeg[0],r=upper?rig.right[1]:rig.rightLeg[0];
 float left[3]={0,1,0},up[3]={0,0,1};
 if(l>=0&&r>=0)for(int i=0;i<3;++i)left[i]=pose[l][i][3]-pose[r][i][3];
 const auto normalize=[](float* v){float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(n<.00001f)return false;for(int i=0;i<3;++i)v[i]/=n;return true;};
 if(!normalize(left)){left[0]=0;left[1]=1;left[2]=0;}
 if(upper&&rig.spine[3]>=0){Matrix torso;multiply(pose[rig.spine[3]],rig.torsoInverseBind,torso);for(int i=0;i<3;++i)up[i]=torso[i][2];}
 float dot=0;for(int i=0;i<3;++i)dot+=up[i]*left[i];for(int i=0;i<3;++i)up[i]-=dot*left[i];
 if(!normalize(up)){up[0]=0;up[1]=0;up[2]=1;}
 const float forward[3]={left[1]*up[2]-left[2]*up[1],left[2]*up[0]-left[0]*up[2],left[0]*up[1]-left[1]*up[0]};
 for(int i=0;i<3;++i)out[i]=axis==0?forward[i]:axis==1?left[i]:up[i];
}
inline void mirrorLeft(const Rig& rig,Matrix* primary,const Matrix* secondary,const Matrix* inverseBind){
 if(rig.neck<0||rig.left[1]<0||rig.right[1]<0)return;
 Matrix primaryFrame{},secondaryFrame{},inv,alignment;
 for(int col=0;col<3;++col){float a[3],b[3];anatomicalAxis(rig,primary,true,col,a);anatomicalAxis(rig,secondary,true,col,b);for(int row=0;row<3;++row){primaryFrame[row][col]=a[row];secondaryFrame[row][col]=b[row];}}
 for(int row=0;row<3;++row){primaryFrame[row][3]=primary[rig.neck][row][3];secondaryFrame[row][3]=secondary[rig.neck][row][3];}
 inverse(secondaryFrame,inv);multiply(primaryFrame,inv,alignment);
 Matrix leftBind,rightBind;inverse(inverseBind[rig.left[1]],leftBind);inverse(inverseBind[rig.right[1]],rightBind);
 float bindNormal[3],length=0;for(int row=0;row<3;++row){bindNormal[row]=leftBind[row][3]-rightBind[row][3];length+=bindNormal[row]*bindNormal[row];}
 if(length<.000001f)return;for(auto& value:bindNormal)value/=std::sqrt(length);
 Matrix bindReflection{},reflection{};
 for(int row=0;row<3;++row)for(int col=0;col<3;++col){bindReflection[row][col]=(row==col?1.f:0.f)-2*bindNormal[row]*bindNormal[col];reflection[row][col]=(row==col?1.f:0.f)-2*secondaryFrame[row][1]*secondaryFrame[col][1];}
 // Reflect about the posed body's sagittal plane, not a guessed model axis.
 // The bind-side correction restores proper handedness for each left bone.
 for(int i=0;i<8;++i){const int l=rig.left[i],r=rig.right[i];if(l<0||r<0)continue;
  inverse(inverseBind[l],leftBind);inverse(inverseBind[r],rightBind);Matrix correction{},mirrored{};
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)for(int a=0;a<3;++a)for(int b=0;b<3;++b)correction[row][col]+=rightBind[a][row]*bindReflection[a][b]*leftBind[b][col];
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)for(int a=0;a<3;++a)for(int b=0;b<3;++b)mirrored[row][col]+=reflection[row][a]*secondary[r][a][b]*correction[b][col];
  for(int row=0;row<3;++row){mirrored[row][3]=secondaryFrame[row][3];for(int col=0;col<3;++col)mirrored[row][3]+=reflection[row][col]*(secondary[r][col][3]-secondaryFrame[col][3]);}
  multiply(alignment,mirrored,primary[l]);
 }
}
inline void copyBranch(const Rig& rig,Matrix* target,const Matrix* source,int branch){
 for(int i=0;i<rig.count;++i)if(rig.descendant(i,branch)){
  const int parent=rig.parents[i];if(parent<0){std::memcpy(target[i],source[i],sizeof(Matrix));continue;}
  Matrix inv,local;inverse(source[parent],inv);multiply(inv,source[i],local);multiply(target[parent],local,target[i]);
 }
}
struct Input {
 float now=0,dt=0,yaw=0,pitch=0,vx=0,vy=0,vz=0,speed=0,frame=0;
 bool crouched=false,ladder=false,swim=false,action=false;
 uint64_t weapon=0;cof_shadow_animation::Selection choice;
};
struct State {
 bool valid=false,crouched=false;float bodyYaw=0,hipYaw=0,aimPitch=0,turnSpeed=0,reverse=1,turnWeight=0;
 float transition=1,duration=.18f,switchTime=1,elapsed=0,travelPhase=0,stridePhase=0;
 int gait=-1,sequence=-1,traversal=0;uint64_t weapon=0,secondary=0;
 cof_shadow_animation::Gesture gesture=cof_shadow_animation::Gesture::None;
 std::array<Joint,Limit> displayed{},start{};
 void clear(){*this={};}
 void advance(const Input& in){
  const float dt=std::clamp(in.dt,0.f,.1f);elapsed+=dt;
  const float travelRate=in.ladder?4*std::clamp(std::fabs(in.vz)/70.f,.3f,2.f)*(in.vz<0?-1:1):in.swim?(in.speed>20?4.f:2.5f):0;
  travelPhase=std::remainder(travelPhase+dt*travelRate,2*Pi);
  if(!valid){bodyYaw=in.yaw;aimPitch=std::clamp(in.pitch,-75.f,75.f);}
  const float yawDelta=angle(in.yaw-bodyYaw),previous=bodyYaw;
  const float rate=in.speed>10?240.f:150.f;
  if(in.speed>10||std::fabs(yawDelta)>30)bodyYaw=angle(bodyYaw+std::clamp(yawDelta,-rate*dt,rate*dt));
  turnSpeed=dt>0?angle(bodyYaw-previous)/dt:0;
  turnWeight+=(1-std::exp(-dt/.1f))*(std::min(1.f,std::fabs(turnSpeed)/120.f)-turnWeight);
  const float yaw=radians(bodyYaw),forward=in.vx*std::cos(yaw)+in.vy*std::sin(yaw),side=-in.vx*std::sin(yaw)+in.vy*std::cos(yaw);
  reverse=forward<-.25f*std::max(1.f,in.speed)?-1.f:1.f;
  stridePhase=std::remainder(stridePhase+dt*.08f*in.speed*reverse,2*Pi);
  float direction=in.speed>10?std::atan2(side*reverse,forward*reverse)*180/Pi:0;
  direction=std::clamp(direction,-75.f,75.f);
  const float mix=1-std::exp(-dt/ .09f);hipYaw+=mix*(direction-hipYaw);aimPitch+=mix*(std::clamp(in.pitch,-75.f,75.f)-aimPitch);
  switchTime=std::min(1.f,switchTime+dt/.32f);
  if(valid&&weapon!=in.weapon)switchTime=0;
  weapon=in.weapon;
 }
 void procedural(const Rig& rig,Matrix* pose,const Input& in)const {
  using namespace cof_shadow_animation;
  const auto turn=[&](int bone,int axis,float degrees){
   const bool arm=rig.descendant(bone,rig.left[0])||rig.descendant(bone,rig.right[0]);
   const bool leg=rig.descendant(bone,rig.leftLeg[0])||rig.descendant(bone,rig.rightLeg[0]);
   if(arm||leg){
    float v[3];anatomicalAxis(rig,pose,arm,axis,v);rotateAxis(rig,pose,bone,v,degrees);
   }else rotate(rig,pose,bone,axis,degrees);
  };
  const auto pitch=[&](int bone,float degrees){float v[3];anatomicalAxis(rig,pose,true,1,v);rotateAxis(rig,pose,bone,v,degrees);};
  if(!in.ladder&&!in.swim){
   turn(rig.pelvis,2,hipYaw);turn(rig.spine[0],2,-hipYaw+std::clamp(angle(in.yaw-bodyYaw),-40.f,40.f));
   // Shoulder-derived axes avoid assuming the donor's model/bone orientation.
   // Keep most aiming in shoulders/head instead of folding the waist.
   for(int i=0;i<4;++i)pitch(rig.spine[i],aimPitch*.065f);
   pitch(rig.neck,aimPitch*.16f);pitch(rig.head,aimPitch*.16f);
   turn(rig.left[1],1,aimPitch*.65f);turn(rig.right[1],1,aimPitch*.65f);
   const float lean=std::clamp(in.speed/240.f,0.f,1.f);turn(rig.spine[0],0,std::clamp(-hipYaw*.06f,-4.f,4.f)*lean);
   // Standing foot shuffles pump a deep crouch upward when its lowest foot
   // is grounded. Turning a stationary crouch must keep both feet planted.
   if(!in.crouched&&in.speed<10&&turnWeight>.001f){const float step=std::sin(elapsed*11)*turnWeight;
    turn(rig.leftLeg[0],1,step*8);turn(rig.rightLeg[0],1,-step*8);turn(rig.leftLeg[1],1,std::max(0.f,step)*10);turn(rig.rightLeg[1],1,std::max(0.f,-step)*10);}
   if(in.crouched&&in.speed>10){const float step=std::sin(stridePhase)*std::clamp(in.speed/60.f,0.f,1.f);
    turn(rig.leftLeg[0],1,step*9);turn(rig.rightLeg[0],1,-step*9);turn(rig.leftLeg[1],1,-step*5);turn(rig.rightLeg[1],1,step*5);turn(rig.leftLeg[2],1,-step*4);turn(rig.rightLeg[2],1,step*4);}
  }
  const auto& l=rig.left;const auto& r=rig.right;
  switch(in.choice.profile){
   case Profile::Famas:turn(r[1],2,-3);turn(l[1],1,-8);turn(l[1],2,5);break;
   case Profile::AK47:turn(r[1],1,3);turn(l[2],1,-10);break;
   case Profile::MP5:turn(r[2],1,-8);turn(l[1],1,4);break;
   case Profile::M76:turn(r[1],1,5);turn(l[2],1,-5);turn(l[1],2,5);break;
   case Profile::G43:turn(l[2],0,4);turn(r[1],1,5);break;
   case Profile::Browning:turn(r[2],2,-6);turn(l[2],2,6);break;
   case Profile::Axe:turn(r[1],1,12);turn(l[1],1,-8);break;
   case Profile::Shovel:turn(r[1],2,-8);turn(l[1],2,8);break;
   case Profile::Phone:turn(r[2],1,-20);turn(r[3],0,12);break;
   case Profile::Flashlight:turn(r[1],1,-10);turn(r[3],0,-8);break;
   case Profile::Lantern:turn(r[1],1,32);turn(r[2],1,15);break;
   case Profile::Camera:turn(r[1],1,-12);turn(l[1],1,-12);turn(r[2],1,-15);turn(l[2],1,-15);break;
   case Profile::Flare:turn(r[1],1,-8);turn(r[3],0,15);break;
   default:break;
  }
  const float phase=std::clamp(in.frame/256.f,0.f,1.f),wave=std::sin(Pi*phase);
  if(in.choice.gesture==Gesture::Ignite){turn(l[1],1,-35*wave);turn(l[2],1,-25*wave);turn(r[2],0,15*wave);}
  else if(in.choice.gesture==Gesture::Throw){turn(r[1],1,phase<.45f?-55*smooth(phase/.45f):-55+120*smooth((phase-.45f)/.55f));turn(r[2],1,-40*wave);}
  else if(in.choice.gesture==Gesture::Use){turn(r[1],1,-12*wave);turn(r[3],2,10*wave);}
  if(in.choice.profile==Profile::Axe&&in.action)turn(r[1],1,-15*wave);
  if(in.choice.profile==Profile::Shovel&&in.action)turn(l[1],1,12*wave);
  float lower=std::sin(Pi*switchTime)*28;
  if(in.choice.gesture==Gesture::Draw)lower+=35*(1-smooth(phase));
  if(in.choice.gesture==Gesture::Holster)lower+=35*smooth(phase);
  turn(r[1],1,lower);turn(l[1],1,lower*.75f);
  if(in.ladder){
   const float step=std::sin(travelPhase);
   pitch(rig.spine[0],8);turn(l[1],1,-60-25*step);turn(r[1],1,-60+25*step);
   turn(l[2],1,-30+20*step);turn(r[2],1,-30-20*step);
   turn(rig.leftLeg[0],1,25+25*step);turn(rig.rightLeg[0],1,25-25*step);
   turn(rig.leftLeg[1],1,-35-25*step);turn(rig.rightLeg[1],1,-35+25*step);
  }else if(in.swim){
   const float moving=std::clamp(in.speed/80.f,0.f,1.f),step=std::sin(travelPhase);
   pitch(rig.pelvis,55*moving);pitch(rig.spine[0],8);
   const float spread=45-20*moving,pitchBase=15-35*moving,stroke=12+28*moving;
   turn(l[1],0,-spread);turn(r[1],0,spread);turn(l[1],1,pitchBase+stroke*step);turn(r[1],1,pitchBase-stroke*step);
   turn(l[2],1,10-35*moving);turn(r[2],1,10-35*moving);turn(rig.leftLeg[0],1,step*18);turn(rig.rightLeg[0],1,-step*18);
   turn(rig.leftLeg[1],1,-12-std::max(0.f,step)*18);turn(rig.rightLeg[1],1,-12-std::max(0.f,-step)*18);
  }
 }
 bool resolve(const Rig& rig,Matrix* target,const Input& in,int newSequence,int newGait,uint64_t secondaryKey){
  if(!rig.valid())return false;
  std::array<Joint,Limit> local{};
  for(int i=0;i<rig.count;++i){Matrix m;const int parent=rig.parents[i];if(parent>=0){Matrix inv;inverse(target[parent],inv);multiply(inv,target[i],m);}else std::memcpy(m,target[i],sizeof(Matrix));local[i]=joint(m);
   for(float x:local[i].p)if(!std::isfinite(x))return false;for(float x:local[i].q)if(!std::isfinite(x))return false;}
  const int travel=in.ladder?1:in.swim?2:0;
  if(!valid){displayed=local;start=local;transition=1;}
  else if(newSequence!=sequence||newGait!=gait||crouched!=in.crouched||secondary!=secondaryKey||traversal!=travel||gesture!=in.choice.gesture||switchTime==0){
   start=displayed;transition=0;duration=crouched!=in.crouched?.28f:travel!=traversal?.3f:in.action?.07f:.18f;
  }
  transition=std::min(1.f,transition+std::clamp(in.dt,0.f,.1f)/duration);
  const float mix=smooth(transition);
  for(int i=0;i<rig.count;++i){displayed[i]=blend(start[i],local[i],mix);Matrix m;matrix(displayed[i],m);if(rig.parents[i]>=0)multiply(target[rig.parents[i]],m,target[i]);else std::memcpy(target[i],m,sizeof(Matrix));}
  valid=true;crouched=in.crouched;sequence=newSequence;gait=newGait;secondary=secondaryKey;traversal=travel;gesture=in.choice.gesture;return true;
 }
};
inline uint64_t key(std::string_view text){uint64_t hash=1469598103934665603ULL;for(unsigned char c:text)hash=(hash^c)*1099511628211ULL;return hash;}
}
