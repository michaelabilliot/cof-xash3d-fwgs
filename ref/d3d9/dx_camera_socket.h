#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
namespace camera_socket {
using V=std::array<float,3>;
using Triangle=std::array<V,3>;
struct Geometry {std::array<V,4> face{};std::vector<Triangle> housing;V pivot{};std::vector<Triangle> hands;};
struct Socket {V face{},normal{},source{};float clearance=0,epsilon=0;};
inline V add(V a,const V& b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
inline V sub(V a,const V& b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
inline V mul(V a,float f){for(float& x:a)x*=f;return a;}
inline float dot(const V& a,const V& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline V cross(const V& a,const V& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline bool finite(const V& v){for(float x:v)if(!std::isfinite(x)||fabsf(x)>1e7f)return false;return true;}
inline bool unit(V& v){const float n=std::sqrt(dot(v,v));if(!std::isfinite(n)||n<1e-8f)return false;v=mul(v,1.f/n);return true;}
inline std::array<V,9> rays(const V& normal,float halfAngle=.4363323129985824f){
 V u=cross(normal,fabsf(normal[2])<.9f?V{0,0,1}:V{1,0,0});unit(u);const V v=cross(normal,u);
 std::array<V,9> result{};result[0]=normal;
 const float angle=halfAngle;
 for(int i=0;i<8;++i){const float a=float(i)*.7853981633974483f;
  result[i+1]=add(mul(normal,std::cos(angle)),mul(add(mul(u,std::cos(a)),mul(v,std::sin(a))),std::sin(angle)));
 }return result;
}
inline bool hit(const V& origin,const V& direction,const Triangle& t,float epsilon){
 const V e1=sub(t[1],t[0]),e2=sub(t[2],t[0]),p=cross(direction,e2);
 const float determinant=dot(e1,p);
 if(fabsf(determinant)<1e-9f)return false;
 const float inv=1.f/determinant;const V d=sub(origin,t[0]);const float u=dot(d,p)*inv;
 if(u<-.00001f||u>1.00001f)return false;
 const V q=cross(d,e1);const float v=dot(direction,q)*inv;
 if(v<-.00001f||u+v>1.00001f)return false;
 return dot(e2,q)*inv>epsilon*.25f;
}
inline bool resolve(const Geometry& geometry,V aim,Socket& output){
 if(geometry.housing.empty()||geometry.housing.size()>20000||!unit(aim))return false;
 Socket result;V lo{1e7f,1e7f,1e7f},hi{-1e7f,-1e7f,-1e7f};float coordinate=1;
 for(const auto& p:geometry.face){if(!finite(p))return false;result.face=add(result.face,mul(p,.25f));}
 result.normal=cross(sub(geometry.face[1],geometry.face[0]),sub(geometry.face[2],geometry.face[0]));
 if(!unit(result.normal))return false;
 if(dot(result.normal,aim)<0)result.normal=mul(result.normal,-1);
 for(const auto& triangle:geometry.housing)for(const auto& p:triangle){
  if(!finite(p))return false;
  for(int a=0;a<3;++a){lo[a]=std::min(lo[a],p[a]);hi[a]=std::max(hi[a],p[a]);coordinate=std::max(coordinate,fabsf(p[a]));}
 }
 const V extent=sub(hi,lo);const float span=std::sqrt(dot(extent,extent));
 // Numerical separation scales with the geometry and world-coordinate ULP;
 // housing intersections, rather than a guessed fixed offset, set clearance.
 result.epsilon=std::max({.001f,span*.0001f,coordinate*std::numeric_limits<float>::epsilon()*8.f});
 float distance=result.epsilon,front=0;
 for(const auto& triangle:geometry.housing)for(const auto& p:triangle)front=std::max(front,dot(sub(p,result.face),result.normal));
 const auto directions=rays(result.normal);
 for(int iteration=0;iteration<18;++iteration){
  result.source=add(result.face,mul(result.normal,distance));float required=distance;bool obstructed=false;
  for(const auto& triangle:geometry.housing){
   bool blocks=false;for(const auto& ray:directions)if(hit(result.source,ray,triangle,result.epsilon)){blocks=true;break;}
   if(!blocks)continue;obstructed=true;
   for(const auto& p:triangle)required=std::max(required,dot(sub(p,result.face),result.normal)+result.epsilon);
  }
  if(!obstructed){result.clearance=distance;output=result;return finite(result.source);}
  distance=iteration<16?std::max(required,distance+result.epsilon):front+result.epsilon;
 }
 return false;
}
struct Area {V face{},source{},normal{},xAxis{},yAxis{};float width=0,height=0,epsilon=0,clearance=0;};
inline std::array<V,15> samples(const Area& a){
 std::array<V,15> result{};unsigned index=0;
 for(int y=0;y<3;++y)for(int x=0;x<5;++x)
  result[index++]=add(a.source,add(mul(a.xAxis,a.width*(float(x)/4.f-.5f)),mul(a.yAxis,a.height*(float(y)/2.f-.5f))));
 return result;
}
inline bool resolveArea(const Geometry& g,V aim,Area& output){
 Socket initial;if(!resolve(g,aim,initial)||g.hands.size()>20000)return false;
 Area a;a.face=initial.face;a.normal=initial.normal;a.epsilon=initial.epsilon;
 a.xAxis=sub(g.face[1],g.face[0]);if(!unit(a.xAxis))return false;a.yAxis=cross(a.normal,a.xAxis);if(!unit(a.yAxis))return false;
 std::array<std::array<float,2>,4> plane{};float hx=0,hy=0;
 for(int i=0;i<4;++i){const V d=sub(g.face[i],a.face);plane[i]={dot(d,a.xAxis),dot(d,a.yAxis)};hx=std::max(hx,fabsf(plane[i][0]));hy=std::max(hy,fabsf(plane[i][1]));}
 if(hx<=1e-6f||hy<=1e-6f)return false;
 // Inscribe a centred rectangle in the corrected (possibly sheared) quad.
 float fit=1;
 for(int i=0;i<4;++i){auto p=plane[i],q=plane[(i+1)%4];float nx=-(q[1]-p[1]),ny=q[0]-p[0];
  float margin=-nx*p[0]-ny*p[1];if(margin<0){nx=-nx;ny=-ny;margin=-margin;}if(margin<=1e-12f)return false;
  for(float x:{-hx,hx})for(float y:{-hy,hy}){const float d=nx*x+ny*y;if(d<0)fit=std::min(fit,margin/-d);}
 }
 a.width=2*hx*fit*.98f;a.height=2*hy*fit*.98f;
 float front=0;for(const auto* collection:{&g.housing,&g.hands})for(const auto& triangle:*collection)for(const auto& p:triangle){if(!finite(p))return false;front=std::max(front,dot(sub(p,a.face),a.normal));}
 float distance=initial.clearance;
 for(int iteration=0;iteration<18;++iteration){
  a.source=add(a.face,mul(a.normal,distance));const auto points=samples(a);bool blocked=false;float required=distance;
  for(const auto* collection:{&g.housing,&g.hands})for(const auto& triangle:*collection){
   float limit=-1e7f;for(const auto& p:triangle)limit=std::max(limit,dot(sub(p,a.face),a.normal));
   if(limit<=distance-a.epsilon*.25f)continue;
   // Keep the entire aperture outside the camera/hands. Do not move the
   // lamp away from its panel merely because a finger occludes a wide ray.
   bool obstruction=false;for(const auto& p:points)if(hit(p,a.normal,triangle,a.epsilon)){obstruction=true;break;}
   if(obstruction){blocked=true;required=std::max(required,limit+a.epsilon);}
  }
  if(!blocked){a.clearance=distance;output=a;return finite(a.source);}
  distance=iteration<16?std::max(required,distance+a.epsilon):front+a.epsilon;
 }
 return false;
}
}
