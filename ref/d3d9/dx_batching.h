#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

inline size_t DX_BatchVertexLimit(float value) {
    if(!std::isfinite(value)) return 32768;
    return static_cast<size_t>(std::max(192.f,std::min(262144.f,value)));
}

// TriangleAPI topology values are fixed by its public ABI.
template<class Emit> bool DX_Triangulate(int mode,size_t count,Emit emit) {
    switch(mode) {
    case 0:
        for(size_t i=0;i+2<count;i+=3) emit(i,i+1,i+2);
        break;
    case 1: case 3:
        for(size_t i=1;i+1<count;++i) emit(0,i,i+1);
        break;
    case 2:
        for(size_t i=0;i+3<count;i+=4) { emit(i,i+1,i+2); emit(i,i+2,i+3); }
        break;
    case 5:
        for(size_t i=0;i+2<count;++i) {
            if(i&1) emit(i+1,i,i+2); else emit(i,i+1,i+2);
        }
        break;
    case 6:
        for(size_t i=0;i+3<count;i+=2) { emit(i,i+1,i+3); emit(i,i+3,i+2); }
        break;
    default: return false;
    }
    return true;
}

template<class Vertex> struct dx_triangle_batch_t {
    std::vector<Vertex> vertices;
    template<class Flush> bool append(int mode,const Vertex* source,size_t count,size_t limit,Flush flush) {
        if((count && !source) || limit<3 || limit>262144) return false;
        if(vertices.capacity()<limit) vertices.reserve(limit);
        return DX_Triangulate(mode,count,[&](size_t a,size_t b,size_t c) {
            if(vertices.size()+3>limit) { flush(); vertices.clear(); }
            vertices.push_back(source[a]); vertices.push_back(source[b]); vertices.push_back(source[c]);
        });
    }
};

struct dx_index_span_t { size_t first=0,count=0; };
inline bool DX_AppendIndexSpan(std::vector<dx_index_span_t>& spans,size_t first,size_t count,size_t total) {
    if(count%3 || first>total || count>total-first) return false;
    if(!count) return true;
    if(!spans.empty() && spans.back().first+spans.back().count==first) spans.back().count+=count;
    else spans.push_back({first,count});
    return true;
}

// Keep source ranges alongside each chunk so failed uploads can draw the same
// chunk through the original index buffer without replaying successful chunks.
template<class Index,class Flush> void DX_StreamVisibleIndices(const Index* source,
    const std::vector<dx_index_span_t>& spans,size_t limit,Flush flush) {
    if(!source || limit<3 || limit>262144) return;
    limit-=limit%3;
    std::vector<Index> indices; indices.reserve(limit);
    std::vector<dx_index_span_t> ranges;
    for(const auto& span:spans) {
        size_t first=span.first,left=span.count;
        while(left) {
            const size_t take=std::min(left,limit-indices.size());
            indices.insert(indices.end(),source+first,source+first+take);
            ranges.push_back({first,take}); first+=take; left-=take;
            if(indices.size()==limit) { flush(indices,ranges); indices.clear(); ranges.clear(); }
        }
    }
    if(!indices.empty()) flush(indices,ranges);
}

inline bool DX_GlassFacesView(const float* n,float dist,const float* m,const float* eye) {
    if(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]<=1e-12f) return true;
    float point[3],normal[3];
    for(int i=0;i<3;++i) {
        point[i]=dist*(n[0]*m[i]+n[1]*m[4+i]+n[2]*m[8+i])+m[12+i];
        normal[i]=n[0]*m[i]+n[1]*m[4+i]+n[2]*m[8+i];
    }
    return normal[0]*(eye[0]-point[0])+normal[1]*(eye[1]-point[1])+normal[2]*(eye[2]-point[2])>.01f;
}
