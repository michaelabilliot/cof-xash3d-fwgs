#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cof_pickup {
struct Point { float x=0,y=0; bool valid=false; };
struct Span { float x,y,width,height; unsigned alpha; };
// A bounded screen-space silhouette. It never submits emissive world geometry
// or lights to Remix, so nearby floors and walls retain their own lighting.
inline std::vector<Span> outline(const std::vector<Point>& points,const std::vector<uint32_t>& indices,
 float vx,float vy,float vw,float vh,float strength,bool soft=false) {
 std::vector<Span> result;
 if(points.empty()||points.size()>65536||indices.empty()||indices.size()>98304||indices.size()%3||
    !std::isfinite(strength)||strength<=0||!std::isfinite(vx)||!std::isfinite(vy)||
    !std::isfinite(vw)||!std::isfinite(vh)||vw<=0||vh<=0||vw>16384||vh>16384)return result;
 float loX=vx+vw,loY=vy+vh,hiX=vx,hiY=vy;
 for(auto index:indices) {
  if(index>=points.size())return {};
  const auto& p=points[index];if(!p.valid)continue;
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||std::fabs(p.x)>1e6f||std::fabs(p.y)>1e6f)return {};
  loX=std::min(loX,p.x);loY=std::min(loY,p.y);hiX=std::max(hiX,p.x);hiY=std::max(hiY,p.y);
 }
 if(hiX<vx||loX>vx+vw||hiY<vy||loY>vy+vh||loX>=hiX||loY>=hiY)return result;
 const float margin=soft?14.f:4.f;
 const float padding=soft?0.f:8.f,capacity=soft?484.f:508.f;
 const float scale=std::max({1.f,(std::min(hiX,vx+vw)-std::max(loX,vx)+padding)/capacity,
  (std::min(hiY,vy+vh)-std::max(loY,vy)+padding)/capacity});
 const float x0=std::max(vx,std::floor(loX-margin*scale)),y0=std::max(vy,std::floor(loY-margin*scale));
 const int width=std::clamp(int(std::ceil((std::min(vx+vw,hiX+margin*scale)-x0)/scale)),1,512);
 const int height=std::clamp(int(std::ceil((std::min(vy+vh,hiY+margin*scale)-y0)/scale)),1,512);
 std::vector<uint8_t> distance(size_t(width)*height,255);
 for(size_t i=0;i<indices.size();i+=3) {
  Point p[3]={points[indices[i]],points[indices[i+1]],points[indices[i+2]]};
  if(!p[0].valid||!p[1].valid||!p[2].valid)continue;
  for(auto& v:p){v.x=(v.x-x0)/scale;v.y=(v.y-y0)/scale;}
  const int top=std::clamp(int(std::ceil(std::min({p[0].y,p[1].y,p[2].y})-.5f)),0,height);
  const int bottom=std::clamp(int(std::ceil(std::max({p[0].y,p[1].y,p[2].y})-.5f)),0,height);
  for(int y=top;y<bottom;++y) {
   float left=1e9f,right=-1e9f;int hits=0;const float sample=y+.5f;
   for(int edge=0;edge<3;++edge) {
    const auto& a=p[edge];const auto& b=p[(edge+1)%3];
    if((a.y<=sample&&b.y>sample)||(b.y<=sample&&a.y>sample)) {
     const float x=a.x+(sample-a.y)*(b.x-a.x)/(b.y-a.y);
     left=std::min(left,x);right=std::max(right,x);++hits;
    }
   }
   if(hits<2)continue;
   const int start=std::clamp(int(std::ceil(left-.5f)),0,width),end=std::clamp(int(std::ceil(right-.5f)),0,width);
   std::fill(distance.begin()+size_t(y)*width+start,distance.begin()+size_t(y)*width+end,0);
  }
 }
 // Chamfer distance in two bounded sweeps; the interior stays transparent.
 for(int pass=0;pass<2;++pass)for(int row=0;row<height;++row)for(int col=0;col<width;++col) {
  const int x=pass?width-1-col:col,y=pass?height-1-row:row,step=pass?1:-1;
  auto& d=distance[size_t(y)*width+x];
  for(int dx=-1;dx<=1;++dx)if(x+dx>=0&&x+dx<width&&y+step>=0&&y+step<height)
   d=uint8_t(std::min(int(d),int(distance[size_t(y+step)*width+x+dx])+1));
  if(x+step>=0&&x+step<width)d=uint8_t(std::min(int(d),int(distance[size_t(y)*width+x+step])+1));
 }
 const auto alpha=[&](uint8_t d){return unsigned((soft?(d>0&&d<14?95.f*std::exp(-float(d*d)/40.f):0.f):(d==1?120.f:d==2?70.f:d==3?22.f:0.f))*std::clamp(strength,0.f,1.f));};
 for(int y=0;y<height;++y)for(int x=0;x<width;) {
  const unsigned a=alpha(distance[size_t(y)*width+x]);const int start=x++;
  while(x<width&&alpha(distance[size_t(y)*width+x])==a)++x;
  if(a)result.push_back({x0+start*scale,y0+y*scale,std::min((x-start)*scale,vx+vw-(x0+start*scale)),std::min(scale,vy+vh-(y0+y*scale)),a});
 }
 return result;
}
}
