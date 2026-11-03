#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// CoF's customdecal wire format and decalinfo.txt are independent of WAD decals.
namespace cof_decal {
using Vec = std::array<float, 3>;
struct Message { Vec origin{}, normal{}; unsigned char flags = 0; std::string group; };
struct Variant { std::string texture; float halfWidth = 0, halfHeight = 0, overlap = 0; };
struct Group { std::string name; std::vector<Variant> variants; };
inline bool nameValid(const std::string& s) {
 if(s.empty() || s.size()>63) return false;
 for(unsigned char c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
 return true;
}
inline bool decode(const void* data, int size, Message& out) {
 if(!data || size<15 || size>78) return false;
 const auto* b=static_cast<const unsigned char*>(data); Message m;
 for(int i=0;i<6;++i) {
  const unsigned u=b[i*2]|(static_cast<unsigned>(b[i*2+1])<<8);
  const float v=static_cast<float>(u>=32768?static_cast<int>(u)-65536:static_cast<int>(u))*.125f;
  (i<3?m.origin[i]:m.normal[i-3])=v;
 }
 m.flags=b[12];
 if(b[size-1]!=0) return false;
 for(int i=13;i<size-1;++i) { if(!b[i])return false; m.group+=static_cast<char>(b[i]); }
 if(!nameValid(m.group))return false;
 const float length=m.normal[0]*m.normal[0]+m.normal[1]*m.normal[1]+m.normal[2]*m.normal[2];
 if(length<.25f || length>2.25f)return false;
 out=m;return true;
}
inline float dot(const Vec& a,const Vec& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec cross(const Vec& a,const Vec& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline Vec unit(Vec a){const float n=std::sqrt(dot(a,a));if(n>0)for(float& v:a)v/=n;return a;}
inline void axes(const Vec& normal, Vec& s, Vec& t){
 const Vec seed=(normal[0]==0&&normal[1]==0)?Vec{1,0,0}:Vec{0,0,1};
 s=unit(cross(normal,seed));t=unit(cross(normal,s));
}
inline std::vector<Group> parse(const char* bytes,size_t size){
 if(!bytes||size>1024*1024)return {};
 std::vector<std::string> tokens;
 for(size_t i=0;i<size;){
  if(static_cast<unsigned char>(bytes[i])<=32){++i;continue;}
  if(bytes[i]=='/'&&i+1<size&&bytes[i+1]=='/'){while(i<size&&bytes[i]!='\n')++i;continue;}
  if(bytes[i]=='{'||bytes[i]=='}'){tokens.emplace_back(1,bytes[i++]);continue;}
  std::string token;const bool quoted=bytes[i]=='"';if(quoted)++i;
  while(i<size && (quoted?bytes[i]!='"':static_cast<unsigned char>(bytes[i])>32&&bytes[i]!='{'&&bytes[i]!='}')){
   if(token.size()>=63)return {};token+=bytes[i++];
  }
  if(quoted){if(i==size)return {};++i;}
  tokens.push_back(token);if(tokens.size()>32768)return {};
 }
 std::vector<Group> groups;
 for(size_t i=0;i<tokens.size();){
  std::vector<std::string> names;std::vector<Variant> variants;
  while(i<tokens.size()&&tokens[i]!="{"){if(!nameValid(tokens[i])||names.size()>=128)return {};names.push_back(tokens[i++]);}
  if(names.empty()||i==tokens.size())return {};++i;
  while(i<tokens.size()&&tokens[i]!="}"){
   if(i+3>=tokens.size()||!nameValid(tokens[i])||variants.size()>=64)return {};
   Variant v;v.texture=tokens[i++];float* values[]={&v.halfWidth,&v.halfHeight,&v.overlap};
   for(auto value:values){char* end=nullptr;*value=std::strtof(tokens[i++].c_str(),&end);if(!end||*end||!std::isfinite(*value))return {};}
   if(v.halfWidth<=0||v.halfHeight<=0||v.halfWidth>4096||v.halfHeight>4096||v.overlap<0||v.overlap>1)return {};
   // Native x87 parser multiplies full dimensions by 0.5 before integer conversion.
   v.halfWidth=std::trunc(v.halfWidth*0.5f);
   v.halfHeight=std::trunc(v.halfHeight*0.5f);
   if(v.halfWidth<1||v.halfHeight<1)return {};
   variants.push_back(v);
  }
  if(i==tokens.size()||variants.empty())return {};++i;
  for(const auto& name:names){if(groups.size()>=256)return {};groups.push_back({name,variants});}
 }
 return groups;
}
// Gray 127/128 is neutral under CoF's 2*source*destination blending.
inline unsigned char inkAlpha(int r,int g,int b){
 const int d=std::max(std::abs(r*2-255),std::max(std::abs(g*2-255),std::abs(b*2-255)));
 return static_cast<unsigned char>(std::max(0,std::min(255,(d-8)*2)));
}
}
