#include "dx_batching.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
struct V {
 float x=0,y=0,z=0,u=0,v=0;unsigned color=0;
 V()=default;
 V(float position,float uv,unsigned rgba):x(position),y(position+1),z(position+2),u(uv),v(uv+.1f),color(rgba){}
 bool operator==(const V& b)const{return x==b.x&&y==b.y&&z==b.z&&u==b.u&&v==b.v&&color==b.color;}
};
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
 V v[8];for(unsigned i=0;i<8;++i)v[i]={float(i),float(i)*.1f,i};
 struct C{int mode;size_t count;std::vector<int> expected;};
 const C cases[]={{0,7,{0,1,2,3,4,5}},{1,5,{0,1,2,0,2,3,0,3,4}},{2,8,{0,1,2,0,2,3,4,5,6,4,6,7}},{3,4,{0,1,2,0,2,3}},{5,5,{0,1,2,2,1,3,2,3,4}},{6,6,{0,1,3,0,3,2,2,3,5,2,5,4}}};
 for(const auto& c:cases){dx_triangle_batch_t<V> b;check(b.append(c.mode,v,c.count,32768,[]{check(false,"topology flush");}),"supported");check(b.vertices.size()==c.expected.size(),"count");for(size_t i=0;i<c.expected.size();++i)check(b.vertices[i]==v[c.expected[i]],"winding/fields");}
 for(int mode:{0,2}){dx_triangle_batch_t<V>b;std::vector<V>expected,out;unsigned draws=0;
 auto flush=[&](){if(!b.vertices.empty()){++draws;out.insert(out.end(),b.vertices.begin(),b.vertices.end());b.vertices.clear();}};
 for(unsigned p=0;p<4000;++p){for(unsigned i=0;i<4;++i)v[i]={float(p),float(i)*.1f,p*4+i};if(mode==0)expected.insert(expected.end(),v,v+3);else for(int i:{0,1,2,0,2,3})expected.push_back(v[i]);b.append(mode,v,mode==0?3:4,DX_BatchVertexLimit(32768),flush);}
 flush();check(draws==1&&out==expected,"4000 weather submissions one draw, ordered fields/alpha");}
 dx_triangle_batch_t<V>b;unsigned draws=0;std::vector<V>out;
 auto flush=[&](){if(!b.vertices.empty()){++draws;out.insert(out.end(),b.vertices.begin(),b.vertices.end());b.vertices.clear();}};
 for(unsigned i=0;i<100;++i)b.append(2,v,4,193,flush);flush();check(draws==4&&out.size()==600,"capacity/triangle split");
 check(!b.append(0,nullptr,3,192,flush)&&!b.append(0,v,3,2,flush),"invalid input");check(!b.append(7,v,3,192,flush)&&!b.append(4,v,3,192,flush),"points/lines excluded");check(b.append(0,nullptr,0,192,flush)&&b.vertices.empty(),"empty pass");b.append(0,v,2,192,flush);check(b.vertices.empty(),"incomplete triangle");
 b.append(0,v,3,192,flush);flush();b.append(0,v,3,192,flush);flush();check(draws==6,"state boundaries");b.append(0,v,3,192,flush);b.vertices.clear();flush();check(draws==6,"reset cleanup");
 check(DX_BatchVertexLimit(-5)==192&&DX_BatchVertexLimit(1e9f)==262144&&DX_BatchVertexLimit(std::numeric_limits<float>::quiet_NaN())==32768,"clamp");
 std::vector<unsigned>indices(1200);std::iota(indices.begin(),indices.end(),0u);std::vector<dx_index_span_t>spans;
 for(size_t i=0;i<200;++i)check(DX_AppendIndexSpan(spans,i*6,3,indices.size()),"range");std::vector<unsigned>expected;for(const auto& s:spans)expected.insert(expected.end(),indices.begin()+s.first,indices.begin()+s.first+s.count);
 for(bool fail:{false,true}){std::vector<unsigned>actual;unsigned chunks=0;DX_StreamVisibleIndices(indices.data(),spans,193,[&](const std::vector<unsigned>&chunk,const std::vector<dx_index_span_t>&ranges){++chunks;check(chunk.size()<=193&&chunk.size()%3==0,"index bound");if(fail&&chunks%2)for(const auto& r:ranges)actual.insert(actual.end(),indices.begin()+r.first,indices.begin()+r.first+r.count);else actual.insert(actual.end(),chunk.begin(),chunk.end());});check(chunks==4&&actual==expected,"glass visibility/fallback order");}
 spans.clear();check(DX_AppendIndexSpan(spans,0,3,12)&&DX_AppendIndexSpan(spans,3,6,12)&&spans.size()==1&&spans[0].count==9,"adjacent merge");check(!DX_AppendIndexSpan(spans,12,3,12)&&!DX_AppendIndexSpan(spans,0,4,12),"malformed glass");
 const float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},n[3]={1,0,0},front[3]={5,0,0},back[3]={-5,0,0},zero[3]={0,0,0};check(DX_GlassFacesView(n,1,m,front)&&!DX_GlassFacesView(n,1,m,back)&&DX_GlassFacesView(zero,0,m,back),"visibility");
 const float rotated[16]={0,1,0,0,-1,0,0,0,0,0,1,0,10,20,30,1},above[3]={10,22,30},below[3]={10,20,30};check(DX_GlassFacesView(n,1,rotated,above)&&!DX_GlassFacesView(n,1,rotated,below),"transformed brush");

 // A single oversized call must split without dropping or repeating triangles.
 std::vector<V> large(40002);for(size_t i=0;i<large.size();++i)large[i]={float(i),float(i)*.125f,static_cast<unsigned>(i)};
 out.clear();draws=0;b.append(0,large.data(),large.size(),32768,flush);flush();check(draws==2&&out==large,"oversized submission exact split");
 // Alternating transformed faces use the same ordered indices as individual draws.
 spans.clear();expected.clear();
 for(size_t face=0;face<200;++face){float normal[3]={face%2?-1.f:1.f,0,0};
  const bool visible=DX_GlassFacesView(normal,1,rotated,above);
  // Original point/normal calculation for this rotation and translation.
  const float planeY=20+normal[0];const bool original=normal[0]*(above[1]-planeY)>.01f;
  check(visible==original,"existing transformed visibility equivalence");
  if(visible){DX_AppendIndexSpan(spans,face*6,6,indices.size());expected.insert(expected.end(),indices.begin()+face*6,indices.begin()+face*6+6);}}
 std::vector<unsigned> visibleOut;DX_StreamVisibleIndices(indices.data(),spans,262144,[&](const std::vector<unsigned>&chunk,const std::vector<dx_index_span_t>&){visibleOut.insert(visibleOut.end(),chunk.begin(),chunk.end());});check(visibleOut==expected,"alternating hidden faces equivalence");
 spans.clear();unsigned emptyCalls=0;DX_StreamVisibleIndices(indices.data(),spans,262144,[&](const std::vector<unsigned>&,const std::vector<dx_index_span_t>&){++emptyCalls;});check(emptyCalls==0,"empty/fully culled glass");
 std::puts("PASS: weather topologies; 4000 rain/snow; ordered fields/alpha; capacity/state/reset; glass transformed visibility, fragmentation and upload fallback.");return 0;
}
