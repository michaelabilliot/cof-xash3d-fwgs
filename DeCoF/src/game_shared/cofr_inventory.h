#pragma once
#include <string>
namespace cof {
constexpr int Slots=8;
inline bool validSlot(int n){return n>=1&&n<=Slots;}
struct Reader {
 const unsigned char* p; const unsigned char* end; bool ok=true;
 Reader(void* d,int n):p(static_cast<const unsigned char*>(d)),end(p? p+(n>0?n:0):p){if(!d||n<0)ok=false;}
 int byte(){if(!ok||p==end){ok=false;return 0;}return *p++;}
 int shortValue(){int a=byte();return static_cast<short>(a|(byte()<<8));}
 std::string string(){if(!ok)return {};auto b=p;while(p<end&&*p)++p;if(p==end){ok=false;return {};}std::string s(reinterpret_cast<const char*>(b),reinterpret_cast<const char*>(p));++p;return s;}
};
}
void CoFR_ClientInitialize();void CoFR_ClientInit();void CoFR_ClientFrame();bool CoFR_ClientKey(int,int);void CoFR_ClientShutdown();

struct cl_entity_s;void CoFR_EntityTrace(cl_entity_s*,int,const char*);
